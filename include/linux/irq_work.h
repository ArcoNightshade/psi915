/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. irq_work runs its function on a KPI workqueue worker
 * (thread context) soon after irq_work_queue(), which is safe from
 * interrupt handlers below LOCK_LEVEL. irq_work_queue() on a pending item
 * returns false.
 */
#ifndef _LINUXKPI_LINUX_IRQ_WORK_H_
#define	_LINUXKPI_LINUX_IRQ_WORK_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/llist.h>

struct irq_work {
	/* Linux-visible: callers keep irq_works on llists via node.llist. */
	struct {
		struct llist_node	llist;
		unsigned int		u_flags;
	} node;
	struct work_struct	lkpi_work;
	void			(*func)(struct irq_work *w);
};

void lkpi_irq_work_fn(struct work_struct *w);

#define	IRQ_WORK_INIT(fn) {						\
	.lkpi_work = { .data = ATOMIC_LONG_INIT(0),			\
	    .func = lkpi_irq_work_fn }, .func = (fn) }
#define	IRQ_WORK_INIT_HARD(fn)	IRQ_WORK_INIT(fn)
#define	DEFINE_IRQ_WORK(name, fn) struct irq_work name = IRQ_WORK_INIT(fn)

static inline void
init_irq_work(struct irq_work *w, void (*fn)(struct irq_work *))
{
	INIT_WORK(&w->lkpi_work, lkpi_irq_work_fn);
	w->func = fn;
}

static inline bool
irq_work_queue(struct irq_work *w)
{
	return (queue_work(system_highpri_wq, &w->lkpi_work));
}

#define	irq_work_queue_on(w, cpu)	irq_work_queue(w)

static inline void
irq_work_sync(struct irq_work *w)
{
	(void) flush_work(&w->lkpi_work);
}

static inline bool
irq_work_is_pending(struct irq_work *w)
{
	return (work_pending(&w->lkpi_work));
}

#define	irq_work_is_busy(w)	irq_work_is_pending(w)

#endif /* _LINUXKPI_LINUX_IRQ_WORK_H_ */
