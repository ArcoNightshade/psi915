/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * dma-buf core over glue anonymous files (docs/10, section 4.3).
 * Clean-room from the documented Linux API; see <linux/dma-buf.h>.
 *
 * Simplifications against Linux 6.x, all listed in docs/10, section 4.3:
 *   - attachment list and vmap counter are protected by db->lock, not by
 *     the reservation lock (dma_resv_assert_held checks are not made);
 *   - poll() reports ready immediately (no implicit-fence wait);
 *   - DMA_BUF_IOCTL_EXPORT_SYNC_FILE / IMPORT_SYNC_FILE return ENOTTY;
 *   - no dynamic-importer move_notify, no debugfs/sysfs statistics.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <drmglue/anonfile.h>
#include <drmglue/xlate.h>

/* uAPI (include/uapi/linux/dma-buf.h values, Linux encoding). */
#define	DMA_BUF_SYNC_READ		(1 << 0)
#define	DMA_BUF_SYNC_WRITE		(2 << 0)
#define	DMA_BUF_SYNC_RW			(DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define	DMA_BUF_SYNC_END		(1 << 2)
#define	DMA_BUF_SYNC_VALID_FLAGS_MASK	(DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END)
#define	LKPI_DMA_BUF_BASE		'b'
#define	LKPI_DMA_BUF_IOCTL_SYNC		LKPI_IOC(LKPI_IOC_WRITE, 'b', 0, 8)
#define	LKPI_DMA_BUF_SET_NAME_A		LKPI_IOC(LKPI_IOC_WRITE, 'b', 1, 4)
#define	LKPI_DMA_BUF_SET_NAME_B		LKPI_IOC(LKPI_IOC_WRITE, 'b', 1, 8)

static int
lkpi_dmabuf_copyin(void *k, unsigned long uaddr, size_t len)
{
	return (lkpi_plat_copyin((const void *)uaddr, k, len,
	    current->lkpi_uaccess_kernel) == 0 ? 0 : -EFAULT);
}

static int
dma_buf_file_release(struct inode *ip, struct lkpi_file *f)
{
	struct dma_buf *db = f->private_data;

	WARN_ON(!list_empty(&db->attachments));
	WARN_ON(db->vmapping_counter != 0);
	db->ops->release(db);
	if (db->lkpi_own_resv)
		lkpi_dma_resv_free(db->resv);
	kfree(db->name);
	mutex_destroy(&db->lock);
	kfree(db);
	return (0);
}

static int
dma_buf_file_mmap(struct lkpi_file *f, struct vm_area_struct *vma)
{
	struct dma_buf *db = f->private_data;

	if (db->ops->mmap == NULL)
		return (-EINVAL);
	if (vma->vm_pgoff + vma_pages(vma) < vma->vm_pgoff ||
	    vma->vm_pgoff + vma_pages(vma) > PAGE_ALIGN(db->size) >>
	    PAGE_SHIFT)
		return (-EINVAL);
	return (db->ops->mmap(db, vma));
}

static loff_t
dma_buf_file_llseek(struct lkpi_file *f, loff_t off, int whence)
{
	struct dma_buf *db = f->private_data;

	if (off != 0)
		return (-EINVAL);
	if (whence == 2 /* SEEK_END */)
		return ((loff_t)db->size);
	if (whence == 0 /* SEEK_SET */)
		return (0);
	return (-EINVAL);
}

static __poll_t
dma_buf_file_poll(struct lkpi_file *f, poll_table *pt)
{
	/* Implicit-fence polling not implemented (docs/10, gap G3). */
	return ((LKPI_EPOLLIN | LKPI_EPOLLOUT) & poll_requested_events(pt));
}

static long
dma_buf_file_ioctl(struct lkpi_file *f, unsigned int cmd, unsigned long arg)
{
	struct dma_buf *db = f->private_data;
	enum dma_data_direction dir;
	uint64_t flags;
	char *name, *old;
	long n;
	int r;

	switch (cmd) {
	case LKPI_DMA_BUF_IOCTL_SYNC:
		r = lkpi_dmabuf_copyin(&flags, arg, sizeof (flags));
		if (r != 0)
			return (r);
		if (flags & ~(uint64_t)DMA_BUF_SYNC_VALID_FLAGS_MASK)
			return (-EINVAL);
		switch (flags & DMA_BUF_SYNC_RW) {
		case DMA_BUF_SYNC_READ:
			dir = DMA_FROM_DEVICE;
			break;
		case DMA_BUF_SYNC_WRITE:
			dir = DMA_TO_DEVICE;
			break;
		case DMA_BUF_SYNC_RW:
			dir = DMA_BIDIRECTIONAL;
			break;
		default:
			return (-EINVAL);
		}
		if (flags & DMA_BUF_SYNC_END)
			return (dma_buf_end_cpu_access(db, dir));
		return (dma_buf_begin_cpu_access(db, dir));

	case LKPI_DMA_BUF_SET_NAME_A:
	case LKPI_DMA_BUF_SET_NAME_B:
		name = kzalloc(DMA_BUF_NAME_LEN, GFP_KERNEL);
		if (name == NULL)
			return (-ENOMEM);
		n = lkpi_plat_copyinstr((const char *)arg, name,
		    DMA_BUF_NAME_LEN, current->lkpi_uaccess_kernel);
		if (n < 0) {
			kfree(name);
			return (n);
		}
		spin_lock(&db->name_lock);
		old = (char *)db->name;
		db->name = name;
		spin_unlock(&db->name_lock);
		kfree(old);
		return (0);

	default:
		return (-ENOTTY);
	}
}

static const struct file_operations dma_buf_fops = {
	.release = dma_buf_file_release,
	.mmap = dma_buf_file_mmap,
	.llseek = dma_buf_file_llseek,
	.poll = dma_buf_file_poll,
	.unlocked_ioctl = dma_buf_file_ioctl,
	.compat_ioctl = dma_buf_file_ioctl,	/* layouts are 32/64 clean */
};

bool
lkpi_is_dma_buf_file(struct lkpi_file *f)
{
	return (f != NULL && f->f_op == &dma_buf_fops);
}

struct dma_buf *
dma_buf_export(const struct dma_buf_export_info *exp)
{
	struct dma_buf *db;
	struct lkpi_file *f;

	if (exp == NULL || exp->ops == NULL || exp->size == 0 ||
	    exp->ops->map_dma_buf == NULL || exp->ops->unmap_dma_buf == NULL ||
	    exp->ops->release == NULL ||
	    (exp->ops->pin == NULL) != (exp->ops->unpin == NULL))
		return (ERR_PTR(-EINVAL));

	db = kzalloc(sizeof (*db), GFP_KERNEL);
	if (db == NULL)
		return (ERR_PTR(-ENOMEM));
	db->size = exp->size;
	db->ops = exp->ops;
	db->priv = exp->priv;
	db->exp_name = exp->exp_name;
	db->owner = exp->owner;
	INIT_LIST_HEAD(&db->attachments);
	mutex_init(&db->lock);
	spin_lock_init(&db->name_lock);
	db->resv = exp->resv;
	if (db->resv == NULL) {
		db->resv = lkpi_dma_resv_alloc();
		if (db->resv == NULL)
			goto nomem;
		db->lkpi_own_resv = true;
	}

	f = anon_inode_getfile("dmabuf", &dma_buf_fops, db,
	    (exp->flags & O_ACCMODE) | (exp->flags & O_NONBLOCK));
	if (IS_ERR(f)) {
		if (db->lkpi_own_resv)
			lkpi_dma_resv_free(db->resv);
		mutex_destroy(&db->lock);
		kfree(db);
		return (ERR_CAST(f));
	}
	i_size_write(file_inode(f), (loff_t)db->size);
	db->file = f;
	return (db);
nomem:
	mutex_destroy(&db->lock);
	kfree(db);
	return (ERR_PTR(-ENOMEM));
}

int
dma_buf_fd(struct dma_buf *db, int flags)
{
	int fd;

	if (db == NULL || db->file == NULL)
		return (-EINVAL);
	fd = get_unused_fd_flags((unsigned int)flags);
	if (fd < 0)
		return (fd);
	/* Consumes the caller's reference, as in Linux. */
	fd_install((unsigned int)fd, db->file);
	return (fd);
}

struct dma_buf *
dma_buf_get(int fd)
{
	struct lkpi_file *f;

	if (fd < 0)
		return (ERR_PTR(-EBADF));
	f = fget((unsigned int)fd);
	if (f == NULL)
		return (ERR_PTR(-EBADF));
	if (!lkpi_is_dma_buf_file(f)) {
		fput(f);
		return (ERR_PTR(-EINVAL));
	}
	return (f->private_data);
}

void
dma_buf_put(struct dma_buf *db)
{
	if (WARN_ON(db == NULL || db->file == NULL))
		return;
	fput(db->file);
}

struct dma_buf_attachment *
dma_buf_dynamic_attach(struct dma_buf *db, struct device *dev,
    const struct dma_buf_attach_ops *ops, void *priv)
{
	struct dma_buf_attachment *a;
	int r;

	if (db == NULL || dev == NULL)
		return (ERR_PTR(-EINVAL));
	a = kzalloc(sizeof (*a), GFP_KERNEL);
	if (a == NULL)
		return (ERR_PTR(-ENOMEM));
	a->dmabuf = db;
	a->dev = dev;
	a->importer_ops = ops;
	a->importer_priv = priv;
	a->dir = DMA_NONE;
	if (ops != NULL)
		a->peer2peer = ops->allow_peer2peer;
	if (db->ops->attach != NULL) {
		r = db->ops->attach(db, a);
		if (r != 0) {
			kfree(a);
			return (ERR_PTR(r));
		}
	}
	mutex_lock(&db->lock);
	list_add(&a->node, &db->attachments);
	mutex_unlock(&db->lock);
	return (a);
}

struct dma_buf_attachment *
dma_buf_attach(struct dma_buf *db, struct device *dev)
{
	return (dma_buf_dynamic_attach(db, dev, NULL, NULL));
}

void
dma_buf_detach(struct dma_buf *db, struct dma_buf_attachment *a)
{
	if (db == NULL || a == NULL || WARN_ON(a->dmabuf != db))
		return;
	if (a->sgt != NULL) {
		db->ops->unmap_dma_buf(a, a->sgt, a->dir);
		a->sgt = NULL;
	}
	mutex_lock(&db->lock);
	list_del(&a->node);
	mutex_unlock(&db->lock);
	if (db->ops->detach != NULL)
		db->ops->detach(db, a);
	kfree(a);
}

int
dma_buf_pin(struct dma_buf_attachment *a)
{
	struct dma_buf *db = a->dmabuf;

	return (db->ops->pin != NULL ? db->ops->pin(a) : 0);
}

void
dma_buf_unpin(struct dma_buf_attachment *a)
{
	struct dma_buf *db = a->dmabuf;

	if (db->ops->unpin != NULL)
		db->ops->unpin(a);
}

struct sg_table *
dma_buf_map_attachment(struct dma_buf_attachment *a,
    enum dma_data_direction dir)
{
	struct dma_buf *db;
	struct sg_table *sgt;

	if (a == NULL || a->dmabuf == NULL)
		return (ERR_PTR(-EINVAL));
	db = a->dmabuf;
	/* cache_sgt_mapping: one mapping per attachment, reused. */
	if (a->sgt != NULL) {
		if (a->dir != dir && a->dir != DMA_BIDIRECTIONAL)
			return (ERR_PTR(-EBUSY));
		return (a->sgt);
	}
	sgt = db->ops->map_dma_buf(a, dir);
	if (sgt == NULL)
		return (ERR_PTR(-ENOMEM));
	if (IS_ERR(sgt))
		return (sgt);
	if (db->ops->cache_sgt_mapping) {
		a->sgt = sgt;
		a->dir = dir;
	}
	return (sgt);
}

void
dma_buf_unmap_attachment(struct dma_buf_attachment *a, struct sg_table *sgt,
    enum dma_data_direction dir)
{
	if (a == NULL || a->dmabuf == NULL || sgt == NULL)
		return;
	if (a->sgt == sgt)
		return;		/* cached: released at detach */
	a->dmabuf->ops->unmap_dma_buf(a, sgt, dir);
}

int
dma_buf_begin_cpu_access(struct dma_buf *db, enum dma_data_direction d)
{
	return (db->ops->begin_cpu_access != NULL ?
	    db->ops->begin_cpu_access(db, d) : 0);
}

int
dma_buf_end_cpu_access(struct dma_buf *db, enum dma_data_direction d)
{
	return (db->ops->end_cpu_access != NULL ?
	    db->ops->end_cpu_access(db, d) : 0);
}

int
dma_buf_mmap(struct dma_buf *db, struct vm_area_struct *vma,
    unsigned long pgoff)
{
	if (db == NULL || vma == NULL || db->ops->mmap == NULL)
		return (-EINVAL);
	if (pgoff + vma_pages(vma) < pgoff ||
	    pgoff + vma_pages(vma) > PAGE_ALIGN(db->size) >> PAGE_SHIFT)
		return (-EINVAL);
	vma_set_file(vma, db->file);
	vma->vm_pgoff = pgoff;
	return (db->ops->mmap(db, vma));
}

int
dma_buf_vmap(struct dma_buf *db, struct iosys_map *map)
{
	int r = 0;

	if (db == NULL || db->ops->vmap == NULL)
		return (-EINVAL);
	/*
	 * Linux caches the first mapping and returns it to later callers;
	 * that needs struct iosys_map's layout (KPI). Every call goes to the
	 * exporter here, which i915 and DRM shmem exporters handle.
	 */
	mutex_lock(&db->lock);
	r = db->ops->vmap(db, map);
	if (r == 0)
		db->vmapping_counter++;
	mutex_unlock(&db->lock);
	return (r);
}

void
dma_buf_vunmap(struct dma_buf *db, struct iosys_map *map)
{
	if (db == NULL || db->ops->vunmap == NULL)
		return;
	mutex_lock(&db->lock);
	if (!WARN_ON(db->vmapping_counter == 0)) {
		db->vmapping_counter--;
		db->ops->vunmap(db, map);
	}
	mutex_unlock(&db->lock);
}
