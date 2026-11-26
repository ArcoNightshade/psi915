/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Anonymous files backed by illumos file descriptors: anon_inode_getfile,
 * get_unused_fd_flags, put_unused_fd, fd_install, fget (docs/10, section
 * 4.2). Clean-room from the documented Linux API.
 *
 * Reference structure (one Linux file <-> at most one vnode at a time):
 *
 *   fd --> file_t --(hold)--> vnode --(one Linux ref)--> struct lkpi_file
 *                                                          ^
 *   kernel users (dma_buf_get, VMAs, drm_prime caches) ----+ (own refs)
 *
 * fd_install of a file that already has a vnode takes another vnode hold
 * and drops the caller's Linux reference, so N descriptors installed for
 * one file (drm_gem_prime_handle_to_fd exports the cached dma-buf again)
 * cost one Linux reference in total. When the vnode becomes inactive it
 * drops that reference; f_op->release runs when the last kernel user is
 * gone too. A concurrent fd_install and vnode inactive are serialised by
 * f->f_lkpi_lock: inactive re-checks the vnode count under it.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <drmglue/lkpi_fs.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>
#include <drmglue/anonfile.h>

#define	LKPI_ANON_MODE	0100600		/* S_IFREG | 0600 */

struct lkpi_file *
anon_inode_getfile(const char *name, const struct file_operations *fops,
    void *priv, int flags)
{
	struct inode *ip;
	struct lkpi_file *f;
	fmode_t mode = 0;

	ip = lkpi_inode_alloc(0, LKPI_ANON_MODE);
	if (ip == NULL)
		return (ERR_PTR(-ENOMEM));
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		mode = FMODE_READ;
		break;
	case O_WRONLY:
		mode = FMODE_WRITE;
		break;
	default:
		mode = FMODE_READ | FMODE_WRITE;
		break;
	}
	f = lkpi_file_alloc(LKPI_FILE_ANON, fops, ip,
	    (unsigned int)flags & ~(unsigned int)O_CLOEXEC, mode);
	if (f == NULL) {
		lkpi_inode_put(ip);
		return (ERR_PTR(-ENOMEM));
	}
	f->f_lkpi_own_inode = ip;
	f->private_data = priv;
	return (f);
}

/* anon_inode_getfile() installed in a new descriptor (KPI R1/R3). */
int
anon_inode_getfd(const char *name, const struct file_operations *fops,
    void *priv, int flags)
{
	struct lkpi_file *f;
	int fd;

	fd = get_unused_fd_flags((unsigned int)flags);
	if (fd < 0)
		return (fd);
	f = anon_inode_getfile(name, fops, priv, flags);
	if (IS_ERR(f)) {
		put_unused_fd((unsigned int)fd);
		return ((int)PTR_ERR(f));
	}
	fd_install((unsigned int)fd, f);
	return (fd);
}

/*
 * Pending reservations: Linux takes O_CLOEXEC at get_unused_fd_flags()
 * time, illumos sets it on an installed descriptor. Remember the flags of
 * a reservation until fd_install() in the same process.
 */
#define	LKPI_FD_PENDING	32

static struct {
	bool		used;
	int		fd;
	pid_t		owner;		/* tgid of the reserving process */
	unsigned int	flags;
} lkpi_fd_pending[LKPI_FD_PENDING];
static DEFINE_SPINLOCK(lkpi_fd_pending_lock);

static pid_t
lkpi_fd_owner(void)
{
	return (current->tgid);
}

static void
lkpi_fd_pending_put(int fd, unsigned int flags)
{
	int i;

	spin_lock(&lkpi_fd_pending_lock);
	for (i = 0; i < LKPI_FD_PENDING; i++) {
		if (!lkpi_fd_pending[i].used) {
			lkpi_fd_pending[i].used = true;
			lkpi_fd_pending[i].fd = fd;
			lkpi_fd_pending[i].owner = lkpi_fd_owner();
			lkpi_fd_pending[i].flags = flags;
			break;
		}
	}
	spin_unlock(&lkpi_fd_pending_lock);
	WARN_ONCE(i == LKPI_FD_PENDING && (flags & O_CLOEXEC),
	    "drmglue: fd reservation table full, O_CLOEXEC lost");
}

static unsigned int
lkpi_fd_pending_take(int fd)
{
	unsigned int flags = 0;
	pid_t me = lkpi_fd_owner();
	int i;

	spin_lock(&lkpi_fd_pending_lock);
	for (i = 0; i < LKPI_FD_PENDING; i++) {
		if (lkpi_fd_pending[i].used && lkpi_fd_pending[i].owner == me &&
		    lkpi_fd_pending[i].fd == fd) {
			flags = lkpi_fd_pending[i].flags;
			lkpi_fd_pending[i].used = false;
			break;
		}
	}
	spin_unlock(&lkpi_fd_pending_lock);
	return (flags);
}

int
get_unused_fd_flags(unsigned int flags)
{
	int fd = lkpi_glue_be->fd_reserve(flags);

	if (fd >= 0)
		lkpi_fd_pending_put(fd, flags);
	return (fd);
}

void
put_unused_fd(unsigned int fd)
{
	(void) lkpi_fd_pending_take((int)fd);
	lkpi_glue_be->fd_unreserve((int)fd);
}

void
fd_install(unsigned int fd, struct lkpi_file *f)
{
	unsigned int rflags = lkpi_fd_pending_take((int)fd);
	bool extra_ref = false;
	void *vn;

	mutex_lock(&f->f_lkpi_lock);
	vn = f->f_lkpi_vnode;
	if (vn != NULL) {
		lkpi_glue_be->vn_hold(vn);
		extra_ref = true;
	} else {
		/* The caller's reference moves to the new vnode. */
		vn = lkpi_glue_be->vn_alloc(f);
		BUG_ON(vn == NULL);
		f->f_lkpi_vnode = vn;
	}
	mutex_unlock(&f->f_lkpi_lock);
	lkpi_glue_be->fd_install((int)fd, vn,
	    (f->f_flags & ~(unsigned int)O_CLOEXEC) | (rflags & O_CLOEXEC));
	if (extra_ref)
		fput(f);	/* never the last: the vnode holds one */
}

struct lkpi_file *
fget(unsigned int fd)
{
	return (lkpi_glue_be->fd_get((int)fd));
}

void
lkpi_anon_vnode_inactive(struct lkpi_file *f, void *vn)
{
	mutex_lock(&f->f_lkpi_lock);
	if (!lkpi_glue_be->vn_rele_last(vn)) {
		mutex_unlock(&f->f_lkpi_lock);
		return;
	}
	WARN_ON(f->f_lkpi_vnode != vn);
	f->f_lkpi_vnode = NULL;
	mutex_unlock(&f->f_lkpi_lock);
	lkpi_glue_be->vn_free(vn);
	fput(f);
}

/*
 * mmap host ids (glue.h).
 */
static DEFINE_MUTEX(lkpi_anon_mmap_lock);
static struct idr lkpi_anon_mmap_idr = IDR_INIT(lkpi_anon_mmap_idr);

int
lkpi_anon_mmap_id(struct lkpi_file *f, uint32_t *id)
{
	int r = 0;

	mutex_lock(&lkpi_anon_mmap_lock);
	if (f->f_lkpi_mmap_id == 0) {
		r = idr_alloc(&lkpi_anon_mmap_idr, f, 1,
		    (int)LKPI_CLONE_MAX + 1, GFP_KERNEL);
		if (r > 0) {
			f->f_lkpi_mmap_id = (uint32_t)r;
			r = 0;
		}
	}
	*id = f->f_lkpi_mmap_id;
	mutex_unlock(&lkpi_anon_mmap_lock);
	return (r < 0 ? lkpi_errno_to_user(r) : 0);
}

struct lkpi_file *
lkpi_anon_mmap_file(uint32_t id)
{
	struct lkpi_file *f;

	mutex_lock(&lkpi_anon_mmap_lock);
	f = idr_find(&lkpi_anon_mmap_idr, id);
	f = get_file_active(&f);
	mutex_unlock(&lkpi_anon_mmap_lock);
	return (f);
}

void lkpi_anon_file_released(struct lkpi_file *f);

void
lkpi_anon_file_released(struct lkpi_file *f)
{
	if (f->f_lkpi_mmap_id == 0)
		return;
	mutex_lock(&lkpi_anon_mmap_lock);
	(void) idr_remove(&lkpi_anon_mmap_idr, f->f_lkpi_mmap_id);
	f->f_lkpi_mmap_id = 0;
	mutex_unlock(&lkpi_anon_mmap_lock);
}
