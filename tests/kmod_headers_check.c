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

#ifndef LINUXKPI_ILLUMOS_KERNEL
#error "expected the illumos kernel configuration"
#endif

struct kcheck {
	int		v;
	struct list_head node;
	struct hlist_node hnode;
	struct kref	ref;
};

static LIST_HEAD(kcheck_list);

static void
kcheck_release(struct kref *k)
{
	struct kcheck *kc = container_of(k, struct kcheck, ref);

	list_del_init(&kc->node);
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
	lkpi_assert(sum >= 0);
	return (sum + (int)ARRAY_SIZE(arr) + (int)ALIGN(3, 4));
}
