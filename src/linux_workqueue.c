/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Workqueues (see <linux/workqueue.h>). Clean-room design.
 *
 * Locking: lkpi_wq_lock protects every workqueue, worker and work item
 * field marked KPI private. Lock order: lkpi_wq_lock before the timer
 * engine lock (queue_delayed_work arms a timer while holding it); the
 * delayed-work timer callback takes lkpi_wq_lock with no timer lock held.
 * Never call a _sync timer operation with lkpi_wq_lock held.
 *
 * Work item state (lkpi_state):
 *   PENDING        set from a successful queue until a worker dequeues it
 *                  (or a cancel); covers the delayed timer and RCU wait
 *   PENDING|TIMER  delayed work whose timer is armed or firing
 *   PENDING|RCU    rcu_work waiting for its grace period
 * A work item is "running" when some worker's cur_work points to it;
 * that is the only record of execution, so nothing dereferences the item
 * after its function returned.
 *
 * Generations: lkpi_gen (global counter, stamped at each queueing) lets
 * flush_work() wait for "the instance queued at the time of the call":
 * it waits while the item is pending with gen <= T or a worker runs it
 * with gen <= T. lkpi_seq (per workqueue) does the same for
 * flush_workqueue().
 *
 * Workers: created by the manager thread on request (need_spawn), so
 * queue_work() never allocates. A queue that has pending work and no idle
 * worker requests one more, up to max_workers.
 */

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/timer.h>
#include <linux/rcupdate.h>
#include <linuxkpi/subsys.h>

/*
 * Workers per queue, created on demand when all are busy (and kept until
 * the queue is destroyed): Linux's default active limit (WQ_DFL_ACTIVE,
 * 256), because DRM and
 * i915 queue items that block on other items of the same queue (atomic
 * commit tails, fence work): a small cap can deadlock (docs/12 L3).
 */
#define	LKPI_WQ_MAX_WORKERS	WQ_DFL_ACTIVE

struct lkpi_worker {
	struct list_head	wq_entry;	/* wq->workers */
	struct list_head	all_entry;	/* lkpi_wq_all_workers */
	struct workqueue_struct	*wq;
	struct lkpi_plat_thread	*thread;
	struct work_struct	*cur_work;
	u64			cur_gen;
	u64			cur_seq;
	bool			exited;
};

struct workqueue_struct {
	char			name[32];
	unsigned int		flags;
	int			max_workers;
	struct list_head	pending;	/* work_struct.entry */
	struct list_head	workers;	/* lkpi_worker.wq_entry */
	struct list_head	mgr_entry;	/* manager request list */
	lkpi_plat_cv_t		cv;		/* workers wait here */
	int			nworkers;
	int			nidle;
	int			nspawning;
	bool			stop;
	bool			mgr_queued;
	u64			seq;
};

static lkpi_plat_mutex_t lkpi_wq_lock = LKPI_PLAT_MUTEX_INITIALIZER;
/* Flushers and cancellers wait here for executions to finish. */
static lkpi_plat_cv_t lkpi_wq_done_cv = LKPI_PLAT_CV_INITIALIZER;
static LIST_HEAD(lkpi_wq_all_workers);
static u64 lkpi_wq_gen;

/* Manager thread. */
static lkpi_plat_cv_t lkpi_wq_mgr_cv = LKPI_PLAT_CV_INITIALIZER;
static LIST_HEAD(lkpi_wq_mgr_list);
static struct lkpi_plat_thread *lkpi_wq_mgr_thread;
static bool lkpi_wq_mgr_stop;
static int lkpi_wq_users;

struct workqueue_struct *system_wq;
struct workqueue_struct *system_highpri_wq;
struct workqueue_struct *system_long_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_freezable_wq;
struct workqueue_struct *system_power_efficient_wq;
struct workqueue_struct *system_freezable_power_efficient_wq;

/* Caller holds lkpi_wq_lock. */
static struct lkpi_worker *
lkpi_work_runner(const struct work_struct *w)
{
	struct lkpi_worker *wk;

	list_for_each_entry(wk, &lkpi_wq_all_workers, all_entry)
		if (wk->cur_work == w)
			return (wk);
	return (NULL);
}

/* Ask the manager for another worker if the queue needs one. */
static void
lkpi_wq_maybe_grow(struct workqueue_struct *wq)
{
	if (wq->stop || wq->nidle > 0 || list_empty(&wq->pending))
		return;
	if (wq->nworkers + wq->nspawning >= wq->max_workers)
		return;
	if (wq->mgr_queued || lkpi_wq_mgr_thread == NULL)
		return;
	wq->mgr_queued = true;
	list_add_tail(&wq->mgr_entry, &lkpi_wq_mgr_list);
	lkpi_plat_cv_signal(&lkpi_wq_mgr_cv);
}

/* Put w at the tail of wq; caller holds the lock and set PENDING. */
static void
lkpi_work_enqueue(struct workqueue_struct *wq, struct work_struct *w)
{
	w->lkpi_wq = wq;
	w->lkpi_gen = ++lkpi_wq_gen;
	w->lkpi_seq = ++wq->seq;
	list_add_tail(&w->entry, &wq->pending);
	if (wq->nidle > 0)
		lkpi_plat_cv_signal(&wq->cv);
	else
		lkpi_wq_maybe_grow(wq);
}

/* First pending item of wq that is not running anywhere. */
static struct work_struct *
lkpi_wq_pick(struct workqueue_struct *wq)
{
	struct work_struct *w;

	list_for_each_entry(w, &wq->pending, entry)
		if (lkpi_work_runner(w) == NULL)
			return (w);
	return (NULL);
}

static void
lkpi_worker_main(void *arg)
{
	struct lkpi_worker *wk = arg;
	struct workqueue_struct *wq = wk->wq;
	struct work_struct *w;
	work_func_t fn;
	struct task_struct *t = current;

	t->flags |= PF_WQ_WORKER | PF_KTHREAD;
	t->lkpi_worker = wk;
	(void) strlcpy(t->comm, wq->name, sizeof (t->comm));

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	for (;;) {
		w = lkpi_wq_pick(wq);
		if (w == NULL) {
			if (wq->stop && list_empty(&wq->pending))
				break;
			wq->nidle++;
			lkpi_plat_cv_wait(&wq->cv, &lkpi_wq_lock);
			wq->nidle--;
			continue;
		}
		list_del_init(&w->entry);
		w->lkpi_state &= ~LKPI_WORK_PENDING;
		wk->cur_work = w;
		wk->cur_gen = w->lkpi_gen;
		wk->cur_seq = w->lkpi_seq;
		fn = w->func;
		/* More pending work and nobody idle: grow the pool. */
		lkpi_wq_maybe_grow(wq);
		lkpi_plat_mutex_exit(&lkpi_wq_lock);

		fn(w);	/* may free w */

		lkpi_plat_mutex_enter(&lkpi_wq_lock);
		wk->cur_work = NULL;
		lkpi_plat_cv_broadcast(&lkpi_wq_done_cv);
		/* Items held back by non-reentrancy may be runnable now. */
		if (!list_empty(&wq->pending))
			lkpi_plat_cv_broadcast(&wq->cv);
	}
	wk->exited = true;
	lkpi_plat_cv_broadcast(&lkpi_wq_done_cv);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	t->lkpi_worker = NULL;
	lkpi_task_unbind();
}

/* Create one worker for wq; called without the lock. */
static void
lkpi_wq_spawn(struct workqueue_struct *wq)
{
	struct lkpi_worker *wk;

	wk = kzalloc(sizeof (*wk), GFP_KERNEL);
	wk->wq = wq;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	list_add_tail(&wk->wq_entry, &wq->workers);
	list_add_tail(&wk->all_entry, &lkpi_wq_all_workers);
	wq->nworkers++;
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	wk->thread = lkpi_plat_thread_create(lkpi_worker_main, wk, wq->name);
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (wk->thread == NULL) {
		list_del(&wk->wq_entry);
		list_del(&wk->all_entry);
		wq->nworkers--;
		kfree(wk);
	}
	wq->nspawning--;
	lkpi_plat_cv_broadcast(&lkpi_wq_done_cv);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
}

static void
lkpi_wq_manager(void *arg)
{
	struct workqueue_struct *wq;

	(void) arg;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	while (!lkpi_wq_mgr_stop) {
		if (list_empty(&lkpi_wq_mgr_list)) {
			lkpi_plat_cv_wait(&lkpi_wq_mgr_cv, &lkpi_wq_lock);
			continue;
		}
		wq = list_first_entry(&lkpi_wq_mgr_list,
		    struct workqueue_struct, mgr_entry);
		list_del_init(&wq->mgr_entry);
		wq->mgr_queued = false;
		if (wq->stop || wq->nidle > 0 || list_empty(&wq->pending) ||
		    wq->nworkers + wq->nspawning >= wq->max_workers)
			continue;
		wq->nspawning++;
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		lkpi_wq_spawn(wq);
		lkpi_plat_mutex_enter(&lkpi_wq_lock);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	lkpi_task_unbind();
}

static struct workqueue_struct *
lkpi_wq_create(unsigned int flags, int max_active, const char *name)
{
	struct workqueue_struct *wq;
	int max;

	wq = kzalloc(sizeof (*wq), GFP_KERNEL);
	if (wq == NULL)
		return (NULL);
	(void) strlcpy(wq->name, name, sizeof (wq->name));
	wq->flags = flags;
	if ((flags & __WQ_ORDERED) || max_active == 1)
		max = 1;
	else if (max_active <= 0)
		max = LKPI_WQ_MAX_WORKERS;
	else
		max = max_active < LKPI_WQ_MAX_WORKERS ? max_active :
		    LKPI_WQ_MAX_WORKERS;
	wq->max_workers = max;
	INIT_LIST_HEAD(&wq->pending);
	INIT_LIST_HEAD(&wq->workers);
	INIT_LIST_HEAD(&wq->mgr_entry);
	lkpi_plat_cv_init(&wq->cv);
	/* One worker up front, so the queue works before the manager runs. */
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	wq->nspawning++;
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	lkpi_wq_spawn(wq);
	if (wq->nworkers == 0) {
		lkpi_plat_cv_destroy(&wq->cv);
		kfree(wq);
		return (NULL);
	}
	return (wq);
}

struct workqueue_struct *
lkpi_alloc_workqueue(unsigned int flags, int max_active, const char *fmt,
    ...)
{
	char name[32];
	va_list ap;

	if (lkpi_wq_mgr_thread == NULL && lkpi_workqueue_init() != 0)
		return (NULL);	/* userland: start on first use */
	va_start(ap, fmt);
	(void) lkpi_plat_vsnprintf(name, sizeof (name), fmt, ap);
	va_end(ap);
	return (lkpi_wq_create(flags, max_active, name));
}

/* Caller holds the lock. Is any item queued up to seq still busy? */
static bool
lkpi_wq_busy_upto(struct workqueue_struct *wq, u64 seq)
{
	struct work_struct *w;
	struct lkpi_worker *wk;

	list_for_each_entry(w, &wq->pending, entry)
		if (w->lkpi_seq <= seq)
			return (true);
	list_for_each_entry(wk, &wq->workers, wq_entry)
		if (wk->cur_work != NULL && wk->cur_seq <= seq)
			return (true);
	return (false);
}

void
flush_workqueue(struct workqueue_struct *wq)
{
	u64 seq;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	seq = wq->seq;
	while (lkpi_wq_busy_upto(wq, seq))
		lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
}

void
drain_workqueue(struct workqueue_struct *wq)
{
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	while (lkpi_wq_busy_upto(wq, ~0ULL))
		lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
	struct lkpi_worker *wk, *tmp;

	if (wq == NULL)
		return;
	drain_workqueue(wq);
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	wq->stop = true;
	if (wq->mgr_queued) {
		list_del_init(&wq->mgr_entry);
		wq->mgr_queued = false;
	}
	while (wq->nspawning > 0)
		lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
	lkpi_plat_cv_broadcast(&wq->cv);
	list_for_each_entry_safe(wk, tmp, &wq->workers, wq_entry) {
		while (!wk->exited)
			lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
		list_del(&wk->wq_entry);
		list_del(&wk->all_entry);
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		lkpi_plat_thread_join(wk->thread);
		kfree(wk);
		lkpi_plat_mutex_enter(&lkpi_wq_lock);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	lkpi_plat_cv_destroy(&wq->cv);
	kfree(wq);
}

bool
workqueue_congested(int cpu, struct workqueue_struct *wq)
{
	bool r;

	(void) cpu;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	r = !list_empty(&wq->pending) && wq->nidle == 0;
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (r);
}

bool
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *w)
{
	bool ok = false;

	(void) cpu;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (w->lkpi_cancel == 0 && !(w->lkpi_state & LKPI_WORK_PENDING)) {
		w->lkpi_state |= LKPI_WORK_PENDING;
		lkpi_work_enqueue(wq, w);
		ok = true;
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (ok);
}

/*
 * Delayed work timer. A firing is stale if the work was cancelled (TIMER
 * clear) or re-armed meanwhile (timer pending again).
 */
void
lkpi_delayed_work_timer_fn(struct timer_list *t)
{
	struct delayed_work *dw = from_timer(dw, t, timer);
	struct work_struct *w = &dw->work;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if ((w->lkpi_state & LKPI_WORK_TIMER) && !timer_pending(&dw->timer)) {
		w->lkpi_state &= ~LKPI_WORK_TIMER;
		lkpi_work_enqueue(dw->wq, w);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
}

/* Caller holds the lock and set PENDING. */
static void
lkpi_delayed_arm(struct workqueue_struct *wq, struct delayed_work *dw,
    unsigned long delay)
{
	dw->wq = wq;
	if (delay == 0) {
		lkpi_work_enqueue(wq, &dw->work);
		return;
	}
	dw->work.lkpi_state |= LKPI_WORK_TIMER;
	(void) mod_timer(&dw->timer, jiffies + delay);
}

bool
queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dw, unsigned long delay)
{
	struct work_struct *w = &dw->work;
	bool ok = false;

	(void) cpu;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (w->lkpi_cancel == 0 && !(w->lkpi_state & LKPI_WORK_PENDING)) {
		w->lkpi_state |= LKPI_WORK_PENDING;
		lkpi_delayed_arm(wq, dw, delay);
		ok = true;
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (ok);
}

/* Remove a pending instance; caller holds the lock. Returns whether pending. */
static bool
lkpi_work_unqueue(struct work_struct *w, struct timer_list *timer)
{
	unsigned int st = w->lkpi_state;

	if (!(st & LKPI_WORK_PENDING))
		return (false);
	if (st & LKPI_WORK_RCU)
		return (false);		/* cannot recall an RCU callback */
	if (st & LKPI_WORK_TIMER) {
		if (timer != NULL)
			(void) del_timer(timer);
	} else {
		list_del_init(&w->entry);
	}
	w->lkpi_state &= ~(LKPI_WORK_PENDING | LKPI_WORK_TIMER);
	return (true);
}

bool
mod_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dw, unsigned long delay)
{
	struct work_struct *w = &dw->work;
	bool was;

	(void) cpu;
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (w->lkpi_cancel != 0) {
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return (false);
	}
	was = lkpi_work_unqueue(w, &dw->timer);
	if (!(w->lkpi_state & LKPI_WORK_PENDING)) {
		w->lkpi_state |= LKPI_WORK_PENDING;
		lkpi_delayed_arm(wq, dw, delay);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (was);
}

static void
lkpi_rcu_work_cb(struct rcu_head *head)
{
	struct rcu_work *rw = container_of(head, struct rcu_work, rcu);

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	rw->work.lkpi_state &= ~LKPI_WORK_RCU;
	lkpi_work_enqueue(rw->wq, &rw->work);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
}

bool
queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rw)
{
	struct work_struct *w = &rw->work;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (w->lkpi_cancel != 0 || (w->lkpi_state & LKPI_WORK_PENDING)) {
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return (false);
	}
	w->lkpi_state |= LKPI_WORK_PENDING | LKPI_WORK_RCU;
	rw->wq = wq;
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	call_rcu(&rw->rcu, lkpi_rcu_work_cb);
	return (true);
}

bool
cancel_work(struct work_struct *w)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	was = lkpi_work_unqueue(w, NULL);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (was);
}

bool
cancel_delayed_work(struct delayed_work *dw)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	was = lkpi_work_unqueue(&dw->work, &dw->timer);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (was);
}

static bool
lkpi_cancel_sync(struct work_struct *w, struct timer_list *timer)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	w->lkpi_cancel++;
	was = lkpi_work_unqueue(w, timer);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	/* A timer callback in flight sees TIMER clear and does nothing. */
	if (timer != NULL)
		(void) del_timer_sync(timer);
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	/* An RCU-pending item cannot be recalled: let it run, then wait. */
	for (;;) {
		struct lkpi_worker *wk = lkpi_work_runner(w);

		if (!(w->lkpi_state & LKPI_WORK_RCU) &&
		    (wk == NULL || wk == current->lkpi_worker))
			break;
		lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
		was |= lkpi_work_unqueue(w, timer);
	}
	(void) lkpi_work_unqueue(w, timer);
	w->lkpi_cancel--;
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (was);
}

bool
cancel_work_sync(struct work_struct *w)
{
	return (lkpi_cancel_sync(w, NULL));
}

bool
cancel_delayed_work_sync(struct delayed_work *dw)
{
	return (lkpi_cancel_sync(&dw->work, &dw->timer));
}

bool
flush_work(struct work_struct *w)
{
	struct lkpi_worker *wk, *self = current->lkpi_worker;
	bool waited = false;
	u64 target;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	wk = lkpi_work_runner(w);
	if (wk == self)
		wk = NULL;	/* flushing itself: do not deadlock */
	if (!(w->lkpi_state & LKPI_WORK_PENDING) && wk == NULL) {
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return (false);
	}
	target = w->lkpi_gen;
	for (;;) {
		bool pending = (w->lkpi_state & LKPI_WORK_PENDING) &&
		    !(w->lkpi_state & LKPI_WORK_TIMER) &&
		    w->lkpi_gen <= target;
		bool running;

		wk = lkpi_work_runner(w);
		running = wk != NULL && wk != self && wk->cur_gen <= target;
		/* A pending RCU work counts: wait for it to be queued and run. */
		if (w->lkpi_state & LKPI_WORK_RCU)
			pending = true;
		if (!pending && !running)
			break;
		waited = true;
		lkpi_plat_cv_wait(&lkpi_wq_done_cv, &lkpi_wq_lock);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	return (waited);
}

bool
flush_delayed_work(struct delayed_work *dw)
{
	struct work_struct *w = &dw->work;
	bool queued = false;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if ((w->lkpi_state & (LKPI_WORK_PENDING | LKPI_WORK_TIMER)) ==
	    (LKPI_WORK_PENDING | LKPI_WORK_TIMER)) {
		(void) del_timer(&dw->timer);
		w->lkpi_state &= ~LKPI_WORK_TIMER;
		lkpi_work_enqueue(dw->wq, w);
		queued = true;
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	/* The pending instance is now queued; wait for it (it may be done). */
	return (flush_work(w) || queued);
}

bool
flush_rcu_work(struct rcu_work *rw)
{
	bool waited = false;

	if (READ_ONCE(rw->work.lkpi_state) & LKPI_WORK_RCU) {
		rcu_barrier();
		waited = true;
	}
	return (flush_work(&rw->work) || waited);
}

struct work_struct *
current_work(void)
{
	struct lkpi_worker *wk;

	if (lkpi_plat_in_interrupt())
		return (NULL);
	wk = current->lkpi_worker;
	return (wk != NULL ? READ_ONCE(wk->cur_work) : NULL);
}

bool
current_is_workqueue_rescuer(void)
{
	return (false);
}

int
lkpi_workqueue_init(void)
{
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (lkpi_wq_users++ > 0) {
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return (0);
	}
	lkpi_wq_mgr_stop = false;
	lkpi_wq_mgr_thread = lkpi_plat_thread_create(lkpi_wq_manager, NULL,
	    "linuxkpi_wqmgr");
	if (lkpi_wq_mgr_thread == NULL) {
		lkpi_wq_users--;
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return (ENOMEM);
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);

	system_wq = lkpi_wq_create(0, 0, "events");
	system_highpri_wq = lkpi_wq_create(WQ_HIGHPRI, 0, "events_highpri");
	system_long_wq = lkpi_wq_create(0, 0, "events_long");
	system_unbound_wq = lkpi_wq_create(WQ_UNBOUND, 0, "events_unbound");
	system_freezable_wq = lkpi_wq_create(WQ_FREEZABLE, 0,
	    "events_freezable");
	system_power_efficient_wq = lkpi_wq_create(WQ_POWER_EFFICIENT, 0,
	    "events_power_efficient");
	system_freezable_power_efficient_wq = lkpi_wq_create(
	    WQ_FREEZABLE | WQ_POWER_EFFICIENT, 0, "events_freezable_pwr_ef");
	if (system_wq == NULL || system_highpri_wq == NULL ||
	    system_long_wq == NULL || system_unbound_wq == NULL ||
	    system_freezable_wq == NULL || system_power_efficient_wq == NULL ||
	    system_freezable_power_efficient_wq == NULL) {
		lkpi_workqueue_fini();
		return (ENOMEM);
	}
	return (0);
}

void
lkpi_workqueue_fini(void)
{
	struct workqueue_struct **wqs[] = {
		&system_wq, &system_highpri_wq, &system_long_wq,
		&system_unbound_wq, &system_freezable_wq,
		&system_power_efficient_wq,
		&system_freezable_power_efficient_wq
	};
	struct lkpi_plat_thread *mgr;
	unsigned int i;

	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	if (lkpi_wq_users == 0 || --lkpi_wq_users > 0) {
		lkpi_plat_mutex_exit(&lkpi_wq_lock);
		return;
	}
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	for (i = 0; i < ARRAY_SIZE(wqs); i++) {
		destroy_workqueue(*wqs[i]);
		*wqs[i] = NULL;
	}
	lkpi_plat_mutex_enter(&lkpi_wq_lock);
	mgr = lkpi_wq_mgr_thread;
	lkpi_wq_mgr_thread = NULL;
	lkpi_wq_mgr_stop = true;
	lkpi_plat_cv_broadcast(&lkpi_wq_mgr_cv);
	lkpi_plat_mutex_exit(&lkpi_wq_lock);
	if (mgr != NULL)
		lkpi_plat_thread_join(mgr);
}
