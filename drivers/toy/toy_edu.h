/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * toy_edu results, shared with tests/test_toy.c.
 */
#ifndef _TOY_EDU_H_
#define	_TOY_EDU_H_

#define	TOY_EDU_VENDOR	0x1234
#define	TOY_EDU_DEVICE	0x11e8

struct toy_result {
	bool		probed;
	bool		removed;
	int		selftest;
	unsigned int	id;
	unsigned int	fact;
	bool		live_ok;
	bool		irq_ok;
	bool		fact_ok;
	bool		dma_ok;
	bool		pool_ok;
	int		irqs;
};

extern struct toy_result toy_last_result;
int lkpi_initcall_toy_init(void);
void lkpi_exitcall_toy_exit(void);

#endif /* _TOY_EDU_H_ */
