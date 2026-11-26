/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No swap: shrinker-visible counts are zero.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SWAP_H_
#define	_LINUXKPI_INCLUDE_LINUX_SWAP_H_

#include <linux/mm.h>
#include <linux/writeback.h>	/* via memcontrol.h in Linux */

#define	get_nr_swap_pages()	(0L)
#define	total_swap_pages	(0L)
#define	current_is_kswapd()	(false)
#define	mapping_set_unevictable(m)	do { (void)(m); } while (0)

#define	SWAP_CLUSTER_MAX	32UL

#endif /* _LINUXKPI_INCLUDE_LINUX_SWAP_H_ */
