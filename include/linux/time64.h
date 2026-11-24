/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Time unit constants and struct timespec64.
 */
#ifndef _LINUXKPI_LINUX_TIME64_H_
#define	_LINUXKPI_LINUX_TIME64_H_

#include <linux/types.h>
#include <linux/math64.h>

typedef s64 time64_t;

#define	MSEC_PER_SEC	1000L
#define	USEC_PER_MSEC	1000L
#define	NSEC_PER_USEC	1000L
#define	NSEC_PER_MSEC	1000000L
#define	USEC_PER_SEC	1000000L
#define	NSEC_PER_SEC	1000000000L
#define	PSEC_PER_SEC	1000000000000LL
#define	FSEC_PER_SEC	1000000000000000LL
#define	TIME64_MAX	((s64)~((u64)1 << 63))
#define	KTIME_MAX	((s64)~((u64)1 << 63))
#define	KTIME_SEC_MAX	(KTIME_MAX / NSEC_PER_SEC)

struct timespec64 {
	time64_t	tv_sec;
	long		tv_nsec;
};

static inline s64
timespec64_to_ns(const struct timespec64 *ts)
{
	if (ts->tv_sec >= KTIME_SEC_MAX)
		return (KTIME_MAX);
	return (ts->tv_sec * NSEC_PER_SEC + ts->tv_nsec);
}

static inline struct timespec64
ns_to_timespec64(s64 ns)
{
	struct timespec64 ts;
	s64 rem;

	ts.tv_sec = ns / NSEC_PER_SEC;
	rem = ns % NSEC_PER_SEC;
	if (rem < 0) {
		ts.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts.tv_nsec = (long)rem;
	return (ts);
}

static inline struct timespec64
timespec64_sub(struct timespec64 a, struct timespec64 b)
{
	return (ns_to_timespec64(timespec64_to_ns(&a) - timespec64_to_ns(&b)));
}

static inline bool
timespec64_valid(const struct timespec64 *ts)
{
	return (ts->tv_sec >= 0 && ts->tv_nsec >= 0 &&
	    ts->tv_nsec < NSEC_PER_SEC);
}

#endif /* _LINUXKPI_LINUX_TIME64_H_ */
