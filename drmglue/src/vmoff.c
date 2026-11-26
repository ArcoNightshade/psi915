/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Fake mmap offset manager; see <drmglue/vmoff.h>. Clean-room from the
 * documented behaviour of drm_vma_offset_manager.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <drmglue/vmoff.h>

void
lkpi_vmoff_mgr_init(struct lkpi_vmoff_mgr *m, unsigned long page_offset,
    unsigned long pages)
{
	rwlock_init(&m->lock);
	m->start = page_offset;
	m->pages = pages;
	lkpi_itree_init(&m->tree);
}

void
lkpi_vmoff_mgr_destroy(struct lkpi_vmoff_mgr *m)
{
	WARN_ON(!lkpi_itree_empty(&m->tree));
}

void
lkpi_vmoff_node_init(struct lkpi_vmoff_node *n)
{
	memset(n, 0, sizeof (*n));
	rwlock_init(&n->tag_lock);
	INIT_LIST_HEAD(&n->tags);
}

void
lkpi_vmoff_node_fini(struct lkpi_vmoff_node *n)
{
	struct lkpi_vmoff_tag *t, *tmp;

	WARN_ON(n->pages != 0);
	list_for_each_entry_safe(t, tmp, &n->tags, link) {
		list_del(&t->link);
		kfree(t);
	}
}

int
lkpi_vmoff_add(struct lkpi_vmoff_mgr *m, struct lkpi_vmoff_node *n,
    unsigned long pages)
{
	struct lkpi_itree_node *it;
	unsigned long cand, end;
	int ret = -ENOSPC;

	if (pages == 0)
		return (-EINVAL);
	write_lock(&m->lock);
	if (n->pages != 0) {
		ret = 0;
		goto out;
	}
	end = m->start + m->pages;	/* exclusive */
	cand = m->start;
	/* Disjoint intervals: in-order is also order of ends. */
	for (it = lkpi_itree_min(&m->tree); it != NULL;
	    it = lkpi_itree_succ(it)) {
		if (it->start >= cand && it->start - cand >= pages)
			break;
		if (it->last + 1 > cand)
			cand = it->last + 1;
	}
	if (cand < m->start || cand > end || end - cand < pages)
		goto out;
	n->it.start = cand;
	n->it.last = cand + pages - 1;
	n->pages = pages;
	lkpi_itree_insert(&m->tree, &n->it);
	ret = 0;
out:
	write_unlock(&m->lock);
	return (ret);
}

void
lkpi_vmoff_remove(struct lkpi_vmoff_mgr *m, struct lkpi_vmoff_node *n)
{
	write_lock(&m->lock);
	if (n->pages != 0) {
		lkpi_itree_remove(&m->tree, &n->it);
		n->pages = 0;
		n->it.start = n->it.last = 0;
	}
	write_unlock(&m->lock);
}

struct lkpi_vmoff_node *
lkpi_vmoff_lookup_locked(struct lkpi_vmoff_mgr *m, unsigned long start,
    unsigned long pages)
{
	struct lkpi_itree_node *it;
	unsigned long last;

	if (pages == 0)
		pages = 1;
	last = start + pages - 1;
	if (last < start)
		return (NULL);
	/* Disjoint: at most one node contains start. */
	it = lkpi_itree_first(&m->tree, start, start);
	if (it == NULL || it->last < last)
		return (NULL);
	return (container_of(it, struct lkpi_vmoff_node, it));
}

static struct lkpi_vmoff_tag *
lkpi_vmoff_find_tag(struct lkpi_vmoff_node *n, const void *tag)
{
	struct lkpi_vmoff_tag *t;

	list_for_each_entry(t, &n->tags, link)
		if (t->tag == tag)
			return (t);
	return (NULL);
}

int
lkpi_vmoff_allow(struct lkpi_vmoff_node *n, const void *tag)
{
	struct lkpi_vmoff_tag *t, *new;

	/* Allocate outside the lock; drm_vma_node_allow may sleep too. */
	new = kzalloc(sizeof (*new), GFP_KERNEL);
	write_lock(&n->tag_lock);
	t = lkpi_vmoff_find_tag(n, tag);
	if (t != NULL) {
		t->count++;
	} else if (new != NULL) {
		new->tag = tag;
		new->count = 1;
		list_add_tail(&new->link, &n->tags);
		new = NULL;
	} else {
		write_unlock(&n->tag_lock);
		return (-ENOMEM);
	}
	write_unlock(&n->tag_lock);
	kfree(new);
	return (0);
}

void
lkpi_vmoff_revoke(struct lkpi_vmoff_node *n, const void *tag)
{
	struct lkpi_vmoff_tag *t;

	write_lock(&n->tag_lock);
	t = lkpi_vmoff_find_tag(n, tag);
	if (t != NULL && --t->count == 0)
		list_del(&t->link);
	else
		t = NULL;
	write_unlock(&n->tag_lock);
	kfree(t);
}

bool
lkpi_vmoff_is_allowed(struct lkpi_vmoff_node *n, const void *tag)
{
	bool ok;

	read_lock(&n->tag_lock);
	ok = lkpi_vmoff_find_tag(n, tag) != NULL;
	read_unlock(&n->tag_lock);
	return (ok);
}
