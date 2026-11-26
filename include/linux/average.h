/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Exponentially weighted moving average, DECLARE_EWMA(name, precision,
 * weight_rcp): the internal value keeps `precision` fractional bits and
 * each sample moves it by 1/weight_rcp (a power of two) of the difference,
 * as documented for Linux. The first sample initialises the average.
 */
#ifndef _LINUXKPI_LINUX_AVERAGE_H_
#define	_LINUXKPI_LINUX_AVERAGE_H_

#include <linux/types.h>
#include <linux/log2.h>
#include <linux/compiler.h>
#include <linux/build_bug.h>

#define	DECLARE_EWMA(name, _precision, _weight_rcp)			\
	struct ewma_##name {						\
		unsigned long internal;					\
	};								\
	static inline void						\
	ewma_##name##_init(struct ewma_##name *e)			\
	{								\
		e->internal = 0;					\
	}								\
	static inline unsigned long					\
	ewma_##name##_read(struct ewma_##name *e)			\
	{								\
		return (READ_ONCE(e->internal) >> (_precision));	\
	}								\
	static inline void						\
	ewma_##name##_add(struct ewma_##name *e, unsigned long val)	\
	{								\
		unsigned long in = READ_ONCE(e->internal);		\
		unsigned long w = ilog2(_weight_rcp);			\
									\
		WRITE_ONCE(e->internal, in != 0 ?			\
		    (((in << w) - in) + (val << (_precision))) >> w :	\
		    (val << (_precision)));				\
	}

#endif /* _LINUXKPI_LINUX_AVERAGE_H_ */
