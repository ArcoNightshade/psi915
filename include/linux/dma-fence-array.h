/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. dma_fence_array. Clean-room declaration of the Linux 6.14 interface (member names and
 * order as upstream, because the GPL implementation in gpl/drivers/dma-buf
 * and drivers use them directly); the out-of-line functions live in the
 * separate GPL module (docs/08 5, docs/10 R10). Inline helpers are this
 * project's own, written from the documented behaviour.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_ARRAY_H_
#define	_LINUXKPI_INCLUDE_LINUX_DMA_FENCE_ARRAY_H_

#include <linux/dma-fence.h>
#include <linux/irq_work.h>

struct dma_fence_array;

struct dma_fence_array_cb {
	struct dma_fence_cb	cb;
	struct dma_fence_array	*array;
};

struct dma_fence_array {
	struct dma_fence		base;
	spinlock_t			lock;
	unsigned int			num_fences;
	atomic_t			num_pending;
	struct dma_fence		**fences;
	struct irq_work			work;
	struct dma_fence_array_cb	callbacks[] __counted_by(num_fences);
};

extern const struct dma_fence_ops dma_fence_array_ops;

static inline bool
dma_fence_is_array(struct dma_fence *fence)
{
	return (fence->ops == &dma_fence_array_ops);
}

static inline struct dma_fence_array *
to_dma_fence_array(struct dma_fence *fence)
{
	if (fence == NULL || !dma_fence_is_array(fence))
		return (NULL);
	return (container_of(fence, struct dma_fence_array, base));
}

#define	dma_fence_array_for_each(fence, index, head)			\
	for ((index) = 0, (fence) = dma_fence_array_first(head);	\
	    (fence) != NULL;						\
	    ++(index), (fence) = dma_fence_array_next(head, index))

struct dma_fence_array *dma_fence_array_alloc(int num_fences);
void dma_fence_array_init(struct dma_fence_array *array, int num_fences,
    struct dma_fence **fences, u64 context, unsigned int seqno,
    bool signal_on_any);
struct dma_fence_array *dma_fence_array_create(int num_fences,
    struct dma_fence **fences, u64 context, unsigned int seqno,
    bool signal_on_any);
bool dma_fence_match_context(struct dma_fence *fence, u64 context);
struct dma_fence *dma_fence_array_first(struct dma_fence *head);
struct dma_fence *dma_fence_array_next(struct dma_fence *head,
    unsigned int index);

#endif /* _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_ARRAY_H_ */
