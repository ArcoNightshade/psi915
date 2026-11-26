/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Clean-room implementation of the DRM managed-resource interface (<drm/drm_managed.h>).
 * Written from the documented API behaviour and its MIT-licensed callers (drm_drv.c,
 * drm_internal.h, struct drm_device in drm_device.h); not derived from Linux drm_managed.c.
 *
 * Model: dev->managed.resources is a list of records, newest at the head.  A record is either an
 * action (release callback + data) or an allocation (payload in the same block, no callback).
 * drm_managed_release() takes records from the head one at a time, without holding the lock while
 * running a callback (callbacks may sleep), so resources are released in reverse order of
 * registration.  dev->managed.final_kfree is freed by drm_drv.c after drm_managed_release().
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include <drm/drm_device.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>

#include "drm_internal.h"

struct drmm_res {
	struct list_head	link;
	drmres_release_t	release;	/* NULL for allocations */
	void			*data;		/* action argument */
	const char		*name;		/* action name, for diagnostics */
	size_t			size;		/* payload size; 0 for actions */
	/* Allocation payload; aligned like kmalloc() memory. */
	unsigned char		payload[] __attribute__((aligned(16)));
};

static struct drmm_res *
drmm_res_alloc(size_t payload, gfp_t gfp)
{
	size_t total;

	if (check_add_overflow(sizeof(struct drmm_res), payload, &total))
		return (NULL);
	return (kmalloc(total, gfp));
}

static void
drmm_res_link(struct drm_device *dev, struct drmm_res *r)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->managed.lock, flags);
	list_add(&r->link, &dev->managed.resources);
	spin_unlock_irqrestore(&dev->managed.lock, flags);
}

int
__drmm_add_action(struct drm_device *dev, drmres_release_t action, void *data,
    const char *name)
{
	struct drmm_res *r;

	r = drmm_res_alloc(0, GFP_KERNEL);
	if (r == NULL)
		return (-ENOMEM);
	r->release = action;
	r->data = data;
	r->name = name;
	r->size = 0;
	drmm_res_link(dev, r);
	return (0);
}
EXPORT_SYMBOL(__drmm_add_action);

int
__drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action, void *data,
    const char *name)
{
	int ret;

	ret = __drmm_add_action(dev, action, data, name);
	if (ret != 0)
		action(dev, data);
	return (ret);
}
EXPORT_SYMBOL(__drmm_add_action_or_reset);

void
drmm_release_action(struct drm_device *dev, drmres_release_t action, void *data)
{
	struct drmm_res *r, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dev->managed.lock, flags);
	list_for_each_entry(r, &dev->managed.resources, link) {
		if (r->release == action && r->data == data) {
			list_del(&r->link);
			found = r;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->managed.lock, flags);

	if (WARN_ON(found == NULL))
		return;
	found->release(dev, found->data);
	kfree(found);
}
EXPORT_SYMBOL(drmm_release_action);

void *
drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
	struct drmm_res *r;

	r = drmm_res_alloc(size, gfp);
	if (r == NULL) {
		drm_err(dev, "managed allocation of %zu bytes failed\n", size);
		return (NULL);
	}
	r->release = NULL;
	r->data = NULL;
	r->name = "kmalloc";
	r->size = size;
	drmm_res_link(dev, r);
	return (r->payload);
}
EXPORT_SYMBOL(drmm_kmalloc);

char *
drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp)
{
	size_t len;
	char *p;

	if (s == NULL)
		return (NULL);
	len = strlen(s) + 1;
	p = drmm_kmalloc(dev, len, gfp);
	if (p != NULL)
		memcpy(p, s, len);
	return (p);
}
EXPORT_SYMBOL(drmm_kstrdup);

void
drmm_kfree(struct drm_device *dev, void *data)
{
	struct drmm_res *r, *found = NULL;
	unsigned long flags;

	if (data == NULL)
		return;
	spin_lock_irqsave(&dev->managed.lock, flags);
	list_for_each_entry(r, &dev->managed.resources, link) {
		if (r->release == NULL && (void *)r->payload == data) {
			list_del(&r->link);
			found = r;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->managed.lock, flags);

	/* Freeing memory that drmm_kmalloc() did not return is a caller bug. */
	if (WARN_ON(found == NULL))
		return;
	kfree(found);
}
EXPORT_SYMBOL(drmm_kfree);

void
__drmm_mutex_release(struct drm_device *dev, void *res)
{
	mutex_destroy((struct mutex *)res);
}
EXPORT_SYMBOL(__drmm_mutex_release);

/*
 * Called once by drm_drv.c when it embeds the drm_device in a larger driver structure: that
 * container is freed (by drm_drv.c) after all managed resources are released.
 */
void
drmm_add_final_kfree(struct drm_device *dev, void *container)
{
	WARN_ON(dev->managed.final_kfree != NULL);
	/* The drm_device must lie inside the container. */
	WARN_ON((char *)dev < (char *)container);
	dev->managed.final_kfree = container;
}

void
drm_managed_release(struct drm_device *dev)
{
	struct drmm_res *r;
	unsigned long flags;

	for (;;) {
		spin_lock_irqsave(&dev->managed.lock, flags);
		r = list_first_entry_or_null(&dev->managed.resources, struct drmm_res, link);
		if (r != NULL)
			list_del(&r->link);
		spin_unlock_irqrestore(&dev->managed.lock, flags);
		if (r == NULL)
			break;
		if (r->release != NULL)
			r->release(dev, r->data);
		kfree(r);
	}
}
