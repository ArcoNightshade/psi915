/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux workqueue API from its
 * documented behaviour (Documentation/core-api/workqueue.rst); function
 * set cross-checked against FreeBSD's LinuxKPI workqueue.h (BSD-2-Clause).
 *
 * Implementation (src/linux_workqueue.c): each workqueue owns a pool of
 * KPI worker threads that grows on demand up to its limit (1 for ordered
 * workqueues, max_active capped at 16 otherwise, 16 for max_active 0) and
 * a FIFO of pending work items. A manager thread creates workers when
 * work is queued and no worker is idle, so queue_work() never allocates
 * and is safe from interrupt handlers below LOCK_LEVEL. Guarantees, as in
 * Linux:
 *
 *   - queue_work() on a pending item is a no-op returning false;
 *   - an item never runs concurrently with itself (non-reentrant), on any
 *     workqueue;
 *   - flush_work() waits for the last queued instance to finish;
 *   - cancel_work_sync() returns with the item neither pending nor
 *     running, and queueing attempts during the cancel fail;
 *   - a work function may free its own work item;
 *   - ordered workqueues run items one at a time in queueing order.
 *
 * Why not taskq(9F): a dispatched taskq entry cannot be cancelled, and
 * taskq_dispatch() can fail with TQ_NOSLEEP from interrupt context; both
 * would break the guarantees above. Bound/unbound, CPU affinity,
 * freezable and high-priority flags are accepted and ignored. All
 * workqueue state is under one global lock (simple, adequate for
 * driver-scale queueing rates).
 */
#ifndef _LINUXKPI_LINUX_WORKQUEUE_H_
#define	_LINUXKPI_LINUX_WORKQUEUE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/rcupdate.h>
#include <linux/bitops.h>

struct work_struct;
struct workqueue_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	atomic_long_t		data;	/* unused; layout compatibility */
	struct list_head	entry;
	work_func_t		func;
	/* KPI private, under the workqueue lock */
	struct workqueue_struct	*lkpi_wq;
	u64			lkpi_gen;	/* last queued instance */
	u64			lkpi_seq;	/* order within lkpi_wq */
	unsigned int		lkpi_state;
	unsigned int		lkpi_cancel;
};

#define	LKPI_WORK_PENDING	0x1	/* queued, timer armed or RCU wait */
#define	LKPI_WORK_TIMER		0x2	/* delayed work timer armed */
#define	LKPI_WORK_RCU		0x4	/* rcu_work grace period pending */

struct delayed_work {
	struct work_struct	work;
	struct timer_list	timer;
	struct workqueue_struct	*wq;
	int			cpu;
};

struct rcu_work {
	struct work_struct	work;
	struct rcu_head		rcu;
	struct workqueue_struct	*wq;
};

#define	WQ_UNBOUND		(1U << 1)
#define	WQ_FREEZABLE		(1U << 2)
#define	WQ_MEM_RECLAIM		(1U << 3)
#define	WQ_HIGHPRI		(1U << 4)
#define	WQ_CPU_INTENSIVE	(1U << 5)
#define	WQ_SYSFS		(1U << 6)
#define	WQ_POWER_EFFICIENT	(1U << 7)
#define	WQ_BH			(1U << 0)
#define	__WQ_ORDERED		(1U << 17)
#define	__WQ_LEGACY		(1U << 18)
#define	WQ_MAX_ACTIVE		512
#define	WQ_UNBOUND_MAX_ACTIVE	WQ_MAX_ACTIVE
#define	WQ_DFL_ACTIVE		(WQ_MAX_ACTIVE / 2)

#define	WORK_CPU_UNBOUND	(-1)
#define	WORK_STRUCT_PENDING_BIT	0

void lkpi_delayed_work_timer_fn(struct timer_list *t);

#define	__WORK_INITIALIZER(n, f) {					\
	.data = ATOMIC_LONG_INIT(0),					\
	.entry = LIST_HEAD_INIT((n).entry),				\
	.func = (f) }
#define	__DELAYED_WORK_INITIALIZER(n, f, tflags) {			\
	.work = __WORK_INITIALIZER((n).work, (f)),			\
	.timer = __TIMER_INITIALIZER(lkpi_delayed_work_timer_fn, (tflags)) }
#define	DECLARE_WORK(n, f)	struct work_struct n = __WORK_INITIALIZER(n, f)
#define	DECLARE_DELAYED_WORK(n, f)					\
	struct delayed_work n = __DELAYED_WORK_INITIALIZER(n, f, 0)
#define	DECLARE_DEFERRABLE_WORK(n, f)					\
	struct delayed_work n = __DELAYED_WORK_INITIALIZER(n, f, TIMER_DEFERRABLE)

static inline void
lkpi_init_work(struct work_struct *w, work_func_t fn)
{
	memset(w, 0, sizeof (*w));
	INIT_LIST_HEAD(&w->entry);
	w->func = fn;
}

#define	INIT_WORK(w, fn)		lkpi_init_work((w), (fn))
#define	INIT_WORK_ONSTACK(w, fn)	lkpi_init_work((w), (fn))
#define	__INIT_WORK(w, fn, onstack)	lkpi_init_work((w), (fn))
#define	destroy_work_on_stack(w)	do { (void)(w); } while (0)
#define	destroy_delayed_work_on_stack(w) do { (void)(w); } while (0)

static inline void
lkpi_init_delayed_work(struct delayed_work *dw, work_func_t fn,
    unsigned int tflags)
{
	lkpi_init_work(&dw->work, fn);
	timer_setup(&dw->timer, lkpi_delayed_work_timer_fn, tflags);
	dw->wq = NULL;
	dw->cpu = WORK_CPU_UNBOUND;
}

#define	INIT_DELAYED_WORK(dw, fn)	lkpi_init_delayed_work((dw), (fn), 0)
#define	INIT_DELAYED_WORK_ONSTACK(dw, fn) lkpi_init_delayed_work((dw), (fn), 0)
#define	INIT_DEFERRABLE_WORK(dw, fn)					\
	lkpi_init_delayed_work((dw), (fn), TIMER_DEFERRABLE)
#define	INIT_RCU_WORK(rw, fn)		lkpi_init_work(&(rw)->work, (fn))
#define	INIT_RCU_WORK_ONSTACK(rw, fn)	lkpi_init_work(&(rw)->work, (fn))

static inline struct delayed_work *
to_delayed_work(struct work_struct *w)
{
	return (container_of(w, struct delayed_work, work));
}

static inline struct rcu_work *
to_rcu_work(struct work_struct *w)
{
	return (container_of(w, struct rcu_work, work));
}

#define	work_pending(w)							\
	((READ_ONCE((w)->lkpi_state) & LKPI_WORK_PENDING) != 0)
#define	delayed_work_pending(dw)	work_pending(&(dw)->work)

extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_highpri_wq;
extern struct workqueue_struct *system_long_wq;
extern struct workqueue_struct *system_unbound_wq;
extern struct workqueue_struct *system_freezable_wq;
extern struct workqueue_struct *system_power_efficient_wq;
extern struct workqueue_struct *system_freezable_power_efficient_wq;
#define	system_percpu_wq	system_wq
#define	system_dfl_wq		system_unbound_wq

struct workqueue_struct *lkpi_alloc_workqueue(unsigned int flags,
    int max_active, const char *fmt, ...) __printf(3, 4);
#define	alloc_workqueue(fmt, flags, max_active, ...)			\
	lkpi_alloc_workqueue((flags), (max_active), (fmt), ##__VA_ARGS__)
#define	alloc_ordered_workqueue(fmt, flags, ...)			\
	lkpi_alloc_workqueue(WQ_UNBOUND | __WQ_ORDERED | (flags), 1,	\
	    (fmt), ##__VA_ARGS__)
#define	create_workqueue(name)						\
	lkpi_alloc_workqueue(WQ_MEM_RECLAIM, 1, "%s", (name))
#define	create_freezable_workqueue(name)				\
	lkpi_alloc_workqueue(WQ_FREEZABLE | WQ_UNBOUND | WQ_MEM_RECLAIM, 1, \
	    "%s", (name))
#define	create_singlethread_workqueue(name)				\
	alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM, (name))

void destroy_workqueue(struct workqueue_struct *wq);
void flush_workqueue(struct workqueue_struct *wq);
void drain_workqueue(struct workqueue_struct *wq);
bool workqueue_congested(int cpu, struct workqueue_struct *wq);

bool queue_work_on(int cpu, struct workqueue_struct *wq,
    struct work_struct *w);
bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dw, unsigned long delay);
bool mod_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dw, unsigned long delay);
bool queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rw);

bool cancel_work(struct work_struct *w);
bool cancel_work_sync(struct work_struct *w);
bool cancel_delayed_work(struct delayed_work *dw);
bool cancel_delayed_work_sync(struct delayed_work *dw);
bool flush_work(struct work_struct *w);
bool flush_delayed_work(struct delayed_work *dw);
bool flush_rcu_work(struct rcu_work *rw);
struct work_struct *current_work(void);
bool current_is_workqueue_rescuer(void);

#define	queue_work(wq, w)	queue_work_on(WORK_CPU_UNBOUND, (wq), (w))
#define	queue_work_node(n, wq, w) queue_work_on(WORK_CPU_UNBOUND, (wq), (w))
#define	queue_delayed_work(wq, dw, d)					\
	queue_delayed_work_on(WORK_CPU_UNBOUND, (wq), (dw), (d))
#define	mod_delayed_work(wq, dw, d)					\
	mod_delayed_work_on(WORK_CPU_UNBOUND, (wq), (dw), (d))
#define	schedule_work(w)	queue_work(system_wq, (w))
#define	schedule_work_on(c, w)	queue_work_on((c), system_wq, (w))
#define	schedule_delayed_work(dw, d) queue_delayed_work(system_wq, (dw), (d))
#define	schedule_delayed_work_on(c, dw, d)				\
	queue_delayed_work_on((c), system_wq, (dw), (d))
#define	flush_scheduled_work()	flush_workqueue(system_wq)
#define	disable_work_sync(w)	cancel_work_sync(w)
#define	disable_delayed_work_sync(dw) cancel_delayed_work_sync(dw)
#define	enable_work(w)		(true)
#define	enable_and_queue_work(wq, w) queue_work((wq), (w))
#define	work_on_cpu(cpu, fn, arg) ((fn)(arg))
#define	workqueue_set_max_active(wq, n) do { (void)(wq); (void)(n); } while (0)

#endif /* _LINUXKPI_LINUX_WORKQUEUE_H_ */
