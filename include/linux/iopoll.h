/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Register polling helpers (Linux semantics): evaluate op(args) into val
 * until cond is true or timeout_us elapses (0 = forever); sleep_us between
 * reads (0 = busy). Returns 0 or -ETIMEDOUT, with val holding the last
 * value read.
 */
#ifndef _LINUXKPI_LINUX_IOPOLL_H_
#define	_LINUXKPI_LINUX_IOPOLL_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/io.h>

#define	read_poll_timeout(op, val, cond, sleep_us, timeout_us,		\
    sleep_before_read, args...) ({					\
	u64 __to = (u64)(timeout_us);					\
	unsigned long __sl = (sleep_us);				\
	ktime_t __end = ktime_add_us(ktime_get(), __to);		\
	int __r = 0;							\
	if (sleep_before_read && __sl)					\
		usleep_range((__sl >> 2) + 1, __sl);			\
	for (;;) {							\
		(val) = op(args);					\
		if (cond)						\
			break;						\
		if (__to && ktime_compare(ktime_get(), __end) > 0) {	\
			(val) = op(args);				\
			__r = (cond) ? 0 : -ETIMEDOUT;			\
			break;						\
		}							\
		if (__sl)						\
			usleep_range((__sl >> 2) + 1, __sl);		\
		else							\
			lkpi_cpu_relax();				\
	}								\
	__r;								\
})
#define	read_poll_timeout_atomic(op, val, cond, delay_us, timeout_us,	\
    delay_before_read, args...) ({					\
	u64 __to = (u64)(timeout_us);					\
	unsigned long __dl = (delay_us);				\
	ktime_t __end = ktime_add_us(ktime_get(), __to);		\
	int __r = 0;							\
	if (delay_before_read && __dl)					\
		udelay(__dl);						\
	for (;;) {							\
		(val) = op(args);					\
		if (cond)						\
			break;						\
		if (__to && ktime_compare(ktime_get(), __end) > 0) {	\
			(val) = op(args);				\
			__r = (cond) ? 0 : -ETIMEDOUT;			\
			break;						\
		}							\
		if (__dl)						\
			udelay(__dl);					\
		else							\
			lkpi_cpu_relax();				\
	}								\
	__r;								\
})
#define	readx_poll_timeout(op, addr, val, cond, sleep_us, timeout_us)	\
	read_poll_timeout(op, val, cond, sleep_us, timeout_us, false, addr)
#define	readx_poll_timeout_atomic(op, addr, val, cond, d, t)		\
	read_poll_timeout_atomic(op, val, cond, d, t, false, addr)
#define	readl_poll_timeout(addr, val, cond, s, t)			\
	readx_poll_timeout(readl, addr, val, cond, s, t)
#define	readl_poll_timeout_atomic(addr, val, cond, d, t)		\
	readx_poll_timeout_atomic(readl, addr, val, cond, d, t)

#endif /* _LINUXKPI_LINUX_IOPOLL_H_ */
