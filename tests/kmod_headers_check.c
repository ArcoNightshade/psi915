/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Compile check of the linux/ headers in their illumos-kernel configuration
 * (LINUXKPI_ILLUMOS_KERNEL), against the stub headers in stubs/illumos.
 * Not linked or run. Includes the illumos headers known to clash with Linux
 * names first, to exercise the overrides.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sysmacros.h>
#include <sys/queue.h>

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/stddef.h>
#include <linux/build_bug.h>
#include <linux/container_of.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <asm/barrier.h>
#include <linux/refcount.h>
#include <linux/kref.h>
#include <linux/bits.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/bitfield.h>
#include <linux/log2.h>
#include <linux/math.h>
#include <linux/math64.h>
#include <linux/minmax.h>
#include <linux/limits.h>
#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/overflow.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/seqlock.h>
#include <linux/completion.h>
#include <linux/lockdep.h>
#include <linux/list_sort.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/xarray.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/ww_mutex.h>
#include <linux/semaphore.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/hashtable.h>
#include <linux/mempool.h>
#include <linux/srcu.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/io-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/highmem.h>
#include <linux/firmware.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/pm_runtime.h>
#include <linux/fcntl.h>
#include <asm/cacheflush.h>
#include <asm/set_memory.h>
#include <asm/msr.h>
#include <asm/processor.h>

#ifndef LINUXKPI_ILLUMOS_KERNEL
#error "expected the illumos kernel configuration"
#endif

struct kcheck {
	int		v;
	struct list_head node;
	struct hlist_node hnode;
	struct kref	ref;
	struct rb_node	rb;
};

static int
kcheck_cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
	return (list_entry(a, struct kcheck, node)->v -
	    list_entry(b, struct kcheck, node)->v);
}

static bool
kcheck_less(struct rb_node *a, const struct rb_node *b)
{
	return (rb_entry(a, struct kcheck, rb)->v <
	    rb_entry(b, struct kcheck, rb)->v);
}

static LIST_HEAD(kcheck_list);
static DEFINE_MUTEX(kcheck_mutex);
static DEFINE_SPINLOCK(kcheck_spin);
static DECLARE_RWSEM(kcheck_rwsem);
static DEFINE_SEQLOCK(kcheck_seq);
static DECLARE_COMPLETION(kcheck_done);
static DEFINE_IDR(kcheck_idr);
static DEFINE_XARRAY_ALLOC1(kcheck_xa);
static DEFINE_IDA(kcheck_ida);

static void
kcheck_release(struct kref *k)
{
	struct kcheck *kc = container_of(k, struct kcheck, ref);

	list_del_init(&kc->node);
}

static void
kcheck_release_locked(struct kref *k)
{
	kcheck_release(k);
	mutex_unlock(&kcheck_mutex);
}

int kmod_headers_check(struct kcheck *kc, struct hlist_head *h);

int
kmod_headers_check(struct kcheck *kc, struct hlist_head *h)
{
	struct kcheck *pos, *tmp;
	int arr[4];
	int sum = 0;
	atomic64_t a64 = ATOMIC64_INIT(0);
	unsigned long ul = 0;

	BUILD_BUG_ON(sizeof(u64) != 8);
	kref_init(&kc->ref);
	kref_get(&kc->ref);
	list_add_tail(&kc->node, &kcheck_list);
	hlist_add_head(&kc->hnode, h);
	list_for_each_entry_safe(pos, tmp, &kcheck_list, node)
		sum += max(pos->v, 0) + min(pos->v, 1);
	if (kref_get_unless_zero(&kc->ref))
		(void)kref_put(&kc->ref, kcheck_release);
	(void)kref_put(&kc->ref, kcheck_release);
	sum += (int)atomic64_add_return(2, &a64);
	sum += (int)cmpxchg(&ul, 0UL, 1UL) + (int)xchg(&ul, 2UL);
	smp_mb();
	{
		DECLARE_BITMAP(bm, 100);
		u64 q = ul;

		bitmap_zero(bm, 100);
		set_bit(ffs(sum) + fls(sum), bm);
		sum += (int)hweight64(ul) + (int)bitmap_weight(bm, 100) +
		    (int)find_next_bit(bm, 100, 3) + ilog2(ul + 1) +
		    (int)FIELD_GET(GENMASK(7, 4), ul) + (int)int_sqrt(ul) +
		    (int)mul_u64_u64_div_u64(ul, ul, ul + 1) + (int)do_div(q, 3);
	}
	wmb();
	if (WARN_ON(sum < 0))
		pr_err("negative %d\n", sum);
	WARN_ON_ONCE(IS_ERR(ERR_PTR(-ENOMEM)) == 0);
	dev_info((struct device *)NULL, "j %lu\n", jiffies + msecs_to_jiffies(10));
	BUG_ON(sum == -12345);
	{
		unsigned long flags;
		unsigned int seq;

		mutex_lock(&kcheck_mutex);
		lockdep_assert_held(&kcheck_mutex);
		mutex_unlock(&kcheck_mutex);
		spin_lock_irqsave(&kcheck_spin, flags);
		spin_unlock_irqrestore(&kcheck_spin, flags);
		down_read(&kcheck_rwsem);
		up_read(&kcheck_rwsem);
		do {
			seq = read_seqbegin(&kcheck_seq);
		} while (read_seqretry(&kcheck_seq, seq));
		struct rb_root_cached root = RB_ROOT_CACHED;

		list_sort(NULL, &kcheck_list, kcheck_cmp);
		(void) rb_add_cached(&kc->rb, &root, kcheck_less);
		(void) rb_erase_cached(&kc->rb, &root);
		rcu_read_lock();
		list_for_each_entry_rcu(pos, &kcheck_list, node)
			sum += pos->v;
		rcu_read_unlock();
		synchronize_rcu();
		{
			u32 xid;
			int id = idr_alloc(&kcheck_idr, kc, 1, 0, GFP_KERNEL);

			if (xa_alloc(&kcheck_xa, &xid, kc, xa_limit_31b,
			    GFP_KERNEL) == 0)
				(void) xa_erase(&kcheck_xa, xid);
			if (id > 0)
				(void) idr_remove(&kcheck_idr, id);
			ida_free(&kcheck_ida, ida_alloc(&kcheck_ida, GFP_KERNEL));
		}
		complete(&kcheck_done);
		sum += (int)wait_for_completion_timeout(&kcheck_done, HZ);
		if (kref_put_mutex(&kc->ref, kcheck_release_locked,
		    &kcheck_mutex) == 0)
			sum++;
	}
	{
		char *s = kstrdup("x", GFP_KERNEL);
		struct kmem_cache *c = KMEM_CACHE(kcheck, SLAB_HWCACHE_ALIGN);
		struct kcheck *o = kmem_cache_zalloc(c, GFP_ATOMIC);

		kmem_cache_free(c, o);
		kmem_cache_destroy(c);
		kfree(s);
		vfree(kvcalloc(4, 8, GFP_KERNEL));
		kfree(kmalloc_array(4, sizeof (int), GFP_NOWAIT | __GFP_ZERO));
	}
	lkpi_assert(sum >= 0);
	return (sum + (int)ARRAY_SIZE(arr) + (int)ALIGN(3, 4));
}
