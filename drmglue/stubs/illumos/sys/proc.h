/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_PROC_H
#define	_STUB_SYS_PROC_H
#include <sys/thread.h>
typedef struct proc proc_t;
extern proc_t p0;
extern kthread_t *thread_create(caddr_t, size_t, void (*)(), void *, size_t,
    proc_t *, int, pri_t);
extern void thread_exit(void) __attribute__((__noreturn__));
#endif
