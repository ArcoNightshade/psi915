/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos character-device entry points for /dev/dri/card* and
 * renderD* (docs/10, section 2), minor nodes, getinfo, deferred poll
 * wakeups, and module-level setup of the glue backend.
 *
 * Structure follows illumos/gfx-drm drm_sunmod.c (MIT licence; Oracle and
 * Intel copyright) in that one cb_ops serves every DRM minor and clone
 * opens identify the open file; no code is taken from it.
 *
 * UNTESTED ON ILLUMOS: compile-checked against drmglue/stubs only.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/model.h>
#include <sys/taskq.h>
#include <sys/atomic.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddidevmap.h>
#include <vm/as.h>

#include <linuxkpi/platform.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>
#include <drmglue/xlate.h>
#include <drmglue/illumos.h>

int drmglue_ioctl_abi = LKPI_IOC_ABI_ILLUMOS;
/* 0: refuse ioctls from 32-bit processes (docs/09 5.9); 1: compat_ioctl. */
int drmglue_allow_ilp32 = 0;
major_t drmglue_major = (major_t)-1;
dev_info_t *drmglue_host_dip;

static taskq_t *drmglue_poll_tq;
static struct lkpi_glue_backend drmglue_be;

/*
 * Poll heads. pollwakeup(9F) must not be called with locks the chpoll path
 * takes, and Linux wake-ups arrive under the wait-queue spinlock, possibly
 * from the interrupt handler, so the wakeup is handed to a taskq with a
 * preallocated entry (taskq_dispatch_ent cannot fail). Wake sources are
 * gone by the time ph_free runs (file.c removes the wait entries, fence
 * callbacks are removed in release), so at most an already-dispatched task
 * can still reference the head; it frees the head if ph_free ran first.
 */
struct drmglue_ph {
	struct pollhead	ph;
	taskq_ent_t	ent;
	kmutex_t	lock;
	volatile uint_t	queued;
	volatile uint_t	pending;	/* illumos POLL* bits */
	boolean_t	dying;
};

static void
drmglue_ph_destroy(struct drmglue_ph *p)
{
	pollhead_clean(&p->ph);
	lkpi_plat_mutex_destroy(&p->lock);
	kmem_free(p, sizeof (*p));
}

static void
drmglue_ph_task(void *arg)
{
	struct drmglue_ph *p = arg;
	boolean_t dying;
	uint_t ev;

	mutex_enter(&p->lock);
	p->queued = 0;
	membar_producer();
	ev = atomic_swap_uint(&p->pending, 0);
	dying = p->dying;
	if (!dying && ev != 0)
		pollwakeup(&p->ph, (short)ev);
	mutex_exit(&p->lock);
	if (dying)
		drmglue_ph_destroy(p);
}

static void *
drmglue_ph_alloc(struct lkpi_file *f)
{
	struct drmglue_ph *p = kmem_zalloc(sizeof (*p), KM_SLEEP);

	lkpi_plat_mutex_init(&p->lock, NULL);
	return (p);
}

static void
drmglue_ph_wake(void *ph, __poll_t ev)
{
	struct drmglue_ph *p = ph;

	atomic_or_uint(&p->pending, (uint_t)(unsigned short)lkpi_poll_to_sun(ev));
	if (atomic_cas_uint(&p->queued, 0, 1) == 0)
		taskq_dispatch_ent(drmglue_poll_tq, drmglue_ph_task, p, 0,
		    &p->ent);
}

static void
drmglue_ph_free(void *ph)
{
	struct drmglue_ph *p = ph;
	boolean_t queued;

	mutex_enter(&p->lock);
	p->dying = B_TRUE;
	queued = p->queued != 0;
	mutex_exit(&p->lock);
	if (!queued)
		drmglue_ph_destroy(p);
}

/*
 * Lookup
 */
struct lkpi_file *
drmglue_dev_file(dev_t dev)
{
	minor_t m = getminor(dev);
	unsigned int node = lkpi_minor_node((uint32_t)m);

	if (node == LKPI_NODE_ANON)
		return (lkpi_anon_mmap_file(lkpi_minor_clone((uint32_t)m)));
	return (lkpi_cdev_file(node, lkpi_minor_clone((uint32_t)m)));
}

static dev_info_t *
drmglue_dev_dip(dev_t dev)
{
	unsigned int node = lkpi_minor_node((uint32_t)getminor(dev));

	if (node == LKPI_NODE_ANON)
		return (drmglue_host_dip);
	return (lkpi_node_plat(node));
}

/*
 * Shared pieces of the cb_ops and anonymous-vnode entry points.
 */
int
drmglue_uio_read(struct lkpi_file *f, struct uio *uio)
{
	boolean_t nonblock = (uio->uio_fmode & (FNONBLOCK | FNDELAY)) != 0;
	struct task_struct *t = current;
	boolean_t saved = t->lkpi_uaccess_kernel;
	size_t got = 0, done;
	struct iovec *iov;
	int r = 0;

	if (uio->uio_segflg == UIO_SYSSPACE)
		t->lkpi_uaccess_kernel = true;
	else if (uio->uio_segflg != UIO_USERSPACE)
		return (EINVAL);
	while (uio->uio_resid > 0 && uio->uio_iovcnt > 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		/*
		 * Linux reads deliver whole records (DRM events); after the
		 * first bytes, never block for the next iovec.
		 */
		r = lkpi_file_read(f, iov->iov_base, iov->iov_len,
		    nonblock || got > 0, &done);
		if (r != 0) {
			if (got > 0)
				r = 0;
			break;
		}
		iov->iov_base += done;
		iov->iov_len -= done;
		uio->uio_resid -= done;
		uio->uio_loffset += done;
		got += done;
		if (done == 0 || iov->iov_len != 0)
			break;
	}
	t->lkpi_uaccess_kernel = saved;
	return (r);
}

int
drmglue_file_ioctl(struct lkpi_file *f, int cmd, intptr_t arg, int mode,
    uint32_t (*kcmd)(struct lkpi_file *, unsigned int, unsigned int),
    int *rvalp)
{
	struct lkpi_ioctl_ctx ctx;
	long rv = 0;
	int r;

	ctx.abi = (enum lkpi_ioc_abi)drmglue_ioctl_abi;
	ctx.ilp32 = ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32;
	ctx.refuse_ilp32 = drmglue_allow_ilp32 == 0;
	ctx.kernel = (mode & FKIOCTL) != 0;
	ctx.nonblock = (mode & (FNONBLOCK | FNDELAY)) != 0;
	ctx.kcmd = kcmd;
	r = lkpi_file_ioctl(f, (uint32_t)cmd, (unsigned long)arg, &ctx, &rv);
	*rvalp = (int)rv;
	return (r);
}

int
drmglue_file_chpoll(struct lkpi_file *f, short events, int anyyet,
    short *reventsp, struct pollhead **phpp)
{
	__poll_t rev;
	void *ph;
	int r;

	r = lkpi_file_poll(f, lkpi_poll_from_sun(events), &rev, &ph);
	if (r != 0)
		return (r);
	*reventsp = lkpi_poll_to_sun(rev);
	/* chpoll(9E): return the head when nothing is ready, or for POLLET. */
	if ((*reventsp == 0 && !anyyet) || (events & POLLET))
		*phpp = &((struct drmglue_ph *)ph)->ph;
	return (0);
}

/*
 * cb_ops
 */
static int
drmglue_open(dev_t *devp, int flag, int otyp, cred_t *cr)
{
	minor_t m = getminor(*devp);
	unsigned int node = lkpi_minor_node((uint32_t)m), oflags;
	fmode_t mode = 0;
	uint32_t clone;
	int r;

	if (otyp != OTYP_CHR)
		return (EINVAL);
	if (lkpi_minor_clone((uint32_t)m) != 0 || node == LKPI_NODE_ANON)
		return (ENXIO);
	if (flag & FREAD)
		mode |= FMODE_READ;
	if (flag & FWRITE)
		mode |= FMODE_WRITE;
	oflags = (mode == (FMODE_READ | FMODE_WRITE)) ? O_RDWR :
	    (mode & FMODE_WRITE) ? O_WRONLY : O_RDONLY;
	if (flag & (FNONBLOCK | FNDELAY))
		oflags |= O_NONBLOCK;

	r = lkpi_cdev_open(node, oflags, mode, &clone);
	if (r != 0)
		return (r);
	*devp = makedevice(getmajor(*devp),
	    (minor_t)lkpi_minor_make(node, clone));
	return (0);
}

static int
drmglue_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	minor_t m = getminor(dev);
	unsigned int node = lkpi_minor_node((uint32_t)m);

	/* The anon mmap host minor is "closed" by spec_delmap; no state. */
	if (node == LKPI_NODE_ANON)
		return (0);
	return (lkpi_cdev_close(node, lkpi_minor_clone((uint32_t)m)));
}

static int
drmglue_read(dev_t dev, struct uio *uio, cred_t *cr)
{
	struct lkpi_file *f = drmglue_dev_file(dev);
	int r;

	if (f == NULL)
		return (ENXIO);
	r = drmglue_uio_read(f, uio);
	fput(f);
	return (r);
}

static int
drmglue_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr,
    int *rvalp)
{
	unsigned int node = lkpi_minor_node((uint32_t)getminor(dev));
	struct lkpi_file *f = drmglue_dev_file(dev);
	int r;

	if (f == NULL)
		return (ENXIO);
	r = drmglue_file_ioctl(f, cmd, arg, mode, lkpi_node_kcmd(node), rvalp);
	fput(f);
	return (r);
}

static int
drmglue_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	struct lkpi_file *f = drmglue_dev_file(dev);
	int r;

	if (f == NULL)
		return (ENXIO);
	r = drmglue_file_chpoll(f, events, anyyet, reventsp, phpp);
	fput(f);
	return (r);
}

static int
drmglue_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp,
    off_t len, uint_t prot, uint_t maxprot, uint_t flags, cred_t *cr)
{
	struct lkpi_file *f = drmglue_dev_file(dev);
	int r;

	if (f == NULL)
		return (ENXIO);
	r = drmglue_segmap_file(f, dev, (offset_t)off, as, addrp, (size_t)len,
	    prot, maxprot, flags, cr);
	fput(f);
	return (r);
}

static int
drmglue_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
    size_t *maplen, uint_t model)
{
	struct lkpi_file *f = drmglue_dev_file(dev);
	dev_info_t *dip = drmglue_dev_dip(dev);
	int r;

	if (f == NULL || dip == NULL) {
		if (f != NULL)
			fput(f);
		return (ENXIO);
	}
	r = drmglue_devmap_file(f, dip, dhp, off, len, maplen);
	fput(f);
	return (r);
}

struct cb_ops drmglue_cb_ops = {
	.cb_open = drmglue_open,
	.cb_close = drmglue_close,
	.cb_strategy = nodev,
	.cb_print = nodev,
	.cb_dump = nodev,
	.cb_read = drmglue_read,
	.cb_write = nodev,
	.cb_ioctl = drmglue_ioctl,
	.cb_devmap = drmglue_devmap,
	.cb_mmap = nodev,
	.cb_segmap = drmglue_segmap,
	.cb_chpoll = drmglue_chpoll,
	.cb_prop_op = ddi_prop_op,
	.cb_str = NULL,
	.cb_flag = D_NEW | D_MP | D_DEVMAP | D_64BIT,
	.cb_rev = CB_REV,
	.cb_aread = nodev,
	.cb_awrite = nodev
};

int
drmglue_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **result)
{
	dev_info_t *d = drmglue_dev_dip((dev_t)(uintptr_t)arg);

	if (d == NULL)
		return (DDI_FAILURE);
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = d;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(uintptr_t)ddi_get_instance(d);
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

/*
 * Minor nodes. Node type DDI_NT_DISPLAY_DRM ("ddi_display:drm"); the
 * /dev/dri links come from a devlink.tab entry (docs/10, section 2.2).
 */
int
drmglue_attach_node(dev_info_t *dip, unsigned int node, const char *name,
    const struct file_operations *fops,
    uint32_t (*kcmd)(struct lkpi_file *, unsigned int, unsigned int))
{
	struct lkpi_node_desc d;
	int r;

	d.node = node;
	d.name = name;
	d.fops = fops;
	d.kcmd = kcmd;
	d.plat = dip;
	r = lkpi_node_register(&d);
	if (r != 0)
		return (lkpi_errno_to_user(r));
	if (ddi_create_minor_node(dip, (char *)name, S_IFCHR,
	    (minor_t)lkpi_minor_make(node, 0), DDI_NT_DISPLAY_DRM, 0) !=
	    DDI_SUCCESS) {
		(void) lkpi_node_unregister(node);
		return (ENXIO);
	}
	if (drmglue_host_dip == NULL) {
		drmglue_host_dip = dip;
		drmglue_major = ddi_driver_major(dip);
	}
	return (0);
}

int
drmglue_detach_node(dev_info_t *dip, unsigned int node, const char *name)
{
	int r = lkpi_node_unregister(node);

	if (r != 0)
		return (lkpi_errno_to_user(r));
	ddi_remove_minor_node(dip, (char *)name);
	/*
	 * The host stays the first dip until every DRM device detaches; anon
	 * mmaps outliving it keep the major's dev_ops referenced through
	 * their spec vnodes, so the module cannot unload under them.
	 */
	return (0);
}

int
drmglue_init(void)
{
	int r;

	drmglue_poll_tq = taskq_create("drmglue_poll", 1, minclsyspri, 4,
	    INT_MAX, TASKQ_PREPOPULATE);
	if (drmglue_poll_tq == NULL)
		return (ENOMEM);
	drmglue_seg_be_fill(&drmglue_be);
	drmglue_anon_be_fill(&drmglue_be);
	drmglue_be.ph_alloc = drmglue_ph_alloc;
	drmglue_be.ph_free = drmglue_ph_free;
	drmglue_be.ph_wake = drmglue_ph_wake;
	lkpi_glue_set_backend(&drmglue_be);
	r = drmglue_anon_init();
	if (r != 0) {
		taskq_destroy(drmglue_poll_tq);
		return (r);
	}
	return (0);
}

void
drmglue_fini(void)
{
	drmglue_anon_fini();
	taskq_wait(drmglue_poll_tq);
	taskq_destroy(drmglue_poll_tq);
	drmglue_host_dip = NULL;
}
