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
#include <linux/refcount.h>
#include <linux/kref.h>

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
	lkpi_assert(sum >= 0);
	return (sum + (int)ARRAY_SIZE(arr) + (int)ALIGN(3, 4));
}
