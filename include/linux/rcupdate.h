/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux RCU
 * API (Documentation/RCU/whatisRCU.rst), with an epoch-based grace period
 * emulation (src/linux_rcu.c):
 *
 *   rcu_read_lock()   the outermost call records the current global epoch
 *                     in a per-thread reader slot; nested calls only count
 *   synchronize_rcu() advances the epoch and waits until no reader slot
 *                     holds an older epoch, i.e. every read-side critical
 *                     section that began before the call has ended
 *   call_rcu()        queues the callback for a service thread that runs
 *                     batches after a synchronize_rcu()
 *
 * Reader slots are keyed by thread (not CPU), so a reader that blocks,
 * e.g. on a contended spinlock_t (an adaptive mutex here), or migrates,
 * stays correct. Read-side sections may therefore block, but as in Linux
 * they must not wait for a grace period (synchronize_rcu() inside a
 * read-side section warns and returns). Readers in interrupt handlers
 * above LOCK_LEVEL are not supported. Readers pay one atomic operation on
 * a hashed slot plus a full barrier; the cost is higher than Linux's.
 *
 * kfree_rcu()/kvfree_rcu() free through call_rcu(). SRCU is not provided.
 */
#ifndef _LINUXKPI_LINUX_RCUPDATE_H_
#define	_LINUXKPI_LINUX_RCUPDATE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <asm/barrier.h>

void rcu_read_lock(void);
void rcu_read_unlock(void);
bool rcu_read_lock_held(void);
void synchronize_rcu(void);
void call_rcu(struct rcu_head *head, rcu_callback_t func);
void rcu_barrier(void);
unsigned long get_state_synchronize_rcu(void);
bool poll_state_synchronize_rcu(unsigned long cookie);
void cond_synchronize_rcu(unsigned long cookie);

#define	rcu_read_lock_bh()		rcu_read_lock()
#define	rcu_read_unlock_bh()		rcu_read_unlock()
#define	rcu_read_lock_sched()		rcu_read_lock()
#define	rcu_read_unlock_sched()		rcu_read_unlock()
#define	rcu_read_lock_sched_notrace()	rcu_read_lock()
#define	rcu_read_unlock_sched_notrace()	rcu_read_unlock()
#define	rcu_read_lock_bh_held()		rcu_read_lock_held()
#define	rcu_read_lock_sched_held()	rcu_read_lock_held()
#define	rcu_read_lock_any_held()	rcu_read_lock_held()
#define	synchronize_rcu_expedited()	synchronize_rcu()
#define	synchronize_rcu_tasks()		synchronize_rcu()
#define	synchronize_net()		synchronize_rcu()
#define	call_rcu_hurry(head, func)	call_rcu(head, func)
#define	start_poll_synchronize_rcu()	get_state_synchronize_rcu()
#define	rcu_is_watching()		(true)
#define	rcu_lockdep_assert(c, s)	do { } while (0)
#define	RCU_LOCKDEP_WARN(c, s)		do { } while (0)
#define	rcu_sleep_check()		do { } while (0)

#define	init_rcu_head(head)		do { } while (0)
#define	destroy_rcu_head(head)		do { } while (0)
#define	init_rcu_head_on_stack(head)	do { } while (0)
#define	destroy_rcu_head_on_stack(head)	do { } while (0)

/*
 * Pointer publication. Readers rely on address dependencies, which every
 * supported CPU (amd64) orders, so rcu_dereference() is a READ_ONCE().
 * The lockdep condition c of the _check/_protected forms is type-checked
 * but not evaluated.
 */
#define	rcu_dereference_raw(p)		READ_ONCE(p)
#define	rcu_dereference(p)		READ_ONCE(p)
#define	rcu_dereference_bh(p)		READ_ONCE(p)
#define	rcu_dereference_sched(p)	READ_ONCE(p)
#define	rcu_dereference_check(p, c)	({ (void)sizeof(!!(c)); READ_ONCE(p); })
#define	rcu_dereference_bh_check(p, c)	rcu_dereference_check(p, c)
#define	rcu_dereference_sched_check(p, c) rcu_dereference_check(p, c)
#define	rcu_dereference_raw_check(p)	READ_ONCE(p)
#define	rcu_dereference_protected(p, c)	({ (void)sizeof(!!(c)); (p); })
#define	rcu_access_pointer(p)		READ_ONCE(p)
#define	rcu_pointer_handoff(p)		(p)
#define	unrcu_pointer(p)		(p)

#define	RCU_INITIALIZER(v)		(v)
#define	RCU_INIT_POINTER(p, v)		WRITE_ONCE(p, v)
#define	rcu_assign_pointer(p, v)					\
	smp_store_release(&(p), (typeof(p))(v))
#define	rcu_replace_pointer(rcu_ptr, ptr, c) ({				\
	typeof(ptr) __lkpi_rrp = rcu_dereference_protected((rcu_ptr), (c)); \
	rcu_assign_pointer((rcu_ptr), (ptr));				\
	__lkpi_rrp;							\
})

/*
 * kfree_rcu(ptr, rhf) frees ptr (kmalloc/kvmalloc memory) after a grace
 * period, using its struct rcu_head member rhf. The one-argument form
 * (kfree_rcu_mightsleep) waits for the grace period in place.
 */
void lkpi_kvfree_rcu(struct rcu_head *head, size_t offset);

#define	kvfree_rcu_mightsleep(ptr) do {					\
	synchronize_rcu();						\
	kvfree(ptr);							\
} while (0)
#define	kfree_rcu_mightsleep(ptr)	kvfree_rcu_mightsleep(ptr)

#define	__lkpi_kvfree_rcu2(ptr, rhf) do {				\
	typeof(ptr) __lkpi_kfr = (ptr);					\
									\
	if (__lkpi_kfr != NULL)						\
		lkpi_kvfree_rcu(&__lkpi_kfr->rhf,			\
		    offsetof(typeof(*__lkpi_kfr), rhf));		\
} while (0)
#define	__lkpi_kvfree_rcu1(ptr)		kvfree_rcu_mightsleep(ptr)
#define	__lkpi_kvfree_rcu_pick(_1, _2, name, ...)	name
#define	kvfree_rcu(...)							\
	__lkpi_kvfree_rcu_pick(__VA_ARGS__, __lkpi_kvfree_rcu2,		\
	    __lkpi_kvfree_rcu1, _unused)(__VA_ARGS__)
#define	kfree_rcu(...)			kvfree_rcu(__VA_ARGS__)

/* Test hook: claim overflow reader slots only (src/linux_rcu.c). */
extern int lkpi_rcu_test_force_overflow;

#endif /* _LINUXKPI_LINUX_RCUPDATE_H_ */
