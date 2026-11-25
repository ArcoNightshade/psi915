/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Binary search over a sorted array; cmp(key, elt) returns
 * <0, 0, >0. Returns a matching element or NULL.
 */
#ifndef _LINUXKPI_LINUX_BSEARCH_H_
#define	_LINUXKPI_LINUX_BSEARCH_H_

#include <linux/types.h>

static inline void *
__inline_bsearch(const void *key, const void *base, size_t num, size_t size,
    int (*cmp)(const void *key, const void *elt))
{
	const char *p = base;
	size_t lo = 0, hi = num, mid;
	int c;

	while (lo < hi) {
		mid = lo + (hi - lo) / 2;
		c = cmp(key, p + mid * size);
		if (c == 0)
			return ((void *)(uintptr_t)(p + mid * size));
		if (c < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return (NULL);
}

#define	bsearch(key, base, num, size, cmp)				\
	__inline_bsearch(key, base, num, size, cmp)

#endif /* _LINUXKPI_LINUX_BSEARCH_H_ */
