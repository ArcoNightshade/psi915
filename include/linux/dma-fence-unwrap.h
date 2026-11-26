/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. dma_fence_unwrap. Clean-room declaration of the Linux 6.14 interface (member names and
 * order as upstream, because the GPL implementation in gpl/drivers/dma-buf
 * and drivers use them directly); the out-of-line functions live in the
 * separate GPL module (docs/08 5, docs/10 R10). Inline helpers are this
 * project's own, written from the documented behaviour.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_UNWRAP_H_
#define	_LINUXKPI_INCLUDE_LINUX_DMA_FENCE_UNWRAP_H_

#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence-chain.h>

struct dma_fence_unwrap {
	struct dma_fence	*chain;
	struct dma_fence	*array;
	unsigned int		index;
};

struct dma_fence *dma_fence_unwrap_first(struct dma_fence *head,
    struct dma_fence_unwrap *cursor);
struct dma_fence *dma_fence_unwrap_next(struct dma_fence_unwrap *cursor);
struct dma_fence *__dma_fence_unwrap_merge(unsigned int num_fences,
    struct dma_fence **fences, struct dma_fence_unwrap *cursors);

#define	dma_fence_unwrap_for_each(fence, cursor, head)			\
	for ((fence) = dma_fence_unwrap_first(head, cursor); (fence) != NULL; \
	    (fence) = dma_fence_unwrap_next(cursor))

#define	dma_fence_unwrap_merge(...) ({					\
	struct dma_fence *__f[] = { __VA_ARGS__ };			\
	struct dma_fence_unwrap __c[ARRAY_SIZE(__f)];			\
	__dma_fence_unwrap_merge(ARRAY_SIZE(__f), __f, __c);		\
})

#endif /* _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_UNWRAP_H_ */
