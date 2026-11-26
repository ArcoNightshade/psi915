/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Managed device resources (devres, see <linux/device.h>). Clean-room.
 *
 * Each resource is a node on dev->lkpi_devres (newest last) with its
 * release function and the caller's data after the header. Groups are
 * marker nodes (open/close) on the same list. Release functions run
 * without the list lock held.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bug.h>

#define	LKPI_DR_RES	0
#define	LKPI_DR_OPEN	1
#define	LKPI_DR_CLOSE	2

struct lkpi_devres {
	struct list_head	entry;
	dr_release_t		release;
	const char		*name;
	void			*group_id;
	int			kind;
	size_t			size;
	unsigned long long	data[];	/* 8-byte aligned (kmalloc) */
};

static inline struct lkpi_devres *
lkpi_dr_of(void *res)
{
	return (container_of(res, struct lkpi_devres, data));
}

void *
lkpi_devres_alloc(dr_release_t release, size_t size, gfp_t gfp, const char *name)
{
	struct lkpi_devres *dr;

	dr = kmalloc(sizeof (*dr) + size, gfp | __GFP_ZERO);
	if (dr == NULL)
		return (NULL);
	INIT_LIST_HEAD(&dr->entry);
	dr->release = release;
	dr->name = name;
	dr->kind = LKPI_DR_RES;
	dr->size = size;
	return (dr->data);
}

void
devres_free(void *res)
{
	struct lkpi_devres *dr;

	if (res == NULL)
		return;
	dr = lkpi_dr_of(res);
	WARN_ON(!list_empty(&dr->entry));
	kfree(dr);
}

void
devres_add(struct device *dev, void *res)
{
	struct lkpi_devres *dr = lkpi_dr_of(res);

	spin_lock(&dev->lkpi_devres_lock);
	list_add_tail(&dr->entry, &dev->lkpi_devres);
	spin_unlock(&dev->lkpi_devres_lock);
}

/* Caller holds the lock. Newest match first. */
static struct lkpi_devres *
lkpi_dr_find(struct device *dev, dr_release_t release, dr_match_t match,
    void *match_data)
{
	struct lkpi_devres *dr;

	list_for_each_entry_reverse(dr, &dev->lkpi_devres, entry) {
		if (dr->kind != LKPI_DR_RES || dr->release != release)
			continue;
		if (match != NULL && !match(dev, dr->data, match_data))
			continue;
		return (dr);
	}
	return (NULL);
}

void *
devres_find(struct device *dev, dr_release_t release, dr_match_t match,
    void *match_data)
{
	struct lkpi_devres *dr;

	spin_lock(&dev->lkpi_devres_lock);
	dr = lkpi_dr_find(dev, release, match, match_data);
	spin_unlock(&dev->lkpi_devres_lock);
	return (dr != NULL ? dr->data : NULL);
}

void *
devres_remove(struct device *dev, dr_release_t release, dr_match_t match,
    void *match_data)
{
	struct lkpi_devres *dr;

	spin_lock(&dev->lkpi_devres_lock);
	dr = lkpi_dr_find(dev, release, match, match_data);
	if (dr != NULL)
		list_del_init(&dr->entry);
	spin_unlock(&dev->lkpi_devres_lock);
	return (dr != NULL ? dr->data : NULL);
}

int
devres_destroy(struct device *dev, dr_release_t release, dr_match_t match,
    void *match_data)
{
	void *res = devres_remove(dev, release, match, match_data);

	if (res == NULL)
		return (-ENOENT);
	devres_free(res);
	return (0);
}

int
devres_release(struct device *dev, dr_release_t release, dr_match_t match,
    void *match_data)
{
	void *res = devres_remove(dev, release, match, match_data);

	if (res == NULL)
		return (-ENOENT);
	release(dev, res);
	devres_free(res);
	return (0);
}

/*
 * Release, newest first, the nodes after `from` (exclusive; NULL = from
 * the list head) up to `to` (exclusive; NULL = the end).
 */
static int
lkpi_dr_release_range(struct device *dev, struct lkpi_devres *from,
    struct lkpi_devres *to)
{
	struct lkpi_devres *dr;
	struct list_head todo;
	int n = 0;

	INIT_LIST_HEAD(&todo);
	spin_lock(&dev->lkpi_devres_lock);
	for (;;) {
		struct list_head *last = to != NULL ? to->entry.prev :
		    dev->lkpi_devres.prev;

		if (last == &dev->lkpi_devres ||
		    (from != NULL && last == &from->entry))
			break;
		dr = list_entry(last, struct lkpi_devres, entry);
		list_move_tail(&dr->entry, &todo);
	}
	spin_unlock(&dev->lkpi_devres_lock);
	/* todo holds the nodes newest first. */
	while (!list_empty(&todo)) {
		dr = list_first_entry(&todo, struct lkpi_devres, entry);
		list_del_init(&dr->entry);
		if (dr->kind == LKPI_DR_RES) {
			if (dr->release != NULL)
				dr->release(dev, dr->data);
			n++;
		}
		kfree(dr);
	}
	return (n);
}

int
lkpi_devres_release_all(struct device *dev)
{
	if (!dev->lkpi_initialized)
		return (0);
	return (lkpi_dr_release_range(dev, NULL, NULL));
}

static struct lkpi_devres *
lkpi_dr_marker(int kind, void *id, gfp_t gfp)
{
	struct lkpi_devres *dr = kzalloc(sizeof (*dr), gfp);

	if (dr == NULL)
		return (NULL);
	INIT_LIST_HEAD(&dr->entry);
	dr->kind = kind;
	dr->group_id = id;
	return (dr);
}

void *
devres_open_group(struct device *dev, void *id, gfp_t gfp)
{
	struct lkpi_devres *dr = lkpi_dr_marker(LKPI_DR_OPEN, NULL, gfp);

	if (dr == NULL)
		return (NULL);
	dr->group_id = id != NULL ? id : (void *)dr;
	spin_lock(&dev->lkpi_devres_lock);
	list_add_tail(&dr->entry, &dev->lkpi_devres);
	spin_unlock(&dev->lkpi_devres_lock);
	return (dr->group_id);
}

/* Caller holds the lock. Newest open marker of the group (NULL id: newest). */
static struct lkpi_devres *
lkpi_dr_find_group(struct device *dev, void *id, int kind)
{
	struct lkpi_devres *dr;

	list_for_each_entry_reverse(dr, &dev->lkpi_devres, entry)
		if (dr->kind == kind && (id == NULL || dr->group_id == id))
			return (dr);
	return (NULL);
}

void
devres_close_group(struct device *dev, void *id)
{
	struct lkpi_devres *open, *dr;

	dr = lkpi_dr_marker(LKPI_DR_CLOSE, NULL, GFP_KERNEL);
	if (dr == NULL)
		return;
	spin_lock(&dev->lkpi_devres_lock);
	open = lkpi_dr_find_group(dev, id, LKPI_DR_OPEN);
	if (open != NULL) {
		dr->group_id = open->group_id;
		list_add_tail(&dr->entry, &dev->lkpi_devres);
		dr = NULL;
	}
	spin_unlock(&dev->lkpi_devres_lock);
	kfree(dr);
}

void
devres_remove_group(struct device *dev, void *id)
{
	struct lkpi_devres *open, *close = NULL;

	spin_lock(&dev->lkpi_devres_lock);
	open = lkpi_dr_find_group(dev, id, LKPI_DR_OPEN);
	if (open != NULL) {
		close = lkpi_dr_find_group(dev, open->group_id, LKPI_DR_CLOSE);
		list_del_init(&open->entry);
		if (close != NULL)
			list_del_init(&close->entry);
	}
	spin_unlock(&dev->lkpi_devres_lock);
	kfree(open);
	kfree(close);
}

int
devres_release_group(struct device *dev, void *id)
{
	struct lkpi_devres *open, *close;
	int n;

	spin_lock(&dev->lkpi_devres_lock);
	open = lkpi_dr_find_group(dev, id, LKPI_DR_OPEN);
	close = open != NULL ?
	    lkpi_dr_find_group(dev, open->group_id, LKPI_DR_CLOSE) : NULL;
	spin_unlock(&dev->lkpi_devres_lock);
	if (open == NULL)
		return (-ENOENT);
	n = lkpi_dr_release_range(dev, open, close);
	spin_lock(&dev->lkpi_devres_lock);
	list_del_init(&open->entry);
	if (close != NULL)
		list_del_init(&close->entry);
	spin_unlock(&dev->lkpi_devres_lock);
	kfree(open);
	kfree(close);
	return (n);
}

/* devm_kmalloc and friends: the data area is the allocation. */
static void
lkpi_devm_kmalloc_release(struct device *dev, void *res)
{
	(void) dev;
	(void) res;
}

void *
devm_kmalloc(struct device *dev, size_t size, gfp_t gfp)
{
	void *p = lkpi_devres_alloc(lkpi_devm_kmalloc_release, size, gfp,
	    "devm_kmalloc");

	if (p != NULL)
		devres_add(dev, p);
	return (p);
}

static int
lkpi_devm_ptr_match(struct device *dev, void *res, void *data)
{
	(void) dev;
	return (res == data);
}

void
devm_kfree(struct device *dev, const void *p)
{
	if (p == NULL)
		return;
	WARN_ON(devres_destroy(dev, lkpi_devm_kmalloc_release,
	    lkpi_devm_ptr_match, (void *)(uintptr_t)p) != 0);
}

char *
devm_kstrdup(struct device *dev, const char *s, gfp_t gfp)
{
	size_t n;
	char *p;

	if (s == NULL)
		return (NULL);
	n = strlen(s) + 1;
	p = devm_kmalloc(dev, n, gfp);
	if (p != NULL)
		memcpy(p, s, n);
	return (p);
}

char *
devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
{
	char tmp[256];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(tmp, sizeof (tmp), fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int)sizeof (tmp))
		return (NULL);
	return (devm_kstrdup(dev, tmp, gfp));
}

void *
devm_kmemdup(struct device *dev, const void *src, size_t len, gfp_t gfp)
{
	void *p = devm_kmalloc(dev, len, gfp);

	if (p != NULL)
		memcpy(p, src, len);
	return (p);
}

struct lkpi_devm_action {
	void	(*action)(void *);
	void	*data;
};

static void
lkpi_devm_action_release(struct device *dev, void *res)
{
	struct lkpi_devm_action *a = res;

	(void) dev;
	a->action(a->data);
}

static int
lkpi_devm_action_match(struct device *dev, void *res, void *p)
{
	struct lkpi_devm_action *a = res, *b = p;

	(void) dev;
	return (a->action == b->action && a->data == b->data);
}

int
devm_add_action(struct device *dev, void (*action)(void *), void *data)
{
	struct lkpi_devm_action *a;

	a = lkpi_devres_alloc(lkpi_devm_action_release, sizeof (*a), GFP_KERNEL,
	    "devm_action");
	if (a == NULL)
		return (-ENOMEM);
	a->action = action;
	a->data = data;
	devres_add(dev, a);
	return (0);
}

void
devm_remove_action(struct device *dev, void (*action)(void *), void *data)
{
	struct lkpi_devm_action key = { action, data };

	WARN_ON(devres_destroy(dev, lkpi_devm_action_release,
	    lkpi_devm_action_match, &key) != 0);
}

void
devm_release_action(struct device *dev, void (*action)(void *), void *data)
{
	struct lkpi_devm_action key = { action, data };

	WARN_ON(devres_release(dev, lkpi_devm_action_release,
	    lkpi_devm_action_match, &key) != 0);
}
