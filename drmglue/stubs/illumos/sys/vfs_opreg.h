/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_VFS_OPREG_H
#define	_STUB_SYS_VFS_OPREG_H
#include <sys/vnode.h>
#include <sys/vfs.h>
struct as;
struct pollhead;
typedef union fs_func {
	int	(*error)();
	int	(*vop_open)(vnode_t **, int, cred_t *, caller_context_t *);
	int	(*vop_close)(vnode_t *, int, int, offset_t, cred_t *,
		    caller_context_t *);
	int	(*vop_read)(vnode_t *, uio_t *, int, cred_t *,
		    caller_context_t *);
	int	(*vop_write)(vnode_t *, uio_t *, int, cred_t *,
		    caller_context_t *);
	int	(*vop_ioctl)(vnode_t *, int, intptr_t, int, cred_t *, int *,
		    caller_context_t *);
	int	(*vop_getattr)(vnode_t *, vattr_t *, int, cred_t *,
		    caller_context_t *);
	int	(*vop_access)(vnode_t *, int, int, cred_t *,
		    caller_context_t *);
	void	(*vop_inactive)(vnode_t *, cred_t *, caller_context_t *);
	int	(*vop_seek)(vnode_t *, offset_t, offset_t *,
		    caller_context_t *);
	int	(*vop_map)(vnode_t *, offset_t, struct as *, caddr_t *, size_t,
		    uchar_t, uchar_t, uint_t, cred_t *, caller_context_t *);
	int	(*vop_addmap)(vnode_t *, offset_t, struct as *, caddr_t,
		    size_t, uchar_t, uchar_t, uint_t, cred_t *,
		    caller_context_t *);
	int	(*vop_delmap)(vnode_t *, offset_t, struct as *, caddr_t,
		    size_t, uint_t, uint_t, uint_t, cred_t *,
		    caller_context_t *);
	int	(*vop_poll)(vnode_t *, short, int, short *, struct pollhead **,
		    caller_context_t *);
} fs_func_p;
typedef struct fs_operation_def {
	char		*name;
	fs_func_p	func;
} fs_operation_def_t;
#define	VOPNAME_OPEN		"open"
#define	VOPNAME_CLOSE		"close"
#define	VOPNAME_READ		"read"
#define	VOPNAME_WRITE		"write"
#define	VOPNAME_IOCTL		"ioctl"
#define	VOPNAME_GETATTR		"getattr"
#define	VOPNAME_ACCESS		"access"
#define	VOPNAME_INACTIVE	"inactive"
#define	VOPNAME_SEEK		"seek"
#define	VOPNAME_MAP		"map"
#define	VOPNAME_ADDMAP		"addmap"
#define	VOPNAME_DELMAP		"delmap"
#define	VOPNAME_POLL		"poll"
#define	VOPNAME_FRLOCK		"frlock"
#define	VOPNAME_SPACE		"space"
#define	VOPNAME_GETPAGE		"getpage"
#define	VOPNAME_PUTPAGE		"putpage"
#define	VOPNAME_SHRLOCK		"shrlock"
extern int fs_nosys();
extern int vn_make_ops(const char *, const fs_operation_def_t *,
    vnodeops_t **);
extern int vfs_makefsops(const fs_operation_def_t *, vfsops_t **);
#endif
