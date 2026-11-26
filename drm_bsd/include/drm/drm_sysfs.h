/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * DRM device-model and uevent interface, psi915 clean-room version (illumos has no sysfs).
 * Declarations follow the callers in the MIT-licensed DRM core.  See drm_bsd/drivers/gpu/drm/
 * drm_sysfs.c for the behaviour on illumos.
 */

#ifndef _DRM_SYSFS_H_
#define _DRM_SYSFS_H_

#include <linux/types.h>

struct device;
struct drm_connector;
struct drm_device;
struct drm_property;

int drm_class_device_register(struct device *dev);
void drm_class_device_unregister(struct device *dev);

void drm_sysfs_hotplug_event(struct drm_device *dev);
void drm_sysfs_connector_hotplug_event(struct drm_connector *connector);
void drm_sysfs_connector_property_event(struct drm_connector *connector,
    struct drm_property *property);

/*
 * psi915: where Linux sends a uevent, the illumos glue may install a hook (e.g. to post a
 * sysevent for the display server).  connector_id and property_id are 0 when not applicable.
 */
enum drm_bsd_uevent {
	DRM_BSD_UEVENT_HOTPLUG,		/* HOTPLUG=1 */
	DRM_BSD_UEVENT_CONNECTOR,	/* HOTPLUG=1 CONNECTOR=id */
	DRM_BSD_UEVENT_PROPERTY,	/* HOTPLUG=1 CONNECTOR=id PROPERTY=id */
	DRM_BSD_UEVENT_LEASE,		/* LEASE=1 */
};
typedef void (*drm_bsd_uevent_hook_t)(struct drm_device *dev, enum drm_bsd_uevent ev,
    u32 connector_id, u32 property_id);
void drm_bsd_set_uevent_hook(drm_bsd_uevent_hook_t hook);

#endif /* _DRM_SYSFS_H_ */
