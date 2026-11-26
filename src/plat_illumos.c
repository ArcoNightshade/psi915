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
 * A joinable thread is emulated with a done flag under a mutex: the thread
 * runs fn, sets done, broadcasts and calls thread_exit(); the joiner waits
 * for done and then frees the descriptor. illumos mutexes may be destroyed
 * as soon as the last holder has dropped them, so the joiner may free the
 * descriptor while the exiting thread is still in thread_exit().
 *
 * Detached threads (lkpi_plat_thread_create_detached) free the descriptor
 * themselves before thread_exit().
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

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_ILLUMOS_KERNEL

struct lkpi_plat_thread {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
	bool			done;
	bool			detached;
	void			(*fn)(void *);
	void			*arg;
};

static void
lkpi_plat_thread_start(void *arg)
{
	struct lkpi_plat_thread *t = arg;

	t->fn(t->arg);
	if (t->detached) {
		lkpi_plat_cv_destroy(&t->cv);
		lkpi_plat_mutex_destroy(&t->lock);
		lkpi_plat_free(t, sizeof (*t));
		thread_exit();
	}
	lkpi_plat_mutex_enter(&t->lock);
	t->done = true;
	lkpi_plat_cv_broadcast(&t->cv);
	lkpi_plat_mutex_exit(&t->lock);
	thread_exit();
}

static struct lkpi_plat_thread *
lkpi_plat_thread_spawn(void (*fn)(void *), void *arg, bool detached)
{
	struct lkpi_plat_thread *t;

	t = lkpi_plat_zalloc(sizeof (*t), LKPI_PLAT_SLEEP);
	lkpi_plat_mutex_init(&t->lock, NULL);
	lkpi_plat_cv_init(&t->cv);
	t->fn = fn;
	t->arg = arg;
	t->detached = detached;
	(void) thread_create(NULL, 0, lkpi_plat_thread_start, t, 0, &p0,
	    TS_RUN, minclsyspri);
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
	lkpi_plat_mutex_enter(&t->lock);
	while (!t->done)
		lkpi_plat_cv_wait(&t->cv, &t->lock);
	lkpi_plat_mutex_exit(&t->lock);
	lkpi_plat_cv_destroy(&t->cv);
	lkpi_plat_mutex_destroy(&t->lock);
	lkpi_plat_free(t, sizeof (*t));
}

/*
 * Signals can only be pending for threads with an lwp (user threads in a
 * system call, e.g. an ioctl); kernel threads have none. ISSIG(JUSTLOOKING)
 * is the check cv_wait_sig() uses, without side effects.
 */
bool
lkpi_plat_signal_pending(void)
{
	kthread_t *t = curthread;

	if (servicing_interrupt() || ttolwp(t) == NULL)
		return (false);
	return (ISSIG(t, JUSTLOOKING) != 0);
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

#endif /* LINUXKPI_ILLUMOS_KERNEL */
