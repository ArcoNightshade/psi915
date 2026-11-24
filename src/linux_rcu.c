/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Epoch-based RCU emulation (see <linux/rcupdate.h>). Clean-room design.
 *
 * Readers. Each thread inside a read-side critical section owns a reader
 * slot: owner (the thread), nesting depth, and the global epoch read at
 * its outermost rcu_read_lock() (0 while idle). A thread's slot is
 * primary[hash(thread)] if that was free when it entered, otherwise one
 * of a few overflow slots (searched only while lkpi_rcu_ovf_used != 0).
 * Only the owner writes owner/nesting; the epoch is published with a full
 * barrier before the section's first access.
 *
 * Writers. synchronize_rcu() atomically advances the epoch to E (a full
 * barrier) and then waits for every slot whose epoch is non-zero and below
 * E. A reader that read the old epoch but published it after the scan
 * passed its slot is harmless: the barrier pairing (epoch store, mb, data
 * load in the reader; update, epoch RMW, slot load in the writer) means it
 * then sees the updated data. Readers that start later have epochs >= E
 * and are not waited for, so writers cannot starve.
 *
 * Callbacks. call_rcu() appends to a FIFO consumed by one service thread,
 * which detaches the whole queue, waits for a grace period and runs the
 * batch. rcu_barrier() waits until every callback queued before it ran.
 */

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linuxkpi/subsys.h>

#define	LKPI_RCU_PRIMARY_SHIFT	8
#define	LKPI_RCU_PRIMARY	(1U << LKPI_RCU_PRIMARY_SHIFT)
#define	LKPI_RCU_OVERFLOW	64
#define	LKPI_RCU_SPINS		1000

struct lkpi_rcu_slot {
	void		*owner;
	unsigned long	epoch;
	unsigned int	nesting;
} __attribute__((__aligned__(64)));

static struct lkpi_rcu_slot lkpi_rcu_primary[LKPI_RCU_PRIMARY];
static struct lkpi_rcu_slot lkpi_rcu_ovf[LKPI_RCU_OVERFLOW];
static unsigned int lkpi_rcu_ovf_used;
static unsigned long lkpi_rcu_epoch = 1;

int lkpi_rcu_test_force_overflow;

static inline struct lkpi_rcu_slot *
lkpi_rcu_primary_slot(void *self)
{
	uint64_t h = (uint64_t)(uintptr_t)self;

	h = (h ^ (h >> 17)) * 0x9e3779b97f4a7c15ULL;
	return (&lkpi_rcu_primary[h >> (64 - LKPI_RCU_PRIMARY_SHIFT)]);
}

/* The calling thread's slot, or NULL if it is not in a read section. */
static struct lkpi_rcu_slot *
lkpi_rcu_find(void *self)
{
	struct lkpi_rcu_slot *s = lkpi_rcu_primary_slot(self);
	unsigned int i;

	if (READ_ONCE(s->owner) == self)
		return (s);
	if (__atomic_load_n(&lkpi_rcu_ovf_used, __ATOMIC_ACQUIRE) == 0)
		return (NULL);
	for (i = 0; i < LKPI_RCU_OVERFLOW; i++)
		if (READ_ONCE(lkpi_rcu_ovf[i].owner) == self)
			return (&lkpi_rcu_ovf[i]);
	return (NULL);
}

static bool
lkpi_rcu_try_claim(struct lkpi_rcu_slot *s, void *self)
{
	void *expect = NULL;

	if (READ_ONCE(s->owner) != NULL)
		return (false);
	return (__atomic_compare_exchange_n(&s->owner, &expect, self, false,
	    __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
}

static struct lkpi_rcu_slot *
lkpi_rcu_claim(void *self)
{
	struct lkpi_rcu_slot *s = lkpi_rcu_primary_slot(self);
	unsigned int i;

	if (!lkpi_rcu_test_force_overflow && lkpi_rcu_try_claim(s, self))
		return (s);
	/* Announce before claiming, so lkpi_rcu_find() will look. */
	__atomic_fetch_add(&lkpi_rcu_ovf_used, 1, __ATOMIC_SEQ_CST);
	for (;;) {
		for (i = 0; i < LKPI_RCU_OVERFLOW; i++)
			if (lkpi_rcu_try_claim(&lkpi_rcu_ovf[i], self))
				return (&lkpi_rcu_ovf[i]);
		/* More concurrent readers than slots: wait for one. */
		lkpi_cpu_relax();
	}
}

void
rcu_read_lock(void)
{
	void *self = lkpi_plat_curthread();
	struct lkpi_rcu_slot *s = lkpi_rcu_find(self);

	if (s != NULL) {
		s->nesting++;
		return;
	}
	s = lkpi_rcu_claim(self);
	s->nesting = 1;
	__atomic_store_n(&s->epoch,
	    __atomic_load_n(&lkpi_rcu_epoch, __ATOMIC_RELAXED),
	    __ATOMIC_RELAXED);
	/* Publish the epoch before any access inside the section. */
	smp_mb();
}

void
rcu_read_unlock(void)
{
	void *self = lkpi_plat_curthread();
	struct lkpi_rcu_slot *s = lkpi_rcu_find(self);

	if (WARN_ON_ONCE(s == NULL))
		return;		/* unbalanced rcu_read_unlock() */
	if (--s->nesting > 0)
		return;
	/* Accesses inside the section happen before the slot goes idle. */
	__atomic_store_n(&s->epoch, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s->owner, NULL, __ATOMIC_RELEASE);
	if (s >= &lkpi_rcu_ovf[0] && s < &lkpi_rcu_ovf[LKPI_RCU_OVERFLOW])
		__atomic_fetch_sub(&lkpi_rcu_ovf_used, 1, __ATOMIC_RELEASE);
}

bool
rcu_read_lock_held(void)
{
	struct lkpi_rcu_slot *s = lkpi_rcu_find(lkpi_plat_curthread());

	return (s != NULL && s->nesting > 0);
}

static bool
lkpi_rcu_slot_blocks(const struct lkpi_rcu_slot *s, unsigned long target)
{
	unsigned long e = __atomic_load_n(&s->epoch, __ATOMIC_ACQUIRE);

	return (e != 0 && e < target);
}

/* Wait until no reader slot holds an epoch older than target. */
static void
lkpi_rcu_wait_epoch(unsigned long target)
{
	struct lkpi_rcu_slot *s;
	unsigned int i, spins;

	for (i = 0; i < LKPI_RCU_PRIMARY + LKPI_RCU_OVERFLOW; i++) {
		s = i < LKPI_RCU_PRIMARY ? &lkpi_rcu_primary[i] :
		    &lkpi_rcu_ovf[i - LKPI_RCU_PRIMARY];
		spins = 0;
		while (lkpi_rcu_slot_blocks(s, target)) {
			if (++spins < LKPI_RCU_SPINS)
				lkpi_cpu_relax();
			else
				lkpi_plat_sleep_ticks(1);
		}
	}
	/* Order the readers' accesses before the caller's next step. */
	smp_mb();
}

unsigned long
get_state_synchronize_rcu(void)
{
	/* Full barrier: prior updates are ordered before the slot scan. */
	return (__atomic_add_fetch(&lkpi_rcu_epoch, 1, __ATOMIC_SEQ_CST));
}

bool
poll_state_synchronize_rcu(unsigned long cookie)
{
	unsigned int i;

	smp_mb();
	for (i = 0; i < LKPI_RCU_PRIMARY; i++)
		if (lkpi_rcu_slot_blocks(&lkpi_rcu_primary[i], cookie))
			return (false);
	for (i = 0; i < LKPI_RCU_OVERFLOW; i++)
		if (lkpi_rcu_slot_blocks(&lkpi_rcu_ovf[i], cookie))
			return (false);
	smp_mb();
	return (true);
}

void
cond_synchronize_rcu(unsigned long cookie)
{
	if (!poll_state_synchronize_rcu(cookie))
		lkpi_rcu_wait_epoch(cookie);
}

void
synchronize_rcu(void)
{
	if (WARN(rcu_read_lock_held(),
	    "linuxkpi: synchronize_rcu() inside an RCU read-side section\n"))
		return;
	might_sleep();
	lkpi_rcu_wait_epoch(get_state_synchronize_rcu());
}

/*
 * call_rcu() machinery. lkpi_rcu_lock protects the queue and counters.
 */
static lkpi_plat_mutex_t lkpi_rcu_lock = LKPI_PLAT_MUTEX_INITIALIZER;
static lkpi_plat_cv_t lkpi_rcu_work_cv = LKPI_PLAT_CV_INITIALIZER;
static lkpi_plat_cv_t lkpi_rcu_done_cv = LKPI_PLAT_CV_INITIALIZER;
static struct rcu_head *lkpi_rcu_head;
static struct rcu_head **lkpi_rcu_tail = &lkpi_rcu_head;
static unsigned long lkpi_rcu_queued;
static unsigned long lkpi_rcu_done;
static bool lkpi_rcu_stop;
static struct lkpi_plat_thread *lkpi_rcu_thread;

/* Callback "addresses" below this are kvfree_rcu() offsets. */
#define	LKPI_RCU_OFFSET_MAX	4096

static void
lkpi_rcu_invoke(struct rcu_head *head)
{
	uintptr_t f = (uintptr_t)head->func;

	if (f < LKPI_RCU_OFFSET_MAX)
		kvfree((char *)head - f);
	else
		head->func(head);
}

static void
lkpi_rcu_worker(void *arg)
{
	struct rcu_head *batch, *next;
	unsigned long n;

	(void) arg;
	lkpi_plat_mutex_enter(&lkpi_rcu_lock);
	for (;;) {
		while (lkpi_rcu_head == NULL && !lkpi_rcu_stop)
			lkpi_plat_cv_wait(&lkpi_rcu_work_cv, &lkpi_rcu_lock);
		if (lkpi_rcu_head == NULL)
			break;		/* stopping and drained */
		batch = lkpi_rcu_head;
		lkpi_rcu_head = NULL;
		lkpi_rcu_tail = &lkpi_rcu_head;
		lkpi_plat_mutex_exit(&lkpi_rcu_lock);

		synchronize_rcu();
		for (n = 0; batch != NULL; batch = next, n++) {
			next = batch->next;
			lkpi_rcu_invoke(batch);
		}

		lkpi_plat_mutex_enter(&lkpi_rcu_lock);
		lkpi_rcu_done += n;
		lkpi_plat_cv_broadcast(&lkpi_rcu_done_cv);
	}
	lkpi_plat_mutex_exit(&lkpi_rcu_lock);
}

int
lkpi_rcu_init(void)
{
	int error = 0;

	lkpi_plat_mutex_enter(&lkpi_rcu_lock);
	if (lkpi_rcu_thread == NULL) {
		lkpi_rcu_stop = false;
		lkpi_rcu_thread = lkpi_plat_thread_create(lkpi_rcu_worker,
		    NULL, "linuxkpi_rcu");
		if (lkpi_rcu_thread == NULL)
			error = ENOMEM;
	}
	lkpi_plat_mutex_exit(&lkpi_rcu_lock);
	return (error);
}

void
lkpi_rcu_fini(void)
{
	struct lkpi_plat_thread *t;

	rcu_barrier();
	lkpi_plat_mutex_enter(&lkpi_rcu_lock);
	t = lkpi_rcu_thread;
	lkpi_rcu_thread = NULL;
	lkpi_rcu_stop = true;
	lkpi_plat_cv_broadcast(&lkpi_rcu_work_cv);
	lkpi_plat_mutex_exit(&lkpi_rcu_lock);
	if (t != NULL)
		lkpi_plat_thread_join(t);
}

void
call_rcu(struct rcu_head *head, rcu_callback_t func)
{
	head->func = func;
	head->next = NULL;
#ifdef LINUXKPI_USERLAND
	/* Userland programs start the service thread on first use. */
	if (READ_ONCE(lkpi_rcu_thread) == NULL)
		(void) lkpi_rcu_init();
#endif
	lkpi_plat_mutex_enter(&lkpi_rcu_lock);
	if (unlikely(lkpi_rcu_thread == NULL)) {
		/* Kernel: module not initialised (or stopping). */
		lkpi_plat_mutex_exit(&lkpi_rcu_lock);
		WARN_ONCE(1, "linuxkpi: call_rcu() without RCU thread\n");
		synchronize_rcu();
		lkpi_rcu_invoke(head);
		return;
	}
	*lkpi_rcu_tail = head;
	lkpi_rcu_tail = &head->next;
	lkpi_rcu_queued++;
	lkpi_plat_cv_signal(&lkpi_rcu_work_cv);
	lkpi_plat_mutex_exit(&lkpi_rcu_lock);
}

void
lkpi_kvfree_rcu(struct rcu_head *head, size_t offset)
{
	if (WARN_ON(offset >= LKPI_RCU_OFFSET_MAX)) {
		synchronize_rcu();
		kvfree((char *)head - offset);
		return;
	}
	call_rcu(head, (rcu_callback_t)offset);
}

void
rcu_barrier(void)
{
	unsigned long target;

	lkpi_plat_mutex_enter(&lkpi_rcu_lock);
	target = lkpi_rcu_queued;
	while (lkpi_rcu_thread != NULL && lkpi_rcu_done < target)
		lkpi_plat_cv_wait(&lkpi_rcu_done_cv, &lkpi_rcu_lock);
	lkpi_plat_mutex_exit(&lkpi_rcu_lock);
}
