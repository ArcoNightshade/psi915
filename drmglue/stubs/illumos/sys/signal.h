/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SIGNAL_H
#define	_STUB_SYS_SIGNAL_H
#include <sys/types.h>
typedef struct {
	uint32_t	__sigbits[3];
} k_sigset_t;
#define	SIGKILL		9
#define	sigmask(n)	((unsigned int)1 << (((n) - 1) & (32 - 1)))
#define	sigword(n)	(((unsigned int)((n) - 1)) >> 5)
#define	sigismember(s, n)	(sigmask(n) & (s)->__sigbits[sigword(n)])
#endif
