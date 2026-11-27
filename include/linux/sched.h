/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * scheduler-facing API (task states, current, schedule(), wake-ups);
 * design cross-checked against FreeBSD's LinuxKPI sched.h and
 * linux_current.c / linux_schedule.c (BSD-2-Clause).
 *
 * struct task_struct is a KPI object, not the illumos kthread_t. Every
 * thread that evaluates `current` gets one, created lazily and kept in
 * thread-specific data (tsd_create(), see src/linux_sched.c); it is freed
 * when the thread exits (TSD destructor) and its last reference is
 * dropped. Interrupt handlers share one static task (no allocation in
 * interrupt context). kthread_create() threads get theirs up front.
 *
 * Sleeping. Each task has a private kmutex_t/kcondvar_t pair. The Linux
 * protocol works unchanged:
 *
 *   set_current_state(TASK_INTERRUPTIBLE);   state store + full barrier
 *   if (!condition) schedule();              sleep while state != RUNNING
 *   __set_current_state(TASK_RUNNING);
 *
 * and wake_up_process() (full barrier, then state -> TASK_RUNNING under
 * the task's lock and a cv_broadcast) cannot be lost between the
 * condition check and the sleep.
 *
 * Signals. TASK_INTERRUPTIBLE sleeps use cv_wait_sig(9F) and return early
 * when a signal is pending (only possible for a user thread inside a
 * system call; kernel threads have no signals). TASK_KILLABLE sleeps end
 * early only for SIGKILL: they use cv_wait_sig() and, while only a
 * non-fatal signal is pending, fall back to 10 ms uninterruptible slices
 * polling for SIGKILL. There is no freezer and no preemption control:
 * preempt_disable() and friends are no-ops (the illumos kernel is fully
 * preemptive and no KPI lock relies on preemption being off).
 */
#ifndef _LINUXKPI_LINUX_SCHED_H_
#define	_LINUXKPI_LINUX_SCHED_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/refcount.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/smp.h>
#include <asm/barrier.h>

#define	TASK_RUNNING		0x00000000
#define	TASK_INTERRUPTIBLE	0x00000001
#define	TASK_UNINTERRUPTIBLE	0x00000002
#define	__TASK_STOPPED		0x00000004
#define	__TASK_TRACED		0x00000008
#define	EXIT_DEAD		0x00000010
#define	EXIT_ZOMBIE		0x00000020
#define	TASK_PARKED		0x00000040
#define	TASK_DEAD		0x00000080
#define	TASK_WAKEKILL		0x00000100
#define	TASK_WAKING		0x00000200
#define	TASK_NOLOAD		0x00000400
#define	TASK_NEW		0x00000800
#define	TASK_FREEZABLE		0x00002000
#define	TASK_KILLABLE		(TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
#define	TASK_STOPPED		(TASK_WAKEKILL | __TASK_STOPPED)
#define	TASK_IDLE		(TASK_UNINTERRUPTIBLE | TASK_NOLOAD)
#define	TASK_NORMAL		(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)

#define	PF_EXITING		0x00000004
#define	PF_MEMALLOC		0x00000800
#define	PF_NOFREEZE		0x00008000
#define	PF_MEMALLOC_NOFS	0x00040000
#define	PF_MEMALLOC_NOIO	0x00080000
#define	PF_KTHREAD		0x00200000
#define	PF_WQ_WORKER		0x00000020

#define	TASK_COMM_LEN		16

#ifndef MAX_SCHEDULE_TIMEOUT
#define	MAX_SCHEDULE_TIMEOUT	LONG_MAX
#endif

#include <linux/pid.h>
#include <linux/bitmap.h>	/* via cpumask.h in Linux */

struct mm_struct;
struct lkpi_kthread;
struct lkpi_worker;

struct task_struct {
	/* Linux-visible fields */
	long			__state;	/* TASK_*; READ_ONCE/WRITE_ONCE */
	unsigned int		flags;		/* PF_* */
	pid_t			pid;
	pid_t			tgid;
	int			prio;
	char			comm[TASK_COMM_LEN];
	struct mm_struct	*mm;
	refcount_t		usage;
	/* KPI private */
	lkpi_plat_mutex_t	lkpi_sleep_lock;
	lkpi_plat_cv_t		lkpi_sleep_cv;
	void			*lkpi_thread;	/* lkpi_plat_curthread() */
	struct lkpi_kthread	*lkpi_kthread;	/* kthread_create() threads */
	struct lkpi_worker	*lkpi_worker;	/* workqueue worker threads */
	bool			lkpi_uaccess_kernel; /* see <linux/uaccess.h> */
	bool			lkpi_static;	/* never freed (irq task) */
	struct pid		*lkpi_tgid_ref;	/* task_tgid(): held for the task */
	void			*lkpi_attach_dip; /* in attach/resume of this dip */
	struct pid		*lkpi_pid_ref;	/* task_pid() */
};

struct task_struct *lkpi_current(void);
#define	current			lkpi_current()
#define	get_current()		lkpi_current()

struct task_struct *lkpi_task_alloc(void);
void lkpi_task_free(struct task_struct *t);

static inline struct task_struct *
get_task_struct(struct task_struct *t)
{
	if (!t->lkpi_static)
		refcount_inc(&t->usage);
	return (t);
}

static inline void
put_task_struct(struct task_struct *t)
{
	if (!t->lkpi_static && refcount_dec_and_test(&t->usage))
		lkpi_task_free(t);
}

#define	task_state(t)		READ_ONCE((t)->__state)
#define	get_current_state()	READ_ONCE(current->__state)
#define	__set_current_state(s)	WRITE_ONCE(current->__state, (long)(s))
#define	set_current_state(s)	do {					\
	WRITE_ONCE(current->__state, (long)(s));			\
	smp_mb();							\
} while (0)
#define	set_special_state(s)	set_current_state(s)
#define	task_is_running(t)	(READ_ONCE((t)->__state) == TASK_RUNNING)

long schedule_timeout(long timeout);
void schedule(void);
int wake_up_process(struct task_struct *t);
int wake_up_state(struct task_struct *t, unsigned int state);

static inline long
schedule_timeout_interruptible(long timeout)
{
	set_current_state(TASK_INTERRUPTIBLE);
	return (schedule_timeout(timeout));
}

static inline long
schedule_timeout_uninterruptible(long timeout)
{
	set_current_state(TASK_UNINTERRUPTIBLE);
	return (schedule_timeout(timeout));
}

static inline long
schedule_timeout_killable(long timeout)
{
	set_current_state(TASK_KILLABLE);
	return (schedule_timeout(timeout));
}

static inline long
schedule_timeout_idle(long timeout)
{
	set_current_state(TASK_IDLE);
	return (schedule_timeout(timeout));
}

#define	io_schedule()			schedule()
#define	io_schedule_timeout(t)		schedule_timeout(t)
#define	schedule_preempt_disabled()	schedule()

/* Only the current task's signals are visible (illumos ISSIG()). */
static inline int
signal_pending(struct task_struct *t)
{
	return (t == current && lkpi_plat_signal_pending());
}

static inline int
fatal_signal_pending(struct task_struct *t)
{
	return (t == current && lkpi_plat_fatal_signal_pending());
}

static inline int
signal_pending_state(unsigned int state, struct task_struct *t)
{
	if ((state & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)) == 0)
		return (0);
	if (state & TASK_INTERRUPTIBLE)
		return (signal_pending(t));
	return (fatal_signal_pending(t));
}

#define	__fatal_signal_pending(t)	fatal_signal_pending(t)
#define	send_sig(sig, t, priv)		(-ENOSYS)
#define	flush_signals(t)		do { } while (0)
#define	allow_signal(sig)		do { } while (0)
#define	disallow_signal(sig)		do { } while (0)

/* Fully preemptive kernel: rescheduling points are free. */
#define	cond_resched()			((int)0)
#define	cond_resched_lock(l)		((int)0)
#define	cond_resched_rcu()		do { } while (0)
#define	need_resched()			(false)
#define	yield()				lkpi_plat_yield()
#define	sched_yield()			lkpi_plat_yield()

static inline pid_t
task_pid_nr(const struct task_struct *t)
{
	return (t->pid);
}

#define	task_pid_vnr(t)		task_pid_nr(t)
#define	task_tgid_nr(t)		((t)->tgid)
#define	task_tgid_vnr(t)	((t)->tgid)
#define	task_pid_nr_ns(t, ns)	task_pid_nr(t)
#define	task_nice(t)		(0)
#define	set_user_nice(t, n)	do { (void)(t); (void)(n); } while (0)
#define	sched_set_fifo(t)	do { (void)(t); } while (0)
#define	sched_set_fifo_low(t)	do { (void)(t); } while (0)
#define	sched_set_normal(t, n)	do { (void)(t); (void)(n); } while (0)
#define	same_thread_group(a, b)	((a)->tgid == (b)->tgid)
#define	task_cpu(t)		(0)
#define	sched_setscheduler_nocheck(t, p, s) ((int)0)

static inline char *
get_task_comm(char *buf, struct task_struct *t)
{
	memcpy(buf, t->comm, TASK_COMM_LEN);
	buf[TASK_COMM_LEN - 1] = '\0';
	return (buf);
}

#define	__get_task_comm(buf, len, t)	get_task_comm(buf, t)

static inline bool
is_idle_task(const struct task_struct *t)
{
	(void) t;
	return (false);
}

/* For kernel threads, current->mm is NULL (no user address space). */
#define	PF_IDLE			0x00000002

#endif /* _LINUXKPI_LINUX_SCHED_H_ */
