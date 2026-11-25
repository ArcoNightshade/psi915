/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Internal: the timer engine shared by timer_list and hrtimer
 * (src/linux_timer.c). Not a Linux header.
 *
 * Pending timers sit in one red-black tree ordered by absolute deadline
 * (lkpi_plat_nsec() scale, FIFO among equal deadlines). One service
 * thread sleeps until the earliest deadline (high-resolution callout),
 * dequeues expired timers and runs their callbacks one at a time without
 * any lock held, in thread context. While a callback runs, the engine
 * records which timer it belongs to, so the _sync cancel operations can
 * wait for it.
 */
#ifndef _LINUXKPI_TIMER_ENGINE_H_
#define	_LINUXKPI_TIMER_ENGINE_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/rbtree.h>

struct lkpi_timer_node;
/*
 * Runs the callback; returns true to re-arm at *deadline (hrtimer
 * restart). The engine applies the new deadline under its lock.
 */
typedef bool (*lkpi_timer_run_t)(struct lkpi_timer_node *node,
    u64 *deadline);

struct lkpi_timer_node {
	struct rb_node		rb;
	u64			deadline;	/* absolute ns */
	lkpi_timer_run_t	run;
	bool			queued;
	bool			shutdown;	/* timer_shutdown*(): never re-arm */
};

/* Arm (or re-arm) at deadline. Returns whether it was queued before. */
bool lkpi_timer_arm(struct lkpi_timer_node *n, u64 deadline);
/* Dequeue. Returns whether it was queued. */
bool lkpi_timer_disarm(struct lkpi_timer_node *n);
/*
 * Dequeue and wait until the callback is not running (unless called from
 * the callback itself); repeats if the callback re-armed the timer.
 * Returns whether it was queued at the first check. shutdown: also block
 * future arming.
 */
bool lkpi_timer_disarm_sync(struct lkpi_timer_node *n, bool shutdown);
/* Dequeue unless the callback is running: 1 dequeued, 0 idle, -1 running. */
int lkpi_timer_try_disarm(struct lkpi_timer_node *n);
bool lkpi_timer_running(const struct lkpi_timer_node *n);
/* Absolute deadline converted from jiffies. */
u64 lkpi_timer_jiffies_to_deadline(unsigned long expires);

#endif /* _LINUXKPI_TIMER_ENGINE_H_ */
