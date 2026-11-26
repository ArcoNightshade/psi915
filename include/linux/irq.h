/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. See <linux/interrupt.h>.
 */
#ifndef _LINUXKPI_LINUX_IRQ_H_
#define	_LINUXKPI_LINUX_IRQ_H_

#include <linux/interrupt.h>

/*
 * Virtual IRQ chips (i915 GSC child interrupts, DG1 and later): no
 * irq_desc allocator, so irq_alloc_desc() fails with -ENOMEM and the
 * chip functions are never reached.
 */
struct irq_data {
	unsigned int	irq;
	void		*chip_data;
};

struct irq_chip {
	const char	*name;
	void		(*irq_mask)(struct irq_data *data);
	void		(*irq_unmask)(struct irq_data *data);
	void		(*irq_enable)(struct irq_data *data);
	void		(*irq_disable)(struct irq_data *data);
};

typedef void (*irq_flow_handler_t)(void *desc);

#define	irq_alloc_desc(node)			((void)(node), -ENOMEM)
#define	irq_free_desc(irq)			do { (void)(irq); } while (0)
#define	irq_set_chip_and_handler_name(i, c, h, n)	\
	do { (void)(i); (void)(c); (void)(n); } while (0)
#define	irq_set_chip_data(i, d)			((void)(i), (void)(d), 0)
#define	irq_data_get_irq_chip_data(d)		((d)->chip_data)
#define	handle_simple_irq			NULL

#endif /* _LINUXKPI_LINUX_IRQ_H_ */
