/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_THREAD_H
#define	_STUB_SYS_THREAD_H
#include <sys/types.h>
/* Real illumos: kthread_t is struct _kthread, threadp() is inline asm. */
typedef struct _kthread kthread_t;
extern kthread_t *threadp(void);
#define	curthread	(threadp())
#define	TS_RUN		0x02
#endif
