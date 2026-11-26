/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * TEST-ONLY stand-in for the KPI's <linux/dma-fence.h> (requested change
 * R10), with just the API subset src/sync_file.c uses and Linux's
 * documented semantics for it: callbacks run once, under the fence lock,
 * at signal time; add_callback returns -ENOENT when already signaled;
 * remove_callback returns whether the callback was still pending.
 * Never used for kernel builds that are meant to run.
 */
#ifndef _LINUXKPI_LINUX_DMA_FENCE_H_
#define	_LINUXKPI_LINUX_DMA_FENCE_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct dma_fence;
struct dma_fence_cb;
typedef void (*dma_fence_func_t)(struct dma_fence *, struct dma_fence_cb *);

struct dma_fence_cb {
	struct list_head	node;
	dma_fence_func_t	func;
};

struct dma_fence_ops {
	const char	*(*get_driver_name)(struct dma_fence *);
	const char	*(*get_timeline_name)(struct dma_fence *);
};

struct dma_fence {
	spinlock_t		lock;
	const struct dma_fence_ops *ops;
	struct list_head	cb_list;
	struct kref		refcount;
	uint64_t		seqno;
	bool			signaled;
	int			error;
};

static inline void
dma_fence_init(struct dma_fence *f, const struct dma_fence_ops *ops,
    uint64_t seqno)
{
	spin_lock_init(&f->lock);
	f->ops = ops;
	INIT_LIST_HEAD(&f->cb_list);
	kref_init(&f->refcount);
	f->seqno = seqno;
	f->signaled = false;
	f->error = 0;
}

static inline struct dma_fence *
dma_fence_get(struct dma_fence *f)
{
	if (f != NULL)
		kref_get(&f->refcount);
	return (f);
}

static inline void
dma_fence_release_fake(struct kref *k)
{
	kfree(container_of(k, struct dma_fence, refcount));
}

static inline void
dma_fence_put(struct dma_fence *f)
{
	if (f != NULL)
		kref_put(&f->refcount, dma_fence_release_fake);
}

static inline bool
dma_fence_is_signaled(struct dma_fence *f)
{
	bool s;

	spin_lock(&f->lock);
	s = f->signaled;
	spin_unlock(&f->lock);
	return (s);
}

static inline int
dma_fence_get_status(struct dma_fence *f)
{
	if (!dma_fence_is_signaled(f))
		return (0);
	return (f->error != 0 ? f->error : 1);
}

static inline int
dma_fence_add_callback(struct dma_fence *f, struct dma_fence_cb *cb,
    dma_fence_func_t fn)
{
	int r = 0;

	spin_lock(&f->lock);
	if (f->signaled) {
		INIT_LIST_HEAD(&cb->node);
		r = -ENOENT;
	} else {
		cb->func = fn;
		list_add_tail(&cb->node, &f->cb_list);
	}
	spin_unlock(&f->lock);
	return (r);
}

static inline bool
dma_fence_remove_callback(struct dma_fence *f, struct dma_fence_cb *cb)
{
	bool pending;

	spin_lock(&f->lock);
	pending = !list_empty(&cb->node);
	if (pending)
		list_del_init(&cb->node);
	spin_unlock(&f->lock);
	return (pending);
}

static inline void
dma_fence_signal(struct dma_fence *f)
{
	struct dma_fence_cb *cb, *tmp;

	spin_lock(&f->lock);
	f->signaled = true;
	list_for_each_entry_safe(cb, tmp, &f->cb_list, node) {
		list_del_init(&cb->node);
		cb->func(f, cb);
	}
	spin_unlock(&f->lock);
}

#endif /* _LINUXKPI_LINUX_DMA_FENCE_H_ */
