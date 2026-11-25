/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_PROC_H
#define	_STUB_SYS_PROC_H
#include <sys/thread.h>
#include <sys/signal.h>
#include <sys/user.h>
/* Real illumos: struct proc is much larger; only fields we use. */
typedef struct proc {
	uint_t		p_flag;
	k_sigset_t	p_sig;
	user_t		p_user;
} proc_t;
#define	SKILLED		0x00000100
#define	PTOU(p)		(&(p)->p_user)
extern proc_t p0;
extern kthread_t *thread_create(caddr_t, size_t, void (*)(), void *, size_t,
    proc_t *, int, pri_t);
extern void thread_exit(void) __attribute__((__noreturn__));
/* Real illumos: ISSIG() tests lwp/proc state, then calls issig(). */
#define	JUSTLOOKING	1
extern int issig(int);
#define	ISSIG(t, why)	issig(why)
#endif
