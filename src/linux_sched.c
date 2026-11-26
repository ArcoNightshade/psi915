/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * task_struct, current and sleeping (see <linux/sched.h>). Clean-room.
 *
 * current: the task pointer lives in thread-specific data under
 * lkpi_task_key. The first `current` in a thread allocates the task and
 * stores it; the TSD destructor drops that reference when the thread
 * exits (or when the key is destroyed at module unload, for threads still
 * alive). Interrupt handlers get lkpi_irq_task, a static task that is
 * never freed and never sleeps.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linuxkpi/subsys.h>

static lkpi_plat_tsd_t lkpi_task_key;
static lkpi_plat_mutex_t lkpi_task_key_lock = LKPI_PLAT_MUTEX_INITIALIZER;
static volatile int lkpi_task_key_ready;

static struct task_struct lkpi_irq_task = {
	.__state = TASK_RUNNING,
	.comm = "irq",
	.lkpi_static = true,
};

atomic_t lkpi_task_live = ATOMIC_INIT(0);

struct task_struct *
lkpi_task_alloc(void)
{
	struct task_struct *t;

	t = kzalloc(sizeof (*t), GFP_KERNEL);
	if (t == NULL)
		return (NULL);
	t->__state = TASK_RUNNING;
	refcount_set(&t->usage, 1);
	lkpi_plat_mutex_init(&t->lkpi_sleep_lock, NULL);
	lkpi_plat_cv_init(&t->lkpi_sleep_cv);
	t->pid = (pid_t)lkpi_plat_getpid();
	t->tgid = t->pid;
	t->prio = 120;
	atomic_inc(&lkpi_task_live);
	return (t);
}

void lkpi_kthread_free(struct lkpi_kthread *kt);

void
lkpi_task_free(struct task_struct *t)
{
	if (t->lkpi_kthread != NULL)
		lkpi_kthread_free(t->lkpi_kthread);
	lkpi_plat_cv_destroy(&t->lkpi_sleep_cv);
	lkpi_plat_mutex_destroy(&t->lkpi_sleep_lock);
	atomic_dec(&lkpi_task_live);
	kfree(t);
}

/* TSD destructor: the thread is exiting (or the key is destroyed). */
static void
lkpi_task_dtor(void *arg)
{
	struct task_struct *t = arg;

	if (t != NULL)
		put_task_struct(t);
}

static void
lkpi_task_key_init(void)
{
	lkpi_plat_mutex_enter(&lkpi_task_key_lock);
	if (!lkpi_task_key_ready) {
		lkpi_plat_tsd_create(&lkpi_task_key, lkpi_task_dtor);
		smp_wmb();
		lkpi_task_key_ready = 1;
	}
	lkpi_plat_mutex_exit(&lkpi_task_key_lock);
}

int
lkpi_sched_init(void)
{
	lkpi_task_key_init();
	return (0);
}

void
lkpi_sched_fini(void)
{
	lkpi_plat_mutex_enter(&lkpi_task_key_lock);
	if (lkpi_task_key_ready) {
		/* Runs lkpi_task_dtor() for every thread with a task. */
		lkpi_plat_tsd_destroy(&lkpi_task_key);
		lkpi_task_key_ready = 0;
	}
	lkpi_plat_mutex_exit(&lkpi_task_key_lock);
}

/*
 * Make t the current thread's task (kthread start-up). Takes over the
 * caller's reference. Returns 0 or an errno.
 */
int
lkpi_task_bind(struct task_struct *t)
{
	if (!lkpi_task_key_ready)
		lkpi_task_key_init();
	t->lkpi_thread = lkpi_plat_curthread();
	return (lkpi_plat_tsd_set(lkpi_task_key, t));
}

/* Detach and release the current thread's task, if any (thread exit). */
void
lkpi_task_unbind(void)
{
	struct task_struct *t;

	if (!lkpi_task_key_ready)
		return;
	t = lkpi_plat_tsd_get(lkpi_task_key);
	if (t == NULL)
		return;
	(void) lkpi_plat_tsd_set(lkpi_task_key, NULL);
	put_task_struct(t);
}

static struct task_struct *
lkpi_current_slow(void)
{
	struct task_struct *t;

	if (!lkpi_task_key_ready)
		lkpi_task_key_init();
	t = lkpi_plat_tsd_get(lkpi_task_key);
	if (t != NULL)
		return (t);
	t = lkpi_task_alloc();	/* GFP_KERNEL: never fails */
	lkpi_plat_getcomm(t->comm, sizeof (t->comm));
	t->lkpi_thread = lkpi_plat_curthread();
	if (lkpi_plat_tsd_set(lkpi_task_key, t) != 0) {
		/* Cannot happen with KM_SLEEP; do not lose the thread. */
		WARN_ONCE(1, "linuxkpi: cannot record current task\n");
	}
	return (t);
}

struct task_struct *
lkpi_current(void)
{
	struct task_struct *t;

	if (lkpi_plat_in_interrupt())
		return (&lkpi_irq_task);
	if (likely(lkpi_task_key_ready)) {
		t = lkpi_plat_tsd_get(lkpi_task_key);
		if (likely(t != NULL))
			return (t);
	}
	return (lkpi_current_slow());
}

/*
 * Wake t if its state is in the mask. The barrier orders the waker's
 * condition stores before the state load, pairing with the barrier in
 * set_current_state() on the sleeper's side.
 */
int
wake_up_state(struct task_struct *t, unsigned int state)
{
	int woken = 0;

	smp_mb();
	if (t->lkpi_static)
		return (0);
	lkpi_plat_mutex_enter(&t->lkpi_sleep_lock);
	if ((unsigned long)READ_ONCE(t->__state) & state) {
		WRITE_ONCE(t->__state, TASK_RUNNING);
		lkpi_plat_cv_broadcast(&t->lkpi_sleep_cv);
		woken = 1;
	}
	lkpi_plat_mutex_exit(&t->lkpi_sleep_lock);
	return (woken);
}

int
wake_up_process(struct task_struct *t)
{
	return (wake_up_state(t, TASK_NORMAL | TASK_PARKED));
}

#define	LKPI_KILLABLE_SLICE_NS	10000000ULL	/* 10 ms */

/*
 * Sleep until the task state becomes TASK_RUNNING (a wake-up), the
 * timeout (jiffies) expires, or a signal the state admits arrives.
 * Returns the jiffies left (0 on timeout), or MAX_SCHEDULE_TIMEOUT for an
 * unbounded sleep. Leaves the task TASK_RUNNING.
 */
long
schedule_timeout(long timeout)
{
	struct task_struct *t = current;
	unsigned long end = 0;
	long state, left = 0;
	bool forever = timeout == MAX_SCHEDULE_TIMEOUT;
	bool intr, killable;
	int r;

	if (t->lkpi_static) {
		/* Interrupt context cannot sleep. */
		WARN_ONCE(1, "linuxkpi: schedule() in interrupt context\n");
		return (0);
	}
	if (timeout < 0) {
		WARN_ONCE(1, "linuxkpi: schedule_timeout(%ld)\n", timeout);
		__set_current_state(TASK_RUNNING);
		return (0);
	}
	if (!forever)
		end = lkpi_plat_ticks() + (unsigned long)timeout;
	state = READ_ONCE(t->__state);
	intr = (state & TASK_INTERRUPTIBLE) != 0;
	killable = !intr && (state & TASK_WAKEKILL) != 0;

	lkpi_plat_mutex_enter(&t->lkpi_sleep_lock);
	while (READ_ONCE(t->__state) != TASK_RUNNING) {
		if (intr && lkpi_plat_signal_pending())
			break;
		if (killable && lkpi_plat_fatal_signal_pending())
			break;
		if (!forever) {
			left = (long)(end - lkpi_plat_ticks());
			if (left <= 0)
				break;
		}
		if (killable && lkpi_plat_signal_pending()) {
			/*
			 * A non-fatal signal is pending, so cv_wait_sig()
			 * would return at once: poll for SIGKILL in slices.
			 */
			(void) lkpi_plat_cv_timedwait_ns(&t->lkpi_sleep_cv,
			    &t->lkpi_sleep_lock,
			    lkpi_plat_nsec() + LKPI_KILLABLE_SLICE_NS, false);
			continue;
		}
		if (forever) {
			if (intr || killable) {
				if (lkpi_plat_cv_wait_sig(&t->lkpi_sleep_cv,
				    &t->lkpi_sleep_lock) && intr)
					break;
			} else {
				lkpi_plat_cv_wait(&t->lkpi_sleep_cv,
				    &t->lkpi_sleep_lock);
			}
		} else {
			r = lkpi_plat_cv_timedwait(&t->lkpi_sleep_cv,
			    &t->lkpi_sleep_lock, left, intr || killable);
			if (r == LKPI_PLAT_WAIT_INTR && intr)
				break;
		}
	}
	WRITE_ONCE(t->__state, TASK_RUNNING);
	lkpi_plat_mutex_exit(&t->lkpi_sleep_lock);

	if (forever)
		return (MAX_SCHEDULE_TIMEOUT);
	left = (long)(end - lkpi_plat_ticks());
	return (left > 0 ? left : 0);
}

void
schedule(void)
{
	if (READ_ONCE(current->__state) == TASK_RUNNING) {
		lkpi_plat_yield();
		return;
	}
	(void) schedule_timeout(MAX_SCHEDULE_TIMEOUT);
}
