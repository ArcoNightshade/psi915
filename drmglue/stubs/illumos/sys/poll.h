/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_POLL_H
#define	_STUB_SYS_POLL_H
#include <sys/types.h>
typedef struct pollhead {
	void	*ph_list;
	void	*ph_dummy;
} pollhead_t;
#define	POLLIN		0x0001
#define	POLLPRI		0x0002
#define	POLLOUT		0x0004
#define	POLLRDNORM	0x0040
#define	POLLERR		0x0008
#define	POLLHUP		0x0010
#define	POLLNVAL	0x0020
#define	POLLET		0x2000
extern void pollwakeup(pollhead_t *, short);
extern void pollhead_clean(pollhead_t *);
#endif
