/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. String helpers (see also <linux/string_choices.h>).
 */
#ifndef _LINUXKPI_LINUX_STRING_HELPERS_H_
#define	_LINUXKPI_LINUX_STRING_HELPERS_H_

#include <linux/types.h>
#include <linux/string.h>
#include <linux/string_choices.h>

enum string_size_units {
	STRING_UNITS_10,
	STRING_UNITS_2
};

/* Human-readable size, e.g. "1.50 MiB"; always NUL-terminates buf. */
static inline int
string_get_size(u64 size, u64 blk_size, enum string_size_units units,
    char *buf, int len)
{
	static const char *const u2[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
	static const char *const u10[] = { "B", "kB", "MB", "GB", "TB", "PB" };
	unsigned int div = units == STRING_UNITS_2 ? 1024 : 1000, i = 0;
	u64 v = size * (blk_size != 0 ? blk_size : 1), rem = 0;

	while (v >= div && i < 5) {
		rem = v % div;
		v /= div;
		i++;
	}
	return (snprintf(buf, (size_t)len, "%llu.%02llu %s",
	    (unsigned long long)v, (unsigned long long)(rem * 100 / div),
	    units == STRING_UNITS_2 ? u2[i] : u10[i]));
}

#endif /* _LINUXKPI_LINUX_STRING_HELPERS_H_ */
