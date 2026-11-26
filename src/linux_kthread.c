/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * kthread and kthread_worker (see <linux/kthread.h>). Clean-room.
 *
 * Lifetime: the task starts with one reference, owned by the thread and
 * dropped by lkpi_task_unbind() when the thread function returns. The
 * struct lkpi_kthread is freed with the task (lkpi_task_free()).
 * kthread_stop() holds an extra reference while it waits for the
 * `exited` completion, which the thread signals while still holding its
 * own reference, so the completion is never freed under the signaller.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linuxkpi/subsys.h>

struct lkpi_kthread {
	int			(*fn)(void *);
	void			*data;
	struct task_struct	*task;
	struct completion	exited;
	struct completion	parked;
	int			result;
	bool			should_stop;
	bool			should_park;
	bool			is_parked;
};

void lkpi_kthread_free(struct lkpi_kthread *kt);

void
lkpi_kthread_free(struct lkpi_kthread *kt)
{
	lkpi_completion_destroy(&kt->exited);
	lkpi_completion_destroy(&kt->parked);
	kfree(kt);
}

static void
lkpi_kthread_main(void *arg)
{
	struct lkpi_kthread *kt = arg;
	struct task_struct *t = kt->task;

	(void) lkpi_task_bind(t);
	/* Created stopped: wait for the first wake_up_process(). */
	while (READ_ONCE(t->__state) != TASK_RUNNING)
		schedule();
	if (!READ_ONCE(kt->should_stop))
		kt->result = kt->fn(kt->data);
	else
		kt->result = -EINTR;
	complete_all(&kt->exited);
	lkpi_task_unbind();	/* may free t and kt */
}

struct task_struct *
lkpi_kthread_create(int (*fn)(void *), void *data, const char *fmt, ...)
{
	struct lkpi_kthread *kt;
	struct task_struct *t;
	va_list ap;

	kt = kzalloc(sizeof (*kt), GFP_KERNEL);
	t = lkpi_task_alloc();
	if (kt == NULL || t == NULL) {
		kfree(kt);
		if (t != NULL)
			lkpi_task_free(t);
		return (ERR_PTR(-ENOMEM));
	}
	kt->fn = fn;
	kt->data = data;
	kt->task = t;
	init_completion(&kt->exited);
	init_completion(&kt->parked);
	va_start(ap, fmt);
	(void) lkpi_plat_vsnprintf(t->comm, sizeof (t->comm), fmt, ap);
	va_end(ap);
	t->flags |= PF_KTHREAD;
	t->pid = 0;
	t->tgid = 0;
	t->__state = TASK_UNINTERRUPTIBLE;
	t->lkpi_kthread = kt;
	if (lkpi_plat_thread_create_detached(lkpi_kthread_main, kt,
	    t->comm) != 0) {
		lkpi_task_free(t);	/* frees kt too */
		return (ERR_PTR(-ENOMEM));
	}
	return (t);
}

int
kthread_stop(struct task_struct *t)
{
	struct lkpi_kthread *kt = t->lkpi_kthread;
	int r;

	if (kt == NULL)
		return (-EINVAL);
	get_task_struct(t);
	WRITE_ONCE(kt->should_stop, true);
	kthread_unpark(t);
	(void) wake_up_process(t);
	wait_for_completion(&kt->exited);
	r = kt->result;
	put_task_struct(t);
	return (r);
}

int
kthread_stop_put(struct task_struct *t)
{
	int r = kthread_stop(t);

	put_task_struct(t);
	return (r);
}

static struct lkpi_kthread *
lkpi_kthread_self(void)
{
	return (lkpi_plat_in_interrupt() ? NULL : current->lkpi_kthread);
}

bool
kthread_should_stop(void)
{
	struct lkpi_kthread *kt = lkpi_kthread_self();

	return (kt != NULL && READ_ONCE(kt->should_stop));
}

bool
__kthread_should_park(struct task_struct *t)
{
	return (t->lkpi_kthread != NULL &&
	    READ_ONCE(t->lkpi_kthread->should_park));
}

bool
kthread_should_park(void)
{
	struct lkpi_kthread *kt = lkpi_kthread_self();

	return (kt != NULL && READ_ONCE(kt->should_park));
}

void *
kthread_data(struct task_struct *t)
{
	return (t->lkpi_kthread != NULL ? t->lkpi_kthread->data : NULL);
}

/* Called by the thread itself when kthread_should_park(). */
void
kthread_parkme(void)
{
	struct lkpi_kthread *kt = lkpi_kthread_self();

	if (kt == NULL)
		return;
	for (;;) {
		set_current_state(TASK_PARKED);
		if (!READ_ONCE(kt->should_park))
			break;
		if (!kt->is_parked) {
			kt->is_parked = true;
			complete_all(&kt->parked);
		}
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	kt->is_parked = false;
}

int
kthread_park(struct task_struct *t)
{
	struct lkpi_kthread *kt = t->lkpi_kthread;

	if (kt == NULL || completion_done(&kt->exited))
		return (-ENOSYS);
	WRITE_ONCE(kt->should_park, true);
	if (t != current) {
		(void) wake_up_process(t);
		wait_for_completion(&kt->parked);
	}
	return (0);
}

void
kthread_unpark(struct task_struct *t)
{
	struct lkpi_kthread *kt = t->lkpi_kthread;

	if (kt == NULL || !READ_ONCE(kt->should_park))
		return;
	reinit_completion(&kt->parked);
	WRITE_ONCE(kt->should_park, false);
	(void) wake_up_state(t, TASK_PARKED);
}

/* kthread_worker */

void
lkpi_kthread_work_fn(struct work_struct *w)
{
	struct kthread_work *kw = container_of(w, struct kthread_work,
	    lkpi_work);

	kw->func(kw);
}

void
lkpi_kthread_dwork_fn(struct work_struct *w)
{
	struct kthread_delayed_work *kdw = container_of(to_delayed_work(w),
	    struct kthread_delayed_work, lkpi_dwork);

	kdw->work.func(&kdw->work);
}

struct kthread_worker *
lkpi_kthread_create_worker(unsigned int flags, const char *fmt, ...)
{
	struct kthread_worker *w;
	char name[32];
	va_list ap;

	va_start(ap, fmt);
	(void) lkpi_plat_vsnprintf(name, sizeof (name), fmt, ap);
	va_end(ap);
	w = kzalloc(sizeof (*w), GFP_KERNEL);
	if (w == NULL)
		return (ERR_PTR(-ENOMEM));
	w->flags = flags;
	w->lkpi_wq = alloc_ordered_workqueue("%s", 0, name);
	if (w->lkpi_wq == NULL) {
		kfree(w);
		return (ERR_PTR(-ENOMEM));
	}
	return (w);
}

void
kthread_destroy_worker(struct kthread_worker *w)
{
	if (w == NULL)
		return;
	destroy_workqueue(w->lkpi_wq);
	kfree(w);
}
