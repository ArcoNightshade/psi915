/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Out-of-line illumos kernel platform primitives (see <linuxkpi/platform.h>):
 * service threads over thread_create(9F-like, uts/common/disp/thread.c).
 *
 * A joinable thread records its kernel thread id (t_did) before running
 * fn; the joiner calls thread_join(did), which returns once the thread
 * has left module text (docs/12 L2).
 *
 * Detached threads (lkpi_plat_thread_create_detached) free the descriptor
 * themselves before running fn.
 *
 * Also: signal checks, process identity, copyinstr and sub-tick sleeps,
 * which need process internals (<sys/proc.h>, <sys/user.h>).
 *
 * UNTESTED ON ILLUMOS.
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/disp.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/klwp.h>
#include <sys/schedctl.h>

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_ILLUMOS_KERNEL

/*
 * Joinable threads are joined with thread_join(t_did), which returns only
 * once the thread is off CPU in thread_exit(): after that no module text
 * runs on its behalf, so _fini may let the module be unloaded (docs/12
 * L2). The thread itself must not touch the descriptor after fn returns.
 * Detached threads (whose code may still run until thread_exit) are
 * only used by subsystems whose fini waits for them otherwise.
 */
struct lkpi_plat_thread {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
	kt_did_t		did;
	bool			started;
	bool			detached;
	void			(*fn)(void *);
	void			*arg;
};

static void
lkpi_plat_thread_start(void *arg)
{
	struct lkpi_plat_thread *t = arg;
	bool detached = t->detached;
	void (*fn)(void *) = t->fn;
	void *fa = t->arg;

	if (!detached) {
		/* The creator waits for the id before returning t. */
		lkpi_plat_mutex_enter(&t->lock);
		t->did = curthread->t_did;
		t->started = true;
		lkpi_plat_cv_broadcast(&t->cv);
		lkpi_plat_mutex_exit(&t->lock);
	} else {
		lkpi_plat_free(t, sizeof (*t));
	}
	fn(fa);
	thread_exit();
}

static struct lkpi_plat_thread *
lkpi_plat_thread_spawn(void (*fn)(void *), void *arg, bool detached)
{
	struct lkpi_plat_thread *t;

	t = lkpi_plat_zalloc(sizeof (*t), LKPI_PLAT_SLEEP);
	t->fn = fn;
	t->arg = arg;
	t->detached = detached;
	if (!detached) {
		lkpi_plat_mutex_init(&t->lock, NULL);
		lkpi_plat_cv_init(&t->cv);
	}
	(void) thread_create(NULL, 0, lkpi_plat_thread_start, t, 0, &p0,
	    TS_RUN, minclsyspri);
	if (detached)
		return (NULL);		/* t belongs to the thread now */
	lkpi_plat_mutex_enter(&t->lock);
	while (!t->started)
		lkpi_plat_cv_wait(&t->cv, &t->lock);
	lkpi_plat_mutex_exit(&t->lock);
	return (t);
}

struct lkpi_plat_thread *
lkpi_plat_thread_create(void (*fn)(void *), void *arg, const char *name)
{
	(void) name;
	return (lkpi_plat_thread_spawn(fn, arg, false));
}

int
lkpi_plat_thread_create_detached(void (*fn)(void *), void *arg,
    const char *name)
{
	(void) name;
	(void) lkpi_plat_thread_spawn(fn, arg, true);
	return (0);
}

void
lkpi_plat_thread_join(struct lkpi_plat_thread *t)
{
	thread_join(t->did);
	lkpi_plat_cv_destroy(&t->cv);
	lkpi_plat_mutex_destroy(&t->lock);
	lkpi_plat_free(t, sizeof (*t));
}

/*
 * Signals can only be pending for threads with an lwp (user threads in a
 * system call, e.g. an ioctl); kernel threads have none. The test is the
 * set of conditions under which cv_wait_sig() returns 0 without sleeping
 * (uts/common/os/condvar.c): a pending signal (ISSIG JUSTLOOKING), a hold
 * for fork/exec//proc (MUSTRETURN), lwp_sysabort, or schedctl
 * cancellation. Reporting all of them makes Linux wait loops return
 * -ERESTARTSYS (EINTR to user space) instead of spinning on a sleep that
 * returns at once, and lets the lwp reach the user boundary (docs/12 L1).
 */
bool
lkpi_plat_signal_pending(void)
{
	kthread_t *t = curthread;
	klwp_t *lwp;

	if (servicing_interrupt() || (lwp = ttolwp(t)) == NULL)
		return (false);
	return (ISSIG(t, JUSTLOOKING) != 0 || MUSTRETURN(ttoproc(t), t) != 0 ||
	    lwp->lwp_sysabort != 0 || schedctl_cancel_pending() != 0);
}

bool
lkpi_plat_fatal_signal_pending(void)
{
	kthread_t *t = curthread;
	proc_t *p;

	if (servicing_interrupt() || ttolwp(t) == NULL)
		return (false);
	p = ttoproc(t);
	return ((p->p_flag & SKILLED) != 0 ||
	    sigismember(&p->p_sig, SIGKILL) != 0 ||
	    sigismember(&t->t_sig, SIGKILL) != 0);
}

int
lkpi_plat_getpid(void)
{
	return ((int)ddi_get_pid());
}

void
lkpi_plat_getcomm(char *buf, size_t len)
{
	if (servicing_interrupt() || ttolwp(curthread) == NULL) {
		(void) strlcpy(buf, servicing_interrupt() ? "irq" : "kthread",
		    len);
		return;
	}
	(void) strlcpy(buf, PTOU(curproc)->u_comm, len);
}

long
lkpi_plat_copyinstr(const char *uaddr, char *kbuf, size_t len, bool kernel)
{
	size_t done = 0;
	int err;

	if (len == 0)
		return (0);
	err = kernel ? copystr(uaddr, kbuf, len, &done) :
	    copyinstr(uaddr, kbuf, len, &done);
	if (err == ENAMETOOLONG)
		return (-ENAMETOOLONG);
	if (err != 0)
		return (-EFAULT);
	return (done > 0 ? (long)done - 1 : 0);
}

/*
 * Sleep at least ns nanoseconds. Below 10 us busy-wait (a callout costs
 * more than that); otherwise wait on a private condition variable with an
 * absolute high-resolution deadline, so sleeps are not rounded up to
 * 1/hz (10 ms at the default hz = 100).
 */
void
lkpi_plat_sleep_ns(uint64_t ns)
{
	lkpi_plat_mutex_t m;
	lkpi_plat_cv_t cv;
	uint64_t deadline;

	if (ns < 10000) {
		drv_usecwait((clock_t)((ns + 999) / 1000));
		return;
	}
	deadline = lkpi_plat_nsec() + ns;
	lkpi_plat_mutex_init(&m, NULL);
	lkpi_plat_cv_init(&cv);
	lkpi_plat_mutex_enter(&m);
	while (lkpi_plat_cv_timedwait_ns(&cv, &m, deadline, false) !=
	    LKPI_PLAT_WAIT_TIMEOUT)
		continue;
	lkpi_plat_mutex_exit(&m);
	lkpi_plat_cv_destroy(&cv);
	lkpi_plat_mutex_destroy(&m);
}

/* Privileged caller (capable()): drv_priv(9F) on the thread's credentials. */
bool
lkpi_plat_privileged(void)
{
	if (servicing_interrupt())
		return (false);
	return (drv_priv(CRED()) == 0);
}

#endif /* LINUXKPI_ILLUMOS_KERNEL */
