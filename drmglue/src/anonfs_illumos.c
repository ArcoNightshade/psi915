/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Pseudo vnodes for Linux anonymous files (dma-buf, sync_file, syncobj
 * files): vnodeops, a placeholder vfs, and the descriptor operations of
 * the glue backend (docs/10, section 4.1).
 *
 * A dma-buf fd is an ordinary file_t on one of these vnodes, so SCM_RIGHTS
 * passing, dup, fork and close need nothing special. VOP_MAP builds a
 * segdev segment with devmap_setup() on a hidden minor of the DRM driver's
 * major ("anon mmap host", node 0xff, id = the file's mmap id): segdev
 * needs a character dev_t whose cb_devmap creates the mapping, and an
 * existing snode for it (segdev_create calls specfind()), which is made
 * with makespecvp() on first mmap and held until the file is freed.
 *
 * UNTESTED ON ILLUMOS: compile-checked against drmglue/stubs only.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddidevmap.h>
#include <vm/as.h>

#include <linuxkpi/platform.h>
#include <linux/kernel.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>
#include <drmglue/illumos.h>

static vnodeops_t *drmglue_anon_vnodeops;
static vfsops_t *drmglue_anon_vfsops;
static vfs_t *drmglue_anon_vfsp;

#define	VTOF(vp)	((struct lkpi_file *)(vp)->v_data)

static int
drmglue_anon_vop_open(vnode_t **vpp, int flag, cred_t *cr,
    caller_context_t *ct)
{
	return (0);
}

static int
drmglue_anon_vop_close(vnode_t *vp, int flag, int count, offset_t off,
    cred_t *cr, caller_context_t *ct)
{
	/* Release happens in VOP_INACTIVE (last vnode hold). */
	return (0);
}

static int
drmglue_anon_vop_read(vnode_t *vp, uio_t *uio, int ioflag, cred_t *cr,
    caller_context_t *ct)
{
	return (drmglue_uio_read(VTOF(vp), uio));
}

static int
drmglue_anon_vop_write(vnode_t *vp, uio_t *uio, int ioflag, cred_t *cr,
    caller_context_t *ct)
{
	return (EINVAL);	/* Linux: no f_op->write */
}

static int
drmglue_anon_vop_ioctl(vnode_t *vp, int cmd, intptr_t arg, int flag,
    cred_t *cr, int *rvalp, caller_context_t *ct)
{
	return (drmglue_file_ioctl(VTOF(vp), cmd, arg, flag, NULL, rvalp));
}

static int
drmglue_anon_vop_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr,
    caller_context_t *ct)
{
	struct lkpi_file *f = VTOF(vp);

	bzero(vap, sizeof (*vap));
	vap->va_mask = AT_ALL;
	vap->va_type = VREG;
	vap->va_mode = 0600;
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = drmglue_anon_vfsp->vfs_dev;
	vap->va_nodeid = (ino64_t)((uintptr_t)f >> 4);
	vap->va_nlink = 1;
	/* lseek(fd, 0, SEEK_END) on a dma-buf returns its size. */
	vap->va_size = (u_offset_t)f->f_inode->i_size;
	vap->va_blksize = PAGESIZE;
	return (0);
}

static int
drmglue_anon_vop_access(vnode_t *vp, int mode, int flags, cred_t *cr,
    caller_context_t *ct)
{
	return (0);
}

static void
drmglue_anon_vop_inactive(vnode_t *vp, cred_t *cr, caller_context_t *ct)
{
	lkpi_anon_vnode_inactive(VTOF(vp), vp);
}

static int
drmglue_anon_vop_seek(vnode_t *vp, offset_t ooff, offset_t *noffp,
    caller_context_t *ct)
{
	return (*noffp < 0 ? EINVAL : 0);
}

static int
drmglue_anon_vop_map(vnode_t *vp, offset_t off, struct as *as,
    caddr_t *addrp, size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    cred_t *cr, caller_context_t *ct)
{
	struct lkpi_file *f = VTOF(vp);
	vnode_t *svp;
	uint32_t id;
	dev_t dev;
	int r;

	if (f->f_op->mmap == NULL || drmglue_major == (major_t)-1)
		return (ENODEV);
	r = lkpi_anon_mmap_id(f, &id);
	if (r != 0)
		return (r);
	dev = makedevice(drmglue_major,
	    (minor_t)lkpi_minor_make(LKPI_NODE_ANON, id));

	mutex_lock(&f->f_lkpi_lock);
	if (f->f_lkpi_mmap_vp == NULL) {
		svp = makespecvp(dev, VCHR);	/* held; found by specfind */
		f->f_lkpi_mmap_vp = svp;
	}
	mutex_unlock(&f->f_lkpi_lock);

	return (drmglue_segmap_file(f, dev, off, as, addrp, len, prot, maxprot,
	    flags, cr));
}

static int
drmglue_anon_vop_addmap(vnode_t *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    cred_t *cr, caller_context_t *ct)
{
	return (0);	/* segdev accounts on the host snode */
}

static int
drmglue_anon_vop_delmap(vnode_t *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uint_t prot, uint_t maxprot, uint_t flags,
    cred_t *cr, caller_context_t *ct)
{
	return (0);
}

static int
drmglue_anon_vop_poll(vnode_t *vp, short events, int anyyet, short *reventsp,
    struct pollhead **phpp, caller_context_t *ct)
{
	return (drmglue_file_chpoll(VTOF(vp), events, anyyet, reventsp, phpp));
}

static const fs_operation_def_t drmglue_anon_vnodeops_template[] = {
	{ VOPNAME_OPEN,		{ .vop_open = drmglue_anon_vop_open } },
	{ VOPNAME_CLOSE,	{ .vop_close = drmglue_anon_vop_close } },
	{ VOPNAME_READ,		{ .vop_read = drmglue_anon_vop_read } },
	{ VOPNAME_WRITE,	{ .vop_write = drmglue_anon_vop_write } },
	{ VOPNAME_IOCTL,	{ .vop_ioctl = drmglue_anon_vop_ioctl } },
	{ VOPNAME_GETATTR,	{ .vop_getattr = drmglue_anon_vop_getattr } },
	{ VOPNAME_ACCESS,	{ .vop_access = drmglue_anon_vop_access } },
	{ VOPNAME_INACTIVE,	{ .vop_inactive = drmglue_anon_vop_inactive } },
	{ VOPNAME_SEEK,		{ .vop_seek = drmglue_anon_vop_seek } },
	{ VOPNAME_MAP,		{ .vop_map = drmglue_anon_vop_map } },
	{ VOPNAME_ADDMAP,	{ .vop_addmap = drmglue_anon_vop_addmap } },
	{ VOPNAME_DELMAP,	{ .vop_delmap = drmglue_anon_vop_delmap } },
	{ VOPNAME_POLL,		{ .vop_poll = drmglue_anon_vop_poll } },
	{ VOPNAME_FRLOCK,	{ .error = fs_nosys } },
	{ VOPNAME_SPACE,	{ .error = fs_nosys } },
	{ VOPNAME_GETPAGE,	{ .error = fs_nosys } },
	{ VOPNAME_PUTPAGE,	{ .error = fs_nosys } },
	{ VOPNAME_SHRLOCK,	{ .error = fs_nosys } },
	{ NULL,			{ NULL } }
};

static const fs_operation_def_t drmglue_anon_vfsops_template[] = {
	{ NULL,			{ NULL } }
};

/*
 * Backend descriptor operations.
 */
static void *
drmglue_vn_alloc(struct lkpi_file *f)
{
	vnode_t *vp = vn_alloc(KM_SLEEP);	/* v_count = 1 */

	vn_setops(vp, drmglue_anon_vnodeops);
	vp->v_type = VREG;
	vp->v_vfsp = drmglue_anon_vfsp;
	vp->v_data = f;
	vp->v_flag |= VNOSWAP;
	vn_exists(vp);
	return (vp);
}

static void
drmglue_vn_hold(void *v)
{
	VN_HOLD((vnode_t *)v);
}

static bool
drmglue_vn_rele_last(void *v)
{
	vnode_t *vp = v;

	/* The usual VOP_INACTIVE re-check (count is still 1 on entry). */
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1) {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		return (false);
	}
	vp->v_count = 0;
	mutex_exit(&vp->v_lock);
	return (true);
}

static void
drmglue_vn_free(void *v)
{
	vnode_t *vp = v;

	vn_invalid(vp);
	vn_free(vp);
}

static int
drmglue_fd_reserve(unsigned int oflags)
{
	int fd = ufalloc(0);

	return (fd == -1 ? -EMFILE : fd);
}

static void
drmglue_fd_unreserve(int fd)
{
	setf(fd, NULL);
}

static void
drmglue_fd_install(int fd, void *vn, unsigned int oflags)
{
	file_t *fp;
	int fflag = 0;

	switch (oflags & O_ACCMODE) {
	case O_RDONLY:
		fflag = FREAD;
		break;
	case O_WRONLY:
		fflag = FWRITE;
		break;
	default:
		fflag = FREAD | FWRITE;
		break;
	}
	if (oflags & O_NONBLOCK)
		fflag |= FNONBLOCK;
	/* fdp == NULL: allocate the file_t only; cannot fail. */
	(void) falloc((vnode_t *)vn, fflag, &fp, NULL);
	mutex_exit(&fp->f_tlock);
	setf(fd, fp);
	if (oflags & O_CLOEXEC)
		f_setfd_or(fd, FD_CLOEXEC);
}

static struct lkpi_file *
drmglue_fd_get(int fd)
{
	struct lkpi_file *f = NULL;
	file_t *fp;

	if ((fp = getf(fd)) == NULL)
		return (NULL);
	if (fp->f_vnode != NULL && vn_matchops(fp->f_vnode,
	    drmglue_anon_vnodeops))
		f = get_file(VTOF(fp->f_vnode));
	releasef(fd);
	return (f);
}

static void
drmglue_file_fini(struct lkpi_file *f)
{
	if (f->f_lkpi_mmap_vp != NULL) {
		VN_RELE((vnode_t *)f->f_lkpi_mmap_vp);
		f->f_lkpi_mmap_vp = NULL;
	}
}

void
drmglue_anon_be_fill(struct lkpi_glue_backend *be)
{
	be->vn_alloc = drmglue_vn_alloc;
	be->vn_hold = drmglue_vn_hold;
	be->vn_rele_last = drmglue_vn_rele_last;
	be->vn_free = drmglue_vn_free;
	be->fd_reserve = drmglue_fd_reserve;
	be->fd_unreserve = drmglue_fd_unreserve;
	be->fd_install = drmglue_fd_install;
	be->fd_get = drmglue_fd_get;
	be->file_fini = drmglue_file_fini;
}

int
drmglue_anon_init(void)
{
	int r;

	r = vn_make_ops("drmglue_anon", drmglue_anon_vnodeops_template,
	    &drmglue_anon_vnodeops);
	if (r != 0)
		return (r);
	r = vfs_makefsops(drmglue_anon_vfsops_template, &drmglue_anon_vfsops);
	if (r != 0) {
		vn_freevnodeops(drmglue_anon_vnodeops);
		return (r);
	}
	drmglue_anon_vfsp = kmem_zalloc(sizeof (vfs_t), KM_SLEEP);
	VFS_INIT(drmglue_anon_vfsp, drmglue_anon_vfsops, NULL);
	drmglue_anon_vfsp->vfs_dev = makedevice(drmglue_major == (major_t)-1 ?
	    0 : drmglue_major, LKPI_NODE_ANON);
	return (0);
}

void
drmglue_anon_fini(void)
{
	/* Module unload is refused while any anon file exists (docs/10). */
	kmem_free(drmglue_anon_vfsp, sizeof (vfs_t));
	vfs_freevfsops(drmglue_anon_vfsops);
	vn_freevnodeops(drmglue_anon_vnodeops);
}
