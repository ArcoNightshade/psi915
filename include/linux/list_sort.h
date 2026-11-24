/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * list_sort() API: a stable sort of a struct list_head list. cmp returns
 * > 0 if a must come after b; equal elements keep their order. Runs in
 * O(n log n) time without allocating (src/linux_list_sort.c).
 */
#ifndef _LINUXKPI_LINUX_LIST_SORT_H_
#define	_LINUXKPI_LINUX_LIST_SORT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>

typedef int (*list_cmp_func_t)(void *priv, const struct list_head *a,
    const struct list_head *b);

void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp);

#endif /* _LINUXKPI_LINUX_LIST_SORT_H_ */
