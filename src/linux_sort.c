/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * sort()/sort_r() (see <linux/sort.h>): textbook heapsort. Clean-room.
 */

#include <linux/kernel.h>
#include <linux/sort.h>

struct lkpi_sort_ctx {
	cmp_r_func_t	cmp;
	swap_r_func_t	swapf;
	const void	*priv;
	size_t		size;
};

static void
lkpi_sort_swap_bytes(void *a, void *b, size_t size)
{
	unsigned char *x = a, *y = b, t;

	while (size-- > 0) {
		t = *x;
		*x++ = *y;
		*y++ = t;
	}
}

static void
lkpi_sort_xchg(const struct lkpi_sort_ctx *c, char *a, char *b)
{
	if (c->swapf != NULL)
		c->swapf(a, b, (int)c->size, c->priv);
	else
		lkpi_sort_swap_bytes(a, b, c->size);
}

/* Restore the max-heap property below node i of a heap of n elements. */
static void
lkpi_sift_down(const struct lkpi_sort_ctx *c, char *base, size_t i, size_t n)
{
	size_t child;

	for (;;) {
		child = 2 * i + 1;
		if (child >= n)
			return;
		if (child + 1 < n && c->cmp(base + child * c->size,
		    base + (child + 1) * c->size, c->priv) < 0)
			child++;
		if (c->cmp(base + i * c->size, base + child * c->size,
		    c->priv) >= 0)
			return;
		lkpi_sort_xchg(c, base + i * c->size, base + child * c->size);
		i = child;
	}
}

void
sort_r(void *base, size_t num, size_t size, cmp_r_func_t cmp,
    swap_r_func_t swap, const void *priv)
{
	struct lkpi_sort_ctx c = { cmp, swap, priv, size };
	char *b = base;
	size_t i;

	if (num < 2 || size == 0)
		return;
	for (i = num / 2; i-- > 0; )
		lkpi_sift_down(&c, b, i, num);
	for (i = num - 1; i > 0; i--) {
		lkpi_sort_xchg(&c, b, b + i * size);
		lkpi_sift_down(&c, b, 0, i);
	}
}

struct lkpi_sort_wrap {
	cmp_func_t	cmp;
	swap_func_t	swapf;
};

static int
lkpi_sort_cmp_wrap(const void *a, const void *b, const void *priv)
{
	return (((const struct lkpi_sort_wrap *)priv)->cmp(a, b));
}

static void
lkpi_sort_swap_wrap(void *a, void *b, int size, const void *priv)
{
	((const struct lkpi_sort_wrap *)priv)->swapf(a, b, size);
}

void
sort(void *base, size_t num, size_t size, cmp_func_t cmp, swap_func_t swap)
{
	struct lkpi_sort_wrap w = { cmp, swap };

	sort_r(base, num, size, lkpi_sort_cmp_wrap,
	    swap != NULL ? lkpi_sort_swap_wrap : NULL, &w);
}
