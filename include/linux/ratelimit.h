/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel ratelimit API: at most burst events per interval jiffies.
 * Unlocked: concurrent callers may let a few extra events through, which
 * is harmless for log rate limiting.
 */
#ifndef _LINUXKPI_LINUX_RATELIMIT_H_
#define	_LINUXKPI_LINUX_RATELIMIT_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/jiffies.h>

#define	DEFAULT_RATELIMIT_INTERVAL_SEC	5
#define	DEFAULT_RATELIMIT_INTERVAL	(5 * HZ)
#define	DEFAULT_RATELIMIT_BURST		10

/* interval == 0 in the initialiser means the default (resolved at use). */
struct ratelimit_state {
	int		interval;
	int		burst;
	int		printed;
	int		missed;
	unsigned long	begin;
};

#define	RATELIMIT_STATE_INIT(name, i, b)	{ .interval = (i), .burst = (b) }
#define	RATELIMIT_STATE_INIT_DISABLED	{ .interval = -1 }
#define	DEFINE_RATELIMIT_STATE(name, i, b)				\
	struct ratelimit_state name = RATELIMIT_STATE_INIT(name, i, b)

static inline void
ratelimit_state_init(struct ratelimit_state *rs, int interval, int burst)
{
	rs->interval = interval;
	rs->burst = burst;
	rs->printed = rs->missed = 0;
	rs->begin = 0;
}

/* Returns nonzero if the event may proceed. */
static inline int
__ratelimit(struct ratelimit_state *rs)
{
	unsigned long now = jiffies;
	unsigned long interval;

	if (rs->interval < 0 || rs->burst <= 0)
		return (rs->interval < 0);
	interval = rs->interval != 0 ? (unsigned long)rs->interval :
	    (unsigned long)DEFAULT_RATELIMIT_INTERVAL;
	if (rs->begin == 0 || time_after_eq(now, rs->begin + interval)) {
		rs->begin = now != 0 ? now : 1;
		rs->printed = 0;
	}
	if (rs->printed < rs->burst) {
		rs->printed++;
		return (1);
	}
	rs->missed++;
	return (0);
}

#define	ratelimit(rs)	__ratelimit(rs)

#endif /* _LINUXKPI_LINUX_RATELIMIT_H_ */
