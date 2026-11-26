/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_VNODE_H
#define	_STUB_SYS_VNODE_H
#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/uio.h>
typedef enum vtype { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VFIFO, VDOOR,
    VPROC, VSOCK, VPORT, VBAD } vtype_t;
struct vfs;
struct vnodeops;
typedef struct vnode {
	kmutex_t	v_lock;
	uint_t		v_flag;
	uint_t		v_count;
	void		*v_data;
	struct vfs	*v_vfsp;
	struct vnodeops	*v_op;
	enum vtype	v_type;
	dev_t		v_rdev;
} vnode_t;
typedef struct vnodeops vnodeops_t;
typedef struct caller_context caller_context_t;
#define	VNOSWAP		0x200
typedef struct vattr {
	uint_t		va_mask;
	vtype_t		va_type;
	mode_t		va_mode;
	uid_t		va_uid;
	gid_t		va_gid;
	dev_t		va_fsid;
	ino64_t		va_nodeid;
	nlink_t		va_nlink;
	u_offset_t	va_size;
	dev_t		va_rdev;
	uint_t		va_blksize;
	blkcnt64_t	va_nblocks;
	uint_t		va_seq;
} vattr_t;
#define	AT_ALL		0x3fff
extern vnode_t *vn_alloc(int);
extern void vn_free(vnode_t *);
extern void vn_setops(vnode_t *, vnodeops_t *);
extern int vn_matchops(vnode_t *, const vnodeops_t *);
extern void vn_exists(vnode_t *);
extern void vn_invalid(vnode_t *);
extern void vn_rele(vnode_t *);
extern void vn_hold(vnode_t *);
extern void vn_freevnodeops(vnodeops_t *);
extern vnode_t *makespecvp(dev_t, vtype_t);
#define	VN_HOLD(vp)	vn_hold(vp)
#define	VN_RELE(vp)	vn_rele(vp)
#endif
