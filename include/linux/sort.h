/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. sort()/sort_r(): in-place heapsort (O(n log n), no extra
 * memory, not stable), as documented for Linux. swap may be NULL for a
 * generic byte swap.
 */
#ifndef _LINUXKPI_LINUX_SORT_H_
#define	_LINUXKPI_LINUX_SORT_H_

#include <linux/types.h>

typedef int (*cmp_func_t)(const void *a, const void *b);
typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);
typedef void (*swap_func_t)(void *a, void *b, int size);
typedef void (*swap_r_func_t)(void *a, void *b, int size, const void *priv);

void sort_r(void *base, size_t num, size_t size, cmp_r_func_t cmp,
    swap_r_func_t swap, const void *priv);
void sort(void *base, size_t num, size_t size, cmp_func_t cmp,
    swap_func_t swap);

#endif /* _LINUXKPI_LINUX_SORT_H_ */
