/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room declaration of the Linux 6.14 dma-fence interface, written
 * from its documentation (Documentation/driver-api/dma-buf.rst, the
 * kernel-doc of each function) so that the upstream implementation
 * (gpl/drivers/dma-buf/dma-fence.c, built in the separate GPL module) and
 * its users compile against the KPI. Member names and order follow
 * upstream v6.14 because the GPL implementation and drivers access them
 * directly. The inline helpers here are this project's own.
 *
 * The out-of-line functions are provided by the GPL module (docs/08 5,
 * docs/10 R10), not by linuxkpi.
 */
#ifndef _LINUXKPI_LINUX_DMA_FENCE_H_
#define	_LINUXKPI_LINUX_DMA_FENCE_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/err.h>

struct dma_fence;
struct dma_fence_ops;
struct dma_fence_cb;
struct seq_file;

struct dma_fence {
	spinlock_t			*lock;
	const struct dma_fence_ops	*ops;
	union {
		struct list_head	cb_list;	/* until signaled */
		ktime_t			timestamp;	/* when signaled */
		struct rcu_head		rcu;		/* when freed */
	};
	u64				context;
	u64				seqno;
	unsigned long			flags;
	struct kref			refcount;
	int				error;
};

enum dma_fence_flag_bits {
	DMA_FENCE_FLAG_SIGNALED_BIT,
	DMA_FENCE_FLAG_TIMESTAMP_BIT,
	DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	DMA_FENCE_FLAG_USER_BITS
};

typedef void (*dma_fence_func_t)(struct dma_fence *fence,
    struct dma_fence_cb *cb);

struct dma_fence_cb {
	struct list_head	node;
	dma_fence_func_t	func;
};

struct dma_fence_ops {
	bool		use_64bit_seqno;
	const char	*(*get_driver_name)(struct dma_fence *fence);
	const char	*(*get_timeline_name)(struct dma_fence *fence);
	bool		(*enable_signaling)(struct dma_fence *fence);
	bool		(*signaled)(struct dma_fence *fence);
	signed long	(*wait)(struct dma_fence *fence, bool intr,
			    signed long timeout);
	void		(*release)(struct dma_fence *fence);
	void		(*fence_value_str)(struct dma_fence *fence, char *str,
			    int size);
	void		(*timeline_value_str)(struct dma_fence *fence,
			    char *str, int size);
	void		(*set_deadline)(struct dma_fence *fence,
			    ktime_t deadline);
};

void dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
    spinlock_t *lock, u64 context, u64 seqno);
void dma_fence_release(struct kref *kref);
void dma_fence_free(struct dma_fence *fence);
void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq);
int dma_fence_signal(struct dma_fence *fence);
int dma_fence_signal_locked(struct dma_fence *fence);
int dma_fence_signal_timestamp(struct dma_fence *fence, ktime_t timestamp);
int dma_fence_signal_timestamp_locked(struct dma_fence *fence,
    ktime_t timestamp);
signed long dma_fence_default_wait(struct dma_fence *fence, bool intr,
    signed long timeout);
int dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb,
    dma_fence_func_t func);
bool dma_fence_remove_callback(struct dma_fence *fence,
    struct dma_fence_cb *cb);
void dma_fence_enable_sw_signaling(struct dma_fence *fence);
int dma_fence_get_status(struct dma_fence *fence);
signed long dma_fence_wait_timeout(struct dma_fence *fence, bool intr,
    signed long timeout);
signed long dma_fence_wait_any_timeout(struct dma_fence **fences,
    uint32_t count, bool intr, signed long timeout, uint32_t *idx);
void dma_fence_set_deadline(struct dma_fence *fence, ktime_t deadline);
struct dma_fence *dma_fence_get_stub(void);
struct dma_fence *dma_fence_allocate_private_stub(ktime_t timestamp);
u64 dma_fence_context_alloc(unsigned int num);
/* Signalling annotations: lockdep-only in Linux (dma-fence.c). */
#ifdef CONFIG_LOCKDEP
bool dma_fence_begin_signalling(void);
void dma_fence_end_signalling(bool cookie);
void __dma_fence_might_wait(void);
#else
static inline bool
dma_fence_begin_signalling(void)
{
	return (true);
}

static inline void
dma_fence_end_signalling(bool cookie)
{
	(void) cookie;
}

static inline void
__dma_fence_might_wait(void)
{
}
#endif

static inline void
dma_fence_put(struct dma_fence *fence)
{
	if (fence != NULL)
		(void) kref_put(&fence->refcount, dma_fence_release);
}

static inline struct dma_fence *
dma_fence_get(struct dma_fence *fence)
{
	if (fence != NULL)
		kref_get(&fence->refcount);
	return (fence);
}

/* Reference for a fence found under RCU; NULL if it is being freed. */
static inline struct dma_fence *
dma_fence_get_rcu(struct dma_fence *fence)
{
	return (kref_get_unless_zero(&fence->refcount) ? fence : NULL);
}

/*
 * Reference to *fencep, re-reading the pointer until the fence found is
 * still the one installed (it may be freed and reused meanwhile, with
 * SLAB_TYPESAFE_BY_RCU). Call under rcu_read_lock().
 */
static inline struct dma_fence *
dma_fence_get_rcu_safe(struct dma_fence __rcu **fencep)
{
	struct dma_fence *fence;

	for (;;) {
		fence = rcu_dereference(*fencep);
		if (fence == NULL || dma_fence_get_rcu(fence) == NULL) {
			if (fence == NULL)
				return (NULL);
			continue;
		}
		if (fence == rcu_access_pointer(*fencep))
			return (fence);
		dma_fence_put(fence);
	}
}

static inline bool
dma_fence_is_signaled_locked(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return (true);
	if (fence->ops->signaled != NULL && fence->ops->signaled(fence)) {
		(void) dma_fence_signal_locked(fence);
		return (true);
	}
	return (false);
}

static inline bool
dma_fence_is_signaled(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return (true);
	if (fence->ops->signaled != NULL && fence->ops->signaled(fence)) {
		(void) dma_fence_signal(fence);
		return (true);
	}
	return (false);
}

/* Is seqno f1 later than f2 on a timeline (32- or 64-bit wrap rules)? */
static inline bool
__dma_fence_is_later(u64 f1, u64 f2, const struct dma_fence_ops *ops)
{
	if (ops->use_64bit_seqno)
		return (f1 > f2);
	return ((int)(lower_32_bits(f1) - lower_32_bits(f2)) > 0);
}

static inline bool
dma_fence_is_later(struct dma_fence *f1, struct dma_fence *f2)
{
	if (WARN_ON(f1->context != f2->context))
		return (false);
	return (__dma_fence_is_later(f1->seqno, f2->seqno, f1->ops));
}

static inline bool
dma_fence_is_later_or_same(struct dma_fence *f1, struct dma_fence *f2)
{
	return (f1 == f2 || dma_fence_is_later(f1, f2));
}

static inline struct dma_fence *
dma_fence_later(struct dma_fence *f1, struct dma_fence *f2)
{
	if (WARN_ON(f1->context != f2->context))
		return (NULL);
	if (dma_fence_is_later(f1, f2))
		return (dma_fence_is_signaled(f1) ? NULL : f1);
	return (dma_fence_is_signaled(f2) ? NULL : f2);
}

/* 0 pending, 1 signaled, negative error. Caller holds fence->lock. */
static inline int
dma_fence_get_status_locked(struct dma_fence *fence)
{
	if (dma_fence_is_signaled_locked(fence))
		return (fence->error != 0 ? fence->error : 1);
	return (0);
}

static inline void
dma_fence_set_error(struct dma_fence *fence, int error)
{
	WARN_ON(test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags));
	WARN_ON(error >= 0 || error < -MAX_ERRNO);
	fence->error = error;
}

static inline ktime_t
dma_fence_timestamp(struct dma_fence *fence)
{
	if (WARN_ON(!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)))
		return (ktime_get());
	while (!test_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags))
		lkpi_cpu_relax();
	return (fence->timestamp);
}

static inline signed long
dma_fence_wait(struct dma_fence *fence, bool intr)
{
	signed long r = dma_fence_wait_timeout(fence, intr,
	    MAX_SCHEDULE_TIMEOUT);

	return (r < 0 ? r : 0);
}


#define	DMA_FENCE_WARN(f, fmt, args...)					\
	pr_warn("f %llu#%llu: " fmt, (unsigned long long)(f)->context,	\
	    (unsigned long long)(f)->seqno, ##args)
#define	DMA_FENCE_ERR(f, fmt, args...)					\
	pr_err("f %llu#%llu: " fmt, (unsigned long long)(f)->context,	\
	    (unsigned long long)(f)->seqno, ##args)

/* Lockdep annotations are not modelled. */
#define	dma_fence_test_signaled_flag(f)					\
	test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &(f)->flags)

/* Container fences (arrays and chains; implementations in gpl/). */
extern const struct dma_fence_ops dma_fence_array_ops;
extern const struct dma_fence_ops dma_fence_chain_ops;

static inline bool
dma_fence_is_container(struct dma_fence *fence)
{
	return (fence->ops == &dma_fence_array_ops ||
	    fence->ops == &dma_fence_chain_ops);
}

#endif /* _LINUXKPI_LINUX_DMA_FENCE_H_ */
