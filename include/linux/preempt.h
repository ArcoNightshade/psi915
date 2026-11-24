/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Preemption and interrupt-context queries. The illumos kernel
 * is fully preemptive and KPI locks never rely on preemption being
 * disabled, so preempt_disable() and friends are compiler barriers only.
 * in_interrupt()/in_irq()/in_hardirq() report whether the caller is an
 * interrupt handler (servicing_interrupt()); softirq context does not
 * exist (tasklets run in a soft interrupt thread, see <linux/interrupt.h>).
 */
#ifndef _LINUXKPI_LINUX_PREEMPT_H_
#define	_LINUXKPI_LINUX_PREEMPT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>

#define	preempt_disable()		barrier()
#define	preempt_enable()		barrier()
#define	preempt_enable_no_resched()	barrier()
#define	preempt_disable_notrace()	barrier()
#define	preempt_enable_notrace()	barrier()
#define	preempt_count()			(0)
#define	preemptible()			(!lkpi_plat_in_interrupt())
#define	migrate_disable()		barrier()
#define	migrate_enable()		barrier()

#define	in_interrupt()			(lkpi_plat_in_interrupt())
#define	in_irq()			(lkpi_plat_in_interrupt())
#define	in_hardirq()			(lkpi_plat_in_interrupt())
#define	in_softirq()			(0)
#define	in_serving_softirq()		(0)
#define	in_nmi()			(0)
#define	in_task()			(!lkpi_plat_in_interrupt())
#define	in_atomic()			(lkpi_plat_in_interrupt())

#endif /* _LINUXKPI_LINUX_PREEMPT_H_ */
