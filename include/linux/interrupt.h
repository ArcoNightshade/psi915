/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux interrupt API from its
 * documented behaviour (Documentation/core-api/genericirq.rst); function
 * set cross-checked against FreeBSD's LinuxKPI interrupt.h (BSD-2-Clause).
 *
 * IRQ numbers are KPI handles for ddi_intr vectors allocated by
 * pci_alloc_irq_vectors()/pci_enable_msi() (src/linux_irq.c). request_irq()
 * attaches the handler with ddi_intr_add_handler(9F) and enables the
 * vector; the illumos handler calls the Linux one and returns
 * DDI_INTR_CLAIMED unless it returned IRQ_NONE. One handler per vector
 * (IRQF_SHARED with a second handler fails with -EBUSY; MSI is never
 * shared).
 *
 * Locking (see also <linux/spinlock.h>): vectors above LOCK_LEVEL are
 * refused (-EINVAL), because spinlock_t is an adaptive mutex. For MSI on
 * x86 the priority is below LOCK_LEVEL, so handlers run as interrupt
 * threads that may take spinlock_t and mutex-backed locks without an
 * iblock cookie. lkpi_irq_iblock(irq) returns the cookie for drivers that
 * want MUTEX_DRIVER locks at the vector's priority
 * (spin_lock_init_irqsafe()).
 *
 * Threaded handlers (IRQ_WAKE_THREAD) and tasklets run on KPI workqueue
 * workers (thread context), irq_work likewise. A tasklet never runs
 * concurrently with itself; tasklet_disable() waits for a running one.
 */
#ifndef _LINUXKPI_LINUX_INTERRUPT_H_
#define	_LINUXKPI_LINUX_INTERRUPT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/irqreturn.h>
#include <linux/hardirq.h>
#include <linux/workqueue.h>
#include <linux/irq_work.h>

typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

#define	IRQF_SHARED		0x00000080
#define	IRQF_PROBE_SHARED	0x00000100
#define	IRQF_TIMER		0x00000200
#define	IRQF_PERCPU		0x00000400
#define	IRQF_NOBALANCING	0x00000800
#define	IRQF_IRQPOLL		0x00001000
#define	IRQF_ONESHOT		0x00002000
#define	IRQF_NO_SUSPEND		0x00004000
#define	IRQF_FORCE_RESUME	0x00008000
#define	IRQF_NO_THREAD		0x00010000
#define	IRQF_EARLY_RESUME	0x00020000
#define	IRQF_COND_SUSPEND	0x00040000
#define	IRQF_NO_AUTOEN		0x00080000
#define	IRQF_TRIGGER_NONE	0x00000000

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
    irq_handler_t thread_fn, unsigned long flags, const char *name,
    void *dev_id);
const void *free_irq(unsigned int irq, void *dev_id);
void disable_irq(unsigned int irq);
void disable_irq_nosync(unsigned int irq);
void enable_irq(unsigned int irq);
void synchronize_irq(unsigned int irq);
void *lkpi_irq_iblock(unsigned int irq);

static inline int
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
    const char *name, void *dev_id)
{
	return (request_threaded_irq(irq, handler, NULL, flags, name, dev_id));
}

struct device;
int devm_request_threaded_irq(struct device *dev, unsigned int irq,
    irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags,
    const char *name, void *dev_id);
#define	devm_request_irq(dev, irq, h, f, n, d)				\
	devm_request_threaded_irq(dev, irq, h, NULL, f, n, d)
void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id);
#define	enable_irq_wake(irq)		(0)
#define	disable_irq_wake(irq)		(0)
#define	irq_set_affinity_hint(i, m)	(0)
#define	disable_hardirq(irq)		(disable_irq_nosync(irq), true)

/* Tasklets. */
enum {
	TASKLET_STATE_SCHED,
	TASKLET_STATE_RUN
};

struct tasklet_struct {
	struct work_struct	lkpi_work;
	unsigned long		state;
	atomic_t		count;
	bool			use_callback;
	union {
		void	(*func)(unsigned long data);
		void	(*callback)(struct tasklet_struct *t);
	};
	unsigned long		data;
};

void lkpi_tasklet_work(struct work_struct *w);

#define	DECLARE_TASKLET(name, cb)					\
	struct tasklet_struct name = {					\
		.lkpi_work = __WORK_INITIALIZER(name.lkpi_work,		\
		    lkpi_tasklet_work),					\
		.count = ATOMIC_INIT(0), .use_callback = true,		\
		.callback = (cb) }
#define	DECLARE_TASKLET_DISABLED(name, cb)				\
	struct tasklet_struct name = {					\
		.lkpi_work = __WORK_INITIALIZER(name.lkpi_work,		\
		    lkpi_tasklet_work),					\
		.count = ATOMIC_INIT(1), .use_callback = true,		\
		.callback = (cb) }
#define	from_tasklet(var, t, field)	container_of(t, typeof(*(var)), field)

void tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long),
    unsigned long data);
void tasklet_setup(struct tasklet_struct *t,
    void (*callback)(struct tasklet_struct *));
void tasklet_schedule(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *t);
void tasklet_disable_nosync(struct tasklet_struct *t);
void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);
bool tasklet_trylock(struct tasklet_struct *t);
void tasklet_unlock(struct tasklet_struct *t);
void tasklet_unlock_wait(struct tasklet_struct *t);
#define	tasklet_hi_schedule(t)		tasklet_schedule(t)
#define	tasklet_disable_in_atomic(t)	tasklet_disable(t)

/*
 * tasklet_is_locked(), __tasklet_is_enabled() and __tasklet_is_scheduled()
 * are not Linux API: i915 defines them itself (i915_tasklet.h).
 */

#define	local_softirq_pending()		(0)

#endif /* _LINUXKPI_LINUX_INTERRUPT_H_ */
