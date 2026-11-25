/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * wait queue API (include/linux/wait.h semantics as described in
 * Documentation/ and the kernel-doc of each function); function set
 * cross-checked against FreeBSD's LinuxKPI wait.h (BSD-2-Clause).
 *
 * A wait queue head is a spinlock_t and a list of entries. Waiting uses
 * the task sleep protocol of <linux/sched.h>: prepare_to_wait*() queues
 * the entry and sets the task state under the queue lock, the caller
 * re-tests its condition, then schedule()s; wake-ups call each entry's
 * function (autoremove_wake_function() by default), which wakes the task.
 *
 * wake_up*() may be called from interrupt handlers below LOCK_LEVEL (the
 * queue lock is an adaptive mutex); see <linux/spinlock.h> for handlers
 * above it.
 *
 * Return values follow Linux: the _timeout forms return 0 if the
 * condition is still false when the timeout elapsed, otherwise the
 * jiffies left (at least 1); the _interruptible/_killable forms return
 * -ERESTARTSYS when a signal ends the wait.
 */
#ifndef _LINUXKPI_LINUX_WAIT_H_
#define	_LINUXKPI_LINUX_WAIT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/errno.h>

struct wait_queue_entry;
typedef int (*wait_queue_func_t)(struct wait_queue_entry *wq_entry,
    unsigned int mode, int flags, void *key);

#define	WQ_FLAG_EXCLUSIVE	0x01
#define	WQ_FLAG_WOKEN		0x02
#define	WQ_FLAG_PRIORITY	0x10

struct wait_queue_entry {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	entry;
};
typedef struct wait_queue_entry wait_queue_entry_t;

struct wait_queue_head {
	spinlock_t		lock;
	struct list_head	head;
};
typedef struct wait_queue_head wait_queue_head_t;

int default_wake_function(struct wait_queue_entry *wq_entry,
    unsigned int mode, int flags, void *key);
int autoremove_wake_function(struct wait_queue_entry *wq_entry,
    unsigned int mode, int flags, void *key);
int woken_wake_function(struct wait_queue_entry *wq_entry,
    unsigned int mode, int flags, void *key);

#define	__WAIT_QUEUE_HEAD_INITIALIZER(name)				\
	{ .lock = __SPIN_LOCK_UNLOCKED(name.lock),			\
	    .head = LIST_HEAD_INIT(name.head) }
#define	DECLARE_WAIT_QUEUE_HEAD(name)					\
	struct wait_queue_head name = __WAIT_QUEUE_HEAD_INITIALIZER(name)
#define	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(name)	DECLARE_WAIT_QUEUE_HEAD(name)

#define	__WAITQUEUE_INITIALIZER(name, tsk)				\
	{ .flags = 0, .private = (tsk), .func = default_wake_function,	\
	    .entry = LIST_HEAD_INIT(name.entry) }
#define	DECLARE_WAITQUEUE(name, tsk)					\
	struct wait_queue_entry name = __WAITQUEUE_INITIALIZER(name, tsk)

#define	DEFINE_WAIT_FUNC(name, function)				\
	struct wait_queue_entry name = {				\
		.flags = 0, .private = current, .func = (function),	\
		.entry = LIST_HEAD_INIT(name.entry) }
#define	DEFINE_WAIT(name)	DEFINE_WAIT_FUNC(name, autoremove_wake_function)

static inline void
__init_waitqueue_head(struct wait_queue_head *wq)
{
	spin_lock_init(&wq->lock);
	INIT_LIST_HEAD(&wq->head);
}

#define	init_waitqueue_head(wq)		__init_waitqueue_head(wq)

static inline void
init_waitqueue_entry(struct wait_queue_entry *we, struct task_struct *t)
{
	we->flags = 0;
	we->private = t;
	we->func = default_wake_function;
	INIT_LIST_HEAD(&we->entry);
}

static inline void
init_waitqueue_func_entry(struct wait_queue_entry *we, wait_queue_func_t fn)
{
	we->flags = 0;
	we->private = NULL;
	we->func = fn;
	INIT_LIST_HEAD(&we->entry);
}

static inline void
init_wait_entry(struct wait_queue_entry *we, int flags)
{
	we->flags = (unsigned int)flags;
	we->private = current;
	we->func = autoremove_wake_function;
	INIT_LIST_HEAD(&we->entry);
}

#define	init_wait(we)		init_wait_entry(we, 0)

/* Not part of the Linux API (wait queues are never destroyed there). */
static inline void
lkpi_waitqueue_destroy(struct wait_queue_head *wq)
{
	lkpi_spin_lock_destroy(&wq->lock);
}

/* Racy hint, as in Linux: callers need their own ordering. */
static inline int
waitqueue_active(struct wait_queue_head *wq)
{
	return (!list_empty(&wq->head));
}

static inline bool
wq_has_sleeper(struct wait_queue_head *wq)
{
	smp_mb();
	return (waitqueue_active(wq));
}

static inline bool
wq_has_single_sleeper(struct wait_queue_head *wq)
{
	return (list_is_singular(&wq->head));
}

void add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *we);
void add_wait_queue_exclusive(struct wait_queue_head *wq,
    struct wait_queue_entry *we);
void add_wait_queue_priority(struct wait_queue_head *wq,
    struct wait_queue_entry *we);
void remove_wait_queue(struct wait_queue_head *wq,
    struct wait_queue_entry *we);
void prepare_to_wait(struct wait_queue_head *wq, struct wait_queue_entry *we,
    int state);
bool prepare_to_wait_exclusive(struct wait_queue_head *wq,
    struct wait_queue_entry *we, int state);
long prepare_to_wait_event(struct wait_queue_head *wq,
    struct wait_queue_entry *we, int state);
void finish_wait(struct wait_queue_head *wq, struct wait_queue_entry *we);
long wait_woken(struct wait_queue_entry *we, unsigned int mode,
    long timeout);

int __wake_up(struct wait_queue_head *wq, unsigned int mode, int nr,
    void *key);
void __wake_up_locked_key(struct wait_queue_head *wq, unsigned int mode,
    void *key);
void __wake_up_locked(struct wait_queue_head *wq, unsigned int mode, int nr);

/* Caller holds wq->lock. */
static inline void
__add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *we)
{
	list_add(&we->entry, &wq->head);
}

static inline void
__add_wait_queue_entry_tail(struct wait_queue_head *wq,
    struct wait_queue_entry *we)
{
	list_add_tail(&we->entry, &wq->head);
}

static inline void
__remove_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *we)
{
	(void) wq;
	list_del(&we->entry);
}

#define	wake_up(wq)		__wake_up(wq, TASK_NORMAL, 1, NULL)
#define	wake_up_nr(wq, nr)	__wake_up(wq, TASK_NORMAL, nr, NULL)
#define	wake_up_all(wq)		__wake_up(wq, TASK_NORMAL, 0, NULL)
#define	wake_up_locked(wq)	__wake_up_locked(wq, TASK_NORMAL, 1)
#define	wake_up_all_locked(wq)	__wake_up_locked(wq, TASK_NORMAL, 0)
#define	wake_up_interruptible(wq)					\
	__wake_up(wq, TASK_INTERRUPTIBLE, 1, NULL)
#define	wake_up_interruptible_nr(wq, nr)				\
	__wake_up(wq, TASK_INTERRUPTIBLE, nr, NULL)
#define	wake_up_interruptible_all(wq)					\
	__wake_up(wq, TASK_INTERRUPTIBLE, 0, NULL)
#define	wake_up_interruptible_sync(wq)					\
	__wake_up(wq, TASK_INTERRUPTIBLE, 1, NULL)
#define	wake_up_poll(wq, m)						\
	__wake_up(wq, TASK_NORMAL, 1, (void *)(uintptr_t)(m))
#define	wake_up_interruptible_poll(wq, m)				\
	__wake_up(wq, TASK_INTERRUPTIBLE, 1, (void *)(uintptr_t)(m))
#define	wake_up_interruptible_sync_poll(wq, m)				\
	__wake_up(wq, TASK_INTERRUPTIBLE, 1, (void *)(uintptr_t)(m))
#define	wake_up_locked_poll(wq, m)					\
	__wake_up_locked_key(wq, TASK_NORMAL, (void *)(uintptr_t)(m))

#define	__lkpi_wait_is_intr(state)					\
	(((state) & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)) != 0)

/*
 * Core loop. __tmo is the timeout in jiffies (MAX_SCHEDULE_TIMEOUT for
 * none). Evaluates to 0 or the jiffies left (>= 1) when cond became true,
 * 0 on timeout, -ERESTARTSYS on a signal. `pre`/`post` run around
 * schedule (used by the _lock_irq and _cmd forms).
 */
#define	__lkpi_wait_event(wq, cond, state, excl, tmo, pre, post) ({	\
	struct wait_queue_entry __we;					\
	long __left = (long)(tmo);					\
	long __res;							\
	init_wait_entry(&__we, (excl) ? WQ_FLAG_EXCLUSIVE : 0);	\
	for (;;) {							\
		long __int = prepare_to_wait_event(&(wq), &__we,	\
		    (state));						\
		if (cond) {						\
			__res = __left == 0 ? 1 : __left;		\
			break;						\
		}							\
		if (__left == 0) {					\
			__res = 0;					\
			break;						\
		}							\
		if (__lkpi_wait_is_intr(state) && __int != 0) {	\
			__res = __int;					\
			break;						\
		}							\
		pre;							\
		__left = schedule_timeout(__left);			\
		post;							\
	}								\
	finish_wait(&(wq), &__we);					\
	__res;								\
})

#define	__lkpi_wait_forever(wq, cond, state, excl, pre, post) ({	\
	long __r = 0;							\
	if (!(cond))							\
		__r = __lkpi_wait_event(wq, cond, state, excl,		\
		    MAX_SCHEDULE_TIMEOUT, pre, post);			\
	__r < 0 ? (int)__r : 0;						\
})

#define	__lkpi_wait_timeout(wq, cond, state, tmo, pre, post) ({		\
	long __t = (long)(tmo);						\
	long __r;							\
	if (cond)							\
		__r = __t == 0 ? 1 : __t;				\
	else								\
		__r = __lkpi_wait_event(wq, cond, state, 0, __t,	\
		    pre, post);						\
	__r;								\
})

#define	wait_event(wq, cond)						\
	((void)__lkpi_wait_forever(wq, cond, TASK_UNINTERRUPTIBLE, 0,	\
	    (void)0, (void)0))
#define	wait_event_freezable(wq, cond)					\
	__lkpi_wait_forever(wq, cond, TASK_INTERRUPTIBLE, 0, (void)0, (void)0)
#define	wait_event_idle(wq, cond)					\
	((void)__lkpi_wait_forever(wq, cond, TASK_IDLE, 0, (void)0, (void)0))
#define	wait_event_idle_exclusive(wq, cond)				\
	((void)__lkpi_wait_forever(wq, cond, TASK_IDLE, 1, (void)0, (void)0))
#define	wait_event_interruptible(wq, cond)				\
	__lkpi_wait_forever(wq, cond, TASK_INTERRUPTIBLE, 0, (void)0, (void)0)
#define	wait_event_interruptible_exclusive(wq, cond)			\
	__lkpi_wait_forever(wq, cond, TASK_INTERRUPTIBLE, 1, (void)0, (void)0)
#define	wait_event_killable(wq, cond)					\
	__lkpi_wait_forever(wq, cond, TASK_KILLABLE, 0, (void)0, (void)0)
#define	wait_event_killable_exclusive(wq, cond)				\
	__lkpi_wait_forever(wq, cond, TASK_KILLABLE, 1, (void)0, (void)0)
#define	wait_event_cmd(wq, cond, cmd1, cmd2)				\
	((void)__lkpi_wait_forever(wq, cond, TASK_UNINTERRUPTIBLE, 0,	\
	    cmd1, cmd2))
#define	wait_event_interruptible_cmd(wq, cond, cmd1, cmd2)		\
	__lkpi_wait_forever(wq, cond, TASK_INTERRUPTIBLE, 0, cmd1, cmd2)

#define	wait_event_timeout(wq, cond, tmo)				\
	__lkpi_wait_timeout(wq, cond, TASK_UNINTERRUPTIBLE, tmo,	\
	    (void)0, (void)0)
#define	wait_event_idle_timeout(wq, cond, tmo)				\
	__lkpi_wait_timeout(wq, cond, TASK_IDLE, tmo, (void)0, (void)0)
#define	wait_event_interruptible_timeout(wq, cond, tmo)			\
	__lkpi_wait_timeout(wq, cond, TASK_INTERRUPTIBLE, tmo,		\
	    (void)0, (void)0)
#define	wait_event_killable_timeout(wq, cond, tmo)			\
	__lkpi_wait_timeout(wq, cond, TASK_KILLABLE, tmo, (void)0, (void)0)

/* Called and returns with lock (a spinlock_t) held; drops it to sleep. */
#define	wait_event_lock_irq(wq, cond, lock)				\
	((void)__lkpi_wait_forever(wq, cond, TASK_UNINTERRUPTIBLE, 0,	\
	    spin_unlock_irq(&(lock)), spin_lock_irq(&(lock))))
#define	wait_event_lock_irq_cmd(wq, cond, lock, cmd)			\
	((void)__lkpi_wait_forever(wq, cond, TASK_UNINTERRUPTIBLE, 0,	\
	    do { spin_unlock_irq(&(lock)); cmd; } while (0),		\
	    spin_lock_irq(&(lock))))
#define	wait_event_interruptible_lock_irq(wq, cond, lock)		\
	__lkpi_wait_forever(wq, cond, TASK_INTERRUPTIBLE, 0,		\
	    spin_unlock_irq(&(lock)), spin_lock_irq(&(lock)))
#define	wait_event_lock_irq_timeout(wq, cond, lock, tmo)		\
	__lkpi_wait_timeout(wq, cond, TASK_UNINTERRUPTIBLE, tmo,	\
	    spin_unlock_irq(&(lock)), spin_lock_irq(&(lock)))
#define	wait_event_interruptible_lock_irq_timeout(wq, cond, lock, tmo)	\
	__lkpi_wait_timeout(wq, cond, TASK_INTERRUPTIBLE, tmo,		\
	    spin_unlock_irq(&(lock)), spin_lock_irq(&(lock)))

/*
 * Variable and bit waits: one of a fixed set of hashed wait queues is
 * chosen by address (src/linux_wait.c); waiters re-test their own
 * condition, so sharing a queue only costs spurious wake-ups.
 */
struct wait_queue_head *__var_waitqueue(const void *p);
void wake_up_var(void *var);
void wake_up_bit(void *word, int bit);

#define	wait_var_event(var, cond)					\
	wait_event(*__var_waitqueue(var), cond)
#define	wait_var_event_timeout(var, cond, tmo)				\
	wait_event_timeout(*__var_waitqueue(var), cond, tmo)
#define	wait_var_event_interruptible(var, cond)				\
	wait_event_interruptible(*__var_waitqueue(var), cond)
#define	wait_var_event_killable(var, cond)				\
	wait_event_killable(*__var_waitqueue(var), cond)

int lkpi_wait_on_bit_timeout(unsigned long *word, int bit, unsigned int mode,
    unsigned long timeout);

static inline int
wait_on_bit(unsigned long *word, int bit, unsigned int mode)
{
	return (lkpi_wait_on_bit_timeout(word, bit, mode,
	    MAX_SCHEDULE_TIMEOUT));
}

static inline int
wait_on_bit_timeout(unsigned long *word, int bit, unsigned int mode,
    unsigned long timeout)
{
	return (lkpi_wait_on_bit_timeout(word, bit, mode, timeout));
}

#define	wait_on_bit_io(w, b, m)		wait_on_bit(w, b, m)

static inline void
clear_and_wake_up_bit(int bit, void *word)
{
	clear_bit_unlock(bit, (unsigned long *)word);
	smp_mb__after_atomic();
	wake_up_bit(word, bit);
}

#endif /* _LINUXKPI_LINUX_WAIT_H_ */
