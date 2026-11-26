/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Memory shrinkers. Registered shrinkers are kept on a list and called by
 * lkpi_shrinkers_run() (src/linux_shrinker.c); hooking that to illumos
 * memory pressure (a kmem reclaim callback) is P5 work, so today they
 * run only when the KPI is asked to reclaim.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SHRINKER_H_
#define	_LINUXKPI_INCLUDE_LINUX_SHRINKER_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/gfp.h>

#define	SHRINK_STOP		(~0UL)
#define	SHRINK_EMPTY		(~0UL - 1)
#define	DEFAULT_SEEKS		2
#define	SHRINKER_NUMA_AWARE	(1 << 2)

struct shrink_control {
	gfp_t		gfp_mask;
	int		nid;
	unsigned long	nr_to_scan;
	unsigned long	nr_scanned;
};

struct shrinker {
	unsigned long	(*count_objects)(struct shrinker *, struct shrink_control *sc);
	unsigned long	(*scan_objects)(struct shrinker *, struct shrink_control *sc);
	long		batch;
	int		seeks;
	unsigned int	flags;
	void		*private_data;
	struct list_head lkpi_entry;
};

struct shrinker *shrinker_alloc(unsigned int flags, const char *fmt, ...)
    __printf(2, 3);
void shrinker_register(struct shrinker *shrinker);
void shrinker_free(struct shrinker *shrinker);
/* Ask every shrinker for up to nr objects; returns objects freed. */
unsigned long lkpi_shrinkers_run(unsigned long nr, gfp_t gfp);

#endif /* _LINUXKPI_INCLUDE_LINUX_SHRINKER_H_ */
