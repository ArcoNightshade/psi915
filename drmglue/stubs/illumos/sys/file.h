/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_FILE_H
#define	_STUB_SYS_FILE_H
#include <sys/types.h>
#define	FKIOCTL		0x80000000
#include <sys/mutex.h>
struct vnode;
typedef struct file {
	kmutex_t	f_tlock;
	ushort_t	f_flag;
	ushort_t	f_flag2;
	struct vnode	*f_vnode;
	offset_t	f_offset;
	int		f_count;
	cred_t		*f_cred;
} file_t;
#define	FREAD		0x01
#define	FWRITE		0x02
#define	FNDELAY		0x04
#define	FNONBLOCK	0x80
#define	FMODELS		0x00f00000
#define	FD_CLOEXEC	1
extern file_t *getf(int);
extern void releasef(int);
extern int ufalloc(int);
extern int falloc(struct vnode *, int, file_t **, int *);
extern void setf(int, file_t *);
extern void f_setfd_or(int, short);
#endif
