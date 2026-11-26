/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_VFS_H
#define	_STUB_SYS_VFS_H
#include <sys/types.h>
typedef struct vfsops vfsops_t;
typedef struct vfs {
	vfsops_t	*vfs_op;
	dev_t		vfs_dev;
	void		*vfs_data;
} vfs_t;
extern void vfs_init(vfs_t *, vfsops_t *, void *);
extern void vfs_freevfsops(vfsops_t *);
#define	VFS_INIT(vfsp, op, data)	vfs_init((vfsp), (op), (data))
#endif
