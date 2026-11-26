/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * DRM device-model glue for illumos: psi915 clean-room replacement for Linux drm_sysfs.c,
 * written from the interface in drm_internal.h / <drm/drm_sysfs.h> and the behaviour the
 * MIT-licensed callers rely on.  Not derived from Linux drm_sysfs.c.
 *
 * What callers need:
 *  - drm_sysfs_minor_alloc() returns an initialised (not yet added) struct device per minor,
 *    named card%d / renderD%d / accel%d, whose drvdata is the minor; drm_drv.c then calls
 *    device_add(), device_del() and put_device() on it.
 *  - drm_sysfs_connector_add() gives each connector a struct device in connector->kdev
 *    (drm_dp_mst_topology.c uses it as the parent of the MST AUX channel).
 *  - Hotplug/property/lease events: Linux sends uevents; here they go to an optional hook that
 *    the illumos glue installs (sysevent).  Without a hook they are dropped.
 * There are no sysfs attributes (status, enabled, dpms, modes, edid): illumos has no sysfs;
 * userspace gets the same data through the DRM ioctls.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>

#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_mode_object.h>
#include <drm/drm_print.h>
#include <drm/drm_property.h>
#include <drm/drm_sysfs.h>

#include "drm_internal.h"

static drm_bsd_uevent_hook_t drm_bsd_uevent_hook;

void
drm_bsd_set_uevent_hook(drm_bsd_uevent_hook_t hook)
{
	WRITE_ONCE(drm_bsd_uevent_hook, hook);
}
EXPORT_SYMBOL(drm_bsd_set_uevent_hook);

static void
drm_bsd_uevent(struct drm_device *dev, enum drm_bsd_uevent ev, u32 conn, u32 prop)
{
	drm_bsd_uevent_hook_t hook = READ_ONCE(drm_bsd_uevent_hook);

	if (hook != NULL)
		hook(dev, ev, conn, prop);
}

int
drm_sysfs_init(void)
{
	return (0);
}

void
drm_sysfs_destroy(void)
{
}

static void
drm_bsd_kdev_release(struct device *kdev)
{
	kfree(kdev);
}

struct device *
drm_sysfs_minor_alloc(struct drm_minor *minor)
{
	struct device *kdev;
	const char *fmt;
	int ret;

	switch (minor->type) {
	case DRM_MINOR_RENDER:
		fmt = "renderD%d";
		break;
	case DRM_MINOR_ACCEL:
		fmt = "accel%d";
		break;
	default:
		fmt = "card%d";
		break;
	}

	kdev = kzalloc(sizeof(*kdev), GFP_KERNEL);
	if (kdev == NULL)
		return (ERR_PTR(-ENOMEM));
	device_initialize(kdev);
	kdev->devt = MKDEV(DRM_MAJOR, minor->index);
	kdev->parent = minor->dev->dev;
	kdev->release = drm_bsd_kdev_release;
	dev_set_drvdata(kdev, minor);
	ret = dev_set_name(kdev, fmt, minor->index);
	if (ret != 0) {
		put_device(kdev);
		return (ERR_PTR(ret));
	}
	return (kdev);
}

/* Nothing to register a class device with: no sysfs class on illumos. */
int
drm_class_device_register(struct device *dev)
{
	return (0);
}
EXPORT_SYMBOL(drm_class_device_register);

void
drm_class_device_unregister(struct device *dev)
{
}
EXPORT_SYMBOL(drm_class_device_unregister);

int
drm_sysfs_connector_add(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct device *kdev;
	int ret;

	if (connector->kdev != NULL)
		return (0);
	kdev = kzalloc(sizeof(*kdev), GFP_KERNEL);
	if (kdev == NULL)
		return (-ENOMEM);
	device_initialize(kdev);
	kdev->parent = dev->primary != NULL ? dev->primary->kdev : dev->dev;
	kdev->release = drm_bsd_kdev_release;
	dev_set_drvdata(kdev, connector);
	ret = dev_set_name(kdev, "card%d-%s",
	    dev->primary != NULL ? dev->primary->index : 0, connector->name);
	if (ret == 0)
		ret = device_add(kdev);
	if (ret != 0) {
		drm_err(dev, "failed to add device for connector %s: %d\n", connector->name, ret);
		put_device(kdev);
		return (ret);
	}
	connector->kdev = kdev;
	return (0);
}

/* Linux adds sysfs links (e.g. to the DDC adapter) here; nothing to do without sysfs. */
int
drm_sysfs_connector_add_late(struct drm_connector *connector)
{
	return (0);
}

void
drm_sysfs_connector_remove_early(struct drm_connector *connector)
{
}

void
drm_sysfs_connector_remove(struct drm_connector *connector)
{
	if (connector->kdev == NULL)
		return;
	device_del(connector->kdev);
	put_device(connector->kdev);
	connector->kdev = NULL;
}

void
drm_sysfs_lease_event(struct drm_device *dev)
{
	drm_bsd_uevent(dev, DRM_BSD_UEVENT_LEASE, 0, 0);
}

void
drm_sysfs_hotplug_event(struct drm_device *dev)
{
	drm_dbg_kms(dev, "generating hotplug event\n");
	drm_bsd_uevent(dev, DRM_BSD_UEVENT_HOTPLUG, 0, 0);
}
EXPORT_SYMBOL(drm_sysfs_hotplug_event);

void
drm_sysfs_connector_hotplug_event(struct drm_connector *connector)
{
	drm_dbg_kms(connector->dev, "[CONNECTOR:%u:%s] hotplug event\n",
	    connector->base.id, connector->name);
	drm_bsd_uevent(connector->dev, DRM_BSD_UEVENT_CONNECTOR, connector->base.id, 0);
}
EXPORT_SYMBOL(drm_sysfs_connector_hotplug_event);

void
drm_sysfs_connector_property_event(struct drm_connector *connector,
    struct drm_property *property)
{
	drm_bsd_uevent(connector->dev, DRM_BSD_UEVENT_PROPERTY, connector->base.id,
	    property != NULL ? property->base.id : 0);
}
EXPORT_SYMBOL(drm_sysfs_connector_property_event);
