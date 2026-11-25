/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux timer_list API from its
 * documented behaviour; function set cross-checked against FreeBSD's
 * LinuxKPI timer.h (BSD-2-Clause).
 *
 * Timers run on the KPI timer thread (<linuxkpi/timer_engine.h>), in
 * thread context rather than softirq context: callbacks may take adaptive
 * mutexes, and must not sleep for long because they delay every other
 * timer. expires is in jiffies; resolution is the high-resolution callout,
 * so a timer fires at the first instant jiffies >= expires (not on the
 * next tick interrupt).
 *
 * The flags argument of timer_setup() (TIMER_DEFERRABLE, TIMER_PINNED,
 * TIMER_IRQSAFE) is accepted and ignored. Timer objects need no
 * destruction.
 */
#ifndef _LINUXKPI_LINUX_TIMER_H_
#define	_LINUXKPI_LINUX_TIMER_H_

#include <linuxkpi/platform.h>
#include <linuxkpi/timer_engine.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>

#define	TIMER_DEFERRABLE	0x00080000
#define	TIMER_PINNED		0x00100000
#define	TIMER_IRQSAFE		0x00200000
#define	TIMER_INIT_FLAGS	(TIMER_DEFERRABLE | TIMER_PINNED | TIMER_IRQSAFE)

struct timer_list {
	struct lkpi_timer_node	lkpi_node;
	unsigned long		expires;
	void			(*function)(struct timer_list *);
	u32			flags;
};

bool lkpi_timer_list_run(struct lkpi_timer_node *n, u64 *deadline);

#define	__TIMER_INITIALIZER(fn, fl) {					\
	.lkpi_node = { .run = lkpi_timer_list_run },			\
	.expires = 0, .function = (fn), .flags = (fl) }
#define	DEFINE_TIMER(name, fn)						\
	struct timer_list name = __TIMER_INITIALIZER(fn, 0)

static inline void
timer_setup(struct timer_list *t, void (*fn)(struct timer_list *),
    unsigned int flags)
{
	memset(&t->lkpi_node, 0, sizeof (t->lkpi_node));
	t->lkpi_node.run = lkpi_timer_list_run;
	t->expires = 0;
	t->function = fn;
	t->flags = flags;
}

#define	timer_setup_on_stack(t, fn, fl)	timer_setup(t, fn, fl)
#define	destroy_timer_on_stack(t)	do { (void)(t); } while (0)
#define	from_timer(var, t, field)	container_of(t, typeof(*(var)), field)
#define	timer_container_of(var, t, field) from_timer(var, t, field)

static inline int
timer_pending(const struct timer_list *t)
{
	return (READ_ONCE(t->lkpi_node.queued));
}

int mod_timer(struct timer_list *t, unsigned long expires);
int mod_timer_pending(struct timer_list *t, unsigned long expires);
int timer_reduce(struct timer_list *t, unsigned long expires);
void add_timer(struct timer_list *t);
int del_timer(struct timer_list *t);
int del_timer_sync(struct timer_list *t);
int try_to_del_timer_sync(struct timer_list *t);
int timer_shutdown_sync(struct timer_list *t);
int timer_shutdown(struct timer_list *t);

#define	add_timer_on(t, cpu)		add_timer(t)
#define	timer_delete(t)			del_timer(t)
#define	timer_delete_sync(t)		del_timer_sync(t)
#define	del_singleshot_timer_sync(t)	del_timer_sync(t)

#endif /* _LINUXKPI_LINUX_TIMER_H_ */
