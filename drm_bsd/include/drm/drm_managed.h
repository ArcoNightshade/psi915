/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Managed (drm_device lifetime) resources: psi915 clean-room implementation of the DRM
 * "drmm_*" interface, written from the documented behaviour of the API and from its callers in
 * the MIT-licensed DRM core and i915.  Not derived from Linux drm_managed.{c,h}.
 *
 * Every resource registered here is released when the last reference to the drm_device goes
 * away (drm_managed_release(), called by drm_drv.c), newest first.
 */

#ifndef _DRM_MANAGED_H_
#define _DRM_MANAGED_H_

#include <linux/gfp.h>
#include <linux/overflow.h>
#include <linux/types.h>

struct drm_device;
struct mutex;

/* Release callback of a managed action: gets the device and the registered data pointer. */
typedef void (*drmres_release_t)(struct drm_device *dev, void *res);

int __drmm_add_action(struct drm_device *dev, drmres_release_t action, void *data,
    const char *name);
int __drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action, void *data,
    const char *name);

/* Register action(dev, data) to run at device release.  Returns 0 or -ENOMEM. */
#define	drmm_add_action(dev, action, data)					\
	__drmm_add_action(dev, action, data, #action)

/* As drmm_add_action(), but runs action(dev, data) immediately if registration fails. */
#define	drmm_add_action_or_reset(dev, action, data)				\
	__drmm_add_action_or_reset(dev, action, data, #action)

/* Run a registered action now and unregister it (the newest matching registration). */
void drmm_release_action(struct drm_device *dev, drmres_release_t action, void *data);

/* Memory freed at device release. */
void *drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp);
char *drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp);
void drmm_kfree(struct drm_device *dev, void *data);

static inline void *
drmm_kzalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
	return (drmm_kmalloc(dev, size, gfp | __GFP_ZERO));
}

static inline void *
drmm_kmalloc_array(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (check_mul_overflow(n, size, &bytes))
		return (NULL);
	return (drmm_kmalloc(dev, bytes, flags));
}

static inline void *
drmm_kcalloc(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
	return (drmm_kmalloc_array(dev, n, size, flags | __GFP_ZERO));
}

/* Managed mutex: initialised now, destroyed at device release. */
void __drmm_mutex_release(struct drm_device *dev, void *res);
#define	drmm_mutex_init(dev, lock) ({						\
	mutex_init(lock);							\
	drmm_add_action_or_reset(dev, __drmm_mutex_release, lock);		\
})

#endif /* _DRM_MANAGED_H_ */
