/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Wait queues (see <linux/wait.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/hash.h>
#include <linuxkpi/subsys.h>

int
default_wake_function(struct wait_queue_entry *we, unsigned int mode,
    int flags, void *key)
{
	(void) flags;
	(void) key;
	return (wake_up_state(we->private, mode));
}

/*
 * Wake the task and dequeue the entry, so a later prepare_to_wait*()
 * re-queues it and finish_wait() has nothing left to do.
 */
int
autoremove_wake_function(struct wait_queue_entry *we, unsigned int mode,
    int flags, void *key)
{
	int r = default_wake_function(we, mode, flags, key);

	if (r)
		list_del_init(&we->entry);
	return (r);
}

int
woken_wake_function(struct wait_queue_entry *we, unsigned int mode,
    int flags, void *key)
{
	/* Pairs with the barrier in wait_woken(). */
	smp_mb();
	we->flags |= WQ_FLAG_WOKEN;
	return (default_wake_function(we, mode, flags, key));
}

void
add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *we)
{
	spin_lock(&wq->lock);
	we->flags &= ~WQ_FLAG_EXCLUSIVE;
	list_add(&we->entry, &wq->head);
	spin_unlock(&wq->lock);
}

void
add_wait_queue_exclusive(struct wait_queue_head *wq,
    struct wait_queue_entry *we)
{
	spin_lock(&wq->lock);
	we->flags |= WQ_FLAG_EXCLUSIVE;
	list_add_tail(&we->entry, &wq->head);
	spin_unlock(&wq->lock);
}

void
add_wait_queue_priority(struct wait_queue_head *wq,
    struct wait_queue_entry *we)
{
	spin_lock(&wq->lock);
	we->flags |= WQ_FLAG_EXCLUSIVE | WQ_FLAG_PRIORITY;
	list_add(&we->entry, &wq->head);
	spin_unlock(&wq->lock);
}

void
remove_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *we)
{
	spin_lock(&wq->lock);
	list_del_init(&we->entry);
	spin_unlock(&wq->lock);
}

void
prepare_to_wait(struct wait_queue_head *wq, struct wait_queue_entry *we,
    int state)
{
	spin_lock(&wq->lock);
	we->flags &= ~WQ_FLAG_EXCLUSIVE;
	if (list_empty(&we->entry))
		list_add(&we->entry, &wq->head);
	set_current_state(state);
	spin_unlock(&wq->lock);
}

/* Returns true if the entry was the first on the queue. */
bool
prepare_to_wait_exclusive(struct wait_queue_head *wq,
    struct wait_queue_entry *we, int state)
{
	bool first = false;

	spin_lock(&wq->lock);
	we->flags |= WQ_FLAG_EXCLUSIVE;
	if (list_empty(&we->entry)) {
		first = list_empty(&wq->head);
		list_add_tail(&we->entry, &wq->head);
	}
	set_current_state(state);
	spin_unlock(&wq->lock);
	return (first);
}

/*
 * Queue the entry and set the task state, or, if the state admits a
 * signal that is pending, dequeue it and return -ERESTARTSYS.
 */
long
prepare_to_wait_event(struct wait_queue_head *wq, struct wait_queue_entry *we,
    int state)
{
	long ret = 0;

	spin_lock(&wq->lock);
	if (signal_pending_state((unsigned int)state, current)) {
		list_del_init(&we->entry);
		ret = -ERESTARTSYS;
	} else {
		if (list_empty(&we->entry)) {
			if (we->flags & WQ_FLAG_EXCLUSIVE)
				list_add_tail(&we->entry, &wq->head);
			else
				list_add(&we->entry, &wq->head);
		}
		set_current_state(state);
	}
	spin_unlock(&wq->lock);
	return (ret);
}

void
finish_wait(struct wait_queue_head *wq, struct wait_queue_entry *we)
{
	__set_current_state(TASK_RUNNING);
	spin_lock(&wq->lock);
	if (!list_empty(&we->entry))
		list_del_init(&we->entry);
	spin_unlock(&wq->lock);
}

/*
 * Sleep unless woken_wake_function() already ran for this entry since the
 * last call; the entry stays queued (add_wait_queue() before the loop).
 */
long
wait_woken(struct wait_queue_entry *we, unsigned int mode, long timeout)
{
	set_current_state(mode);
	if ((READ_ONCE(we->flags) & WQ_FLAG_WOKEN) == 0 &&
	    !signal_pending_state(mode, current))
		timeout = schedule_timeout(timeout);
	__set_current_state(TASK_RUNNING);
	/* Pairs with the barrier in woken_wake_function(). */
	smp_mb();
	WRITE_ONCE(we->flags, we->flags & ~WQ_FLAG_WOKEN);
	return (timeout);
}

/*
 * Call each entry's function; stop after nr exclusive entries were woken
 * (nr == 0: wake all). Caller holds wq->lock. The function may dequeue
 * its own entry, so iterate safely.
 */
static int
lkpi_wake_up_common(struct wait_queue_head *wq, unsigned int mode, int nr,
    int flags, void *key)
{
	struct wait_queue_entry *we, *next;
	int woken = 0, r;

	list_for_each_entry_safe(we, next, &wq->head, entry) {
		unsigned int wflags = we->flags;

		r = we->func(we, mode, flags, key);
		if (r < 0)
			break;
		if (r)
			woken++;
		if (r && (wflags & WQ_FLAG_EXCLUSIVE) && nr > 0 && --nr == 0)
			break;
	}
	return (woken);
}

int
__wake_up(struct wait_queue_head *wq, unsigned int mode, int nr, void *key)
{
	int woken;

	spin_lock(&wq->lock);
	woken = lkpi_wake_up_common(wq, mode, nr, 0, key);
	spin_unlock(&wq->lock);
	return (woken);
}

void
__wake_up_locked_key(struct wait_queue_head *wq, unsigned int mode,
    void *key)
{
	(void) lkpi_wake_up_common(wq, mode, 1, 0, key);
}

void
__wake_up_locked(struct wait_queue_head *wq, unsigned int mode, int nr)
{
	(void) lkpi_wake_up_common(wq, mode, nr, 0, NULL);
}

/* Hashed wait queues for wait_var_event() and wait_on_bit(). */
#define	LKPI_VAR_WQ_BITS	6
#define	LKPI_VAR_WQ_COUNT	(1U << LKPI_VAR_WQ_BITS)

static struct wait_queue_head lkpi_var_wq[LKPI_VAR_WQ_COUNT];
static lkpi_plat_mutex_t lkpi_var_wq_lock = LKPI_PLAT_MUTEX_INITIALIZER;
static volatile int lkpi_var_wq_ready;

struct wait_queue_head *
__var_waitqueue(const void *p)
{
	unsigned int i;

	if (unlikely(!lkpi_var_wq_ready)) {
		lkpi_plat_mutex_enter(&lkpi_var_wq_lock);
		if (!lkpi_var_wq_ready) {
			for (i = 0; i < LKPI_VAR_WQ_COUNT; i++)
				init_waitqueue_head(&lkpi_var_wq[i]);
			smp_wmb();
			lkpi_var_wq_ready = 1;
		}
		lkpi_plat_mutex_exit(&lkpi_var_wq_lock);
	}
	return (&lkpi_var_wq[hash_ptr(p, LKPI_VAR_WQ_BITS)]);
}

void
wake_up_var(void *var)
{
	/* Order the caller's update before the waiter check (Linux does). */
	smp_mb();
	wake_up_all(__var_waitqueue(var));
}

void
wake_up_bit(void *word, int bit)
{
	smp_mb();
	wake_up_all(__var_waitqueue((char *)word + bit));
}

int
lkpi_wait_on_bit_timeout(unsigned long *word, int bit, unsigned int mode,
    unsigned long timeout)
{
	struct wait_queue_head *wq = __var_waitqueue((char *)word + bit);
	long r;

	if (!test_bit_acquire((unsigned long)bit, word))
		return (0);
	if (timeout >= (unsigned long)MAX_SCHEDULE_TIMEOUT) {
		r = __lkpi_wait_forever(*wq,
		    !test_bit_acquire((unsigned long)bit, word), mode, 0,
		    (void)0, (void)0);
		return (r < 0 ? -EINTR : 0);
	}
	r = __lkpi_wait_timeout(*wq,
	    !test_bit_acquire((unsigned long)bit, word), mode, timeout,
	    (void)0, (void)0);
	if (r < 0)
		return (-EINTR);
	return (r == 0 ? -EAGAIN : 0);
}

void
lkpi_wait_fini(void)
{
	unsigned int i;

	if (!lkpi_var_wq_ready)
		return;
	for (i = 0; i < LKPI_VAR_WQ_COUNT; i++)
		lkpi_waitqueue_destroy(&lkpi_var_wq[i]);
	lkpi_var_wq_ready = 0;
}

/* Last teardown step of the module: wait queues, then the task key. */
void
lkpi_sched_fini_last(void)
{
	lkpi_wait_fini();
	lkpi_sched_fini();
}
