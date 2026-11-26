/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. MMU notifiers are not provided (no userptr support yet, docs/03 MM
 * misc); the types exist so GEM object layouts compile.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_MMU_NOTIFIER_H_
#define	_LINUXKPI_INCLUDE_LINUX_MMU_NOTIFIER_H_

#include <linux/types.h>
#include <linux/errno.h>

struct mm_struct;
struct mmu_interval_notifier;

struct mmu_notifier_range {
	struct mm_struct	*mm;
	unsigned long		start;
	unsigned long		end;
	unsigned int		flags;
	int			event;
};

struct mmu_interval_notifier_ops {
	bool	(*invalidate)(struct mmu_interval_notifier *interval_sub,
		    const struct mmu_notifier_range *range,
		    unsigned long cur_seq);
};

struct mmu_interval_notifier {
	const struct mmu_interval_notifier_ops	*ops;
	struct mm_struct			*mm;
	unsigned long				invalidate_seq;
};

#define	mmu_interval_notifier_insert(s, mm, st, len, ops)	(-ENODEV)
#define	mmu_interval_notifier_remove(s)		do { (void)(s); } while (0)
#define	mmu_interval_read_begin(s)		(0UL)
#define	mmu_interval_read_retry(s, seq)		(false)
#define	mmu_interval_check_retry(s, seq)	(false)
#define	mmu_interval_set_seq(s, seq)		do { (void)(s); } while (0)
#define	mmu_notifier_range_blockable(r)		(true)

#endif /* _LINUXKPI_INCLUDE_LINUX_MMU_NOTIFIER_H_ */
