/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. dma_fence_chain. Clean-room declaration of the Linux 6.14 interface (member names and
 * order as upstream, because the GPL implementation in gpl/drivers/dma-buf
 * and drivers use them directly); the out-of-line functions live in the
 * separate GPL module (docs/08 5, docs/10 R10). Inline helpers are this
 * project's own, written from the documented behaviour.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_CHAIN_H_
#define	_LINUXKPI_INCLUDE_LINUX_DMA_FENCE_CHAIN_H_

#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/irq_work.h>
#include <linux/slab.h>

struct dma_fence_chain {
	struct dma_fence	base;
	struct dma_fence __rcu	*prev;
	u64			prev_seqno;
	struct dma_fence	*fence;
	union {
		struct dma_fence_cb	cb;
		struct irq_work		work;
	};
	spinlock_t		lock;
};

extern const struct dma_fence_ops dma_fence_chain_ops;

static inline struct dma_fence_chain *
to_dma_fence_chain(struct dma_fence *fence)
{
	if (fence == NULL || fence->ops != &dma_fence_chain_ops)
		return (NULL);
	return (container_of(fence, struct dma_fence_chain, base));
}

static inline bool
dma_fence_is_chain(struct dma_fence *fence)
{
	return (fence->ops == &dma_fence_chain_ops);
}

static inline struct dma_fence *
dma_fence_chain_contained(struct dma_fence *fence)
{
	struct dma_fence_chain *chain = to_dma_fence_chain(fence);

	return (chain != NULL ? chain->fence : fence);
}

#define	dma_fence_chain_alloc()						\
	((struct dma_fence_chain *)kmalloc(sizeof (struct dma_fence_chain), \
	    GFP_KERNEL))
#define	dma_fence_chain_free(chain)	kfree(chain)

struct dma_fence *dma_fence_chain_walk(struct dma_fence *fence);
int dma_fence_chain_find_seqno(struct dma_fence **pfence, uint64_t seqno);
void dma_fence_chain_init(struct dma_fence_chain *chain,
    struct dma_fence *prev, struct dma_fence *fence, uint64_t seqno);

#define	dma_fence_chain_for_each(iter, head)				\
	for ((iter) = dma_fence_get(head); (iter) != NULL;		\
	    (iter) = dma_fence_chain_walk(iter))

#endif /* _LINUXKPI_INCLUDE_LINUX_DMA_FENCE_CHAIN_H_ */
