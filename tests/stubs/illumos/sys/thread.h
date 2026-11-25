/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_THREAD_H
#define	_STUB_SYS_THREAD_H
#include <sys/types.h>
#include <sys/signal.h>
/* Real illumos: kthread_t is struct _kthread (much larger), threadp() asm. */
struct proc;
struct _klwp;
typedef struct _kthread {
	k_sigset_t	t_sig;
	struct _klwp	*t_lwp;
	struct proc	*t_procp;
} kthread_t;
extern kthread_t *threadp(void);
#define	curthread	(threadp())
#define	ttolwp(x)	((x)->t_lwp)
#define	ttoproc(x)	((x)->t_procp)
#define	curproc		(ttoproc(curthread))
#define	TS_RUN		0x02
extern void tsd_create(uint_t *, void (*)(void *));
extern void tsd_destroy(uint_t *);
extern void *tsd_get(uint_t);
extern int tsd_set(uint_t, void *);
#endif
