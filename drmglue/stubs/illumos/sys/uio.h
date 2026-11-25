/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_UIO_H
#define	_STUB_SYS_UIO_H
#include <sys/types.h>
typedef struct iovec {
	caddr_t	iov_base;
	size_t	iov_len;
} iovec_t;
typedef enum uio_seg { UIO_USERSPACE, UIO_SYSSPACE, UIO_USERISPACE }
    uio_seg_t;
typedef struct uio {
	iovec_t		*uio_iov;
	int		uio_iovcnt;
	offset_t	uio_loffset;
	uio_seg_t	uio_segflg;
	ushort_t	uio_fmode;
	ushort_t	uio_extflg;
	offset_t	uio_limit;
	ssize_t		uio_resid;
} uio_t;
#endif
