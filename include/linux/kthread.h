/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux kthread API from its documented
 * behaviour; function set cross-checked against FreeBSD's LinuxKPI
 * kthread.h (BSD-2-Clause).
 *
 * kthread_create() starts an illumos kernel thread (thread_create(), p0,
 * minclsyspri) that waits until wake_up_process() before calling the
 * thread function, as in Linux. The task_struct reference belongs to the
 * thread and is dropped when it exits; kthread_stop() takes its own
 * reference while it waits, and, as in Linux, must only be used on a
 * thread that has not exited on its own (or one the caller holds a
 * reference to via get_task_struct()).
 *
 * kthread_worker is a thin layer over an ordered workqueue
 * (<linux/workqueue.h>); worker->task is NULL.
 */
#ifndef _LINUXKPI_LINUX_KTHREAD_H_
#define	_LINUXKPI_LINUX_KTHREAD_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

struct task_struct *lkpi_kthread_create(int (*fn)(void *), void *data,
    const char *fmt, ...) __printf(3, 4);
#define	kthread_create(fn, data, fmt, ...)				\
	lkpi_kthread_create((fn), (data), (fmt), ##__VA_ARGS__)
#define	kthread_create_on_node(fn, data, node, fmt, ...)		\
	lkpi_kthread_create((fn), (data), (fmt), ##__VA_ARGS__)
#define	kthread_run(fn, data, fmt, ...) ({				\
	struct task_struct *__k =					\
	    lkpi_kthread_create((fn), (data), (fmt), ##__VA_ARGS__);	\
	if (!IS_ERR(__k))						\
		(void) wake_up_process(__k);				\
	__k;								\
})

int kthread_stop(struct task_struct *t);
int kthread_stop_put(struct task_struct *t);
bool kthread_should_stop(void);
bool kthread_should_park(void);
bool __kthread_should_park(struct task_struct *t);
int kthread_park(struct task_struct *t);
void kthread_unpark(struct task_struct *t);
void kthread_parkme(void);
void *kthread_data(struct task_struct *t);

#define	kthread_bind(t, cpu)		do { (void)(t); (void)(cpu); } while (0)
#define	kthread_bind_mask(t, m)		do { (void)(t); (void)(m); } while (0)
#define	kthread_should_stop_or_park()	(kthread_should_stop() || kthread_should_park())
#define	kthread_freezable_should_stop(was) kthread_should_stop()
#define	set_freezable()			do { } while (0)
#define	try_to_freeze()			(false)
#define	freezing(t)			(false)

/* kthread_worker over an ordered workqueue. */
struct kthread_work;
typedef void (*kthread_work_func_t)(struct kthread_work *work);

struct kthread_worker {
	struct workqueue_struct	*lkpi_wq;
	struct task_struct	*task;
	unsigned int		flags;
};

struct kthread_work {
	struct work_struct	lkpi_work;
	kthread_work_func_t	func;
	struct kthread_worker	*worker;
};

struct kthread_delayed_work {
	struct kthread_work	work;
	struct delayed_work	lkpi_dwork;
};

void lkpi_kthread_work_fn(struct work_struct *w);
void lkpi_kthread_dwork_fn(struct work_struct *w);

static inline void
kthread_init_work(struct kthread_work *kw, kthread_work_func_t fn)
{
	INIT_WORK(&kw->lkpi_work, lkpi_kthread_work_fn);
	kw->func = fn;
	kw->worker = NULL;
}

static inline void
kthread_init_delayed_work(struct kthread_delayed_work *kdw,
    kthread_work_func_t fn)
{
	kthread_init_work(&kdw->work, fn);
	INIT_DELAYED_WORK(&kdw->lkpi_dwork, lkpi_kthread_dwork_fn);
}

struct kthread_worker *lkpi_kthread_create_worker(unsigned int flags,
    const char *fmt, ...) __printf(2, 3);
#define	kthread_create_worker(flags, fmt, ...)				\
	lkpi_kthread_create_worker((flags), (fmt), ##__VA_ARGS__)
#define	kthread_run_worker(flags, fmt, ...)				\
	lkpi_kthread_create_worker((flags), (fmt), ##__VA_ARGS__)
void kthread_destroy_worker(struct kthread_worker *w);

static inline bool
kthread_queue_work(struct kthread_worker *w, struct kthread_work *kw)
{
	kw->worker = w;
	return (queue_work(w->lkpi_wq, &kw->lkpi_work));
}

static inline bool
kthread_queue_delayed_work(struct kthread_worker *w,
    struct kthread_delayed_work *kdw, unsigned long delay)
{
	kdw->work.worker = w;
	return (queue_delayed_work(w->lkpi_wq, &kdw->lkpi_dwork, delay));
}

static inline bool
kthread_mod_delayed_work(struct kthread_worker *w,
    struct kthread_delayed_work *kdw, unsigned long delay)
{
	kdw->work.worker = w;
	return (mod_delayed_work(w->lkpi_wq, &kdw->lkpi_dwork, delay));
}

#define	kthread_flush_work(kw)		((void)flush_work(&(kw)->lkpi_work))
#define	kthread_flush_worker(w)		flush_workqueue((w)->lkpi_wq)
#define	kthread_cancel_work_sync(kw)	cancel_work_sync(&(kw)->lkpi_work)
#define	kthread_cancel_delayed_work_sync(kdw)				\
	cancel_delayed_work_sync(&(kdw)->lkpi_dwork)
#define	kthread_work_pending(kw)	work_pending(&(kw)->lkpi_work)

#endif /* _LINUXKPI_LINUX_KTHREAD_H_ */
