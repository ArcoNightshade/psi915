/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * DRM leases for psi915: clean-room replacement for Linux drm_lease.c, written from the uAPI
 * (MIT <uapi/drm/drm_mode.h>), struct drm_master (MIT <drm/drm_auth.h>) and the documented
 * semantics of the lease ioctls.  Not derived from Linux drm_lease.c.
 *
 * psi915 does not grant leases (DRM_IOCTL_MODE_CREATE_LEASE fails with EOPNOTSUPP; leases serve
 * VR headsets and nested compositors, outside the project's scope).  So every master is an owner
 * (lessor == NULL) and holds every mode object.  The query paths still follow the general rules,
 * so the code stays correct should lessee masters ever be created.
 */

#include <linux/export.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include <drm/drm_auth.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_lease.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_mode_object.h>
#include <drm/drm_print.h>

#include "drm_internal.h"

/* The owner at the top of a chain of lessors. */
struct drm_master *
drm_lease_owner(struct drm_master *master)
{
	while (master->lessor != NULL)
		master = master->lessor;
	return (master);
}

/* Is object id usable through this master?  Caller holds mode_config.idr_mutex. */
static bool
drm_bsd_master_holds(struct drm_master *master, int id)
{
	if (master == NULL || master->lessor == NULL)
		return (true);
	return (idr_find(&master->leases, id) != NULL);
}

bool
_drm_lease_held(struct drm_file *file_priv, int id)
{
	if (file_priv == NULL || file_priv->master == NULL)
		return (true);
	return (drm_bsd_master_holds(file_priv->master, id));
}

bool
drm_lease_held(struct drm_file *file_priv, int id)
{
	struct drm_master *master;
	struct drm_device *dev;
	bool held;

	if (file_priv == NULL)
		return (true);
	master = drm_file_get_master(file_priv);
	if (master == NULL)
		return (true);
	dev = master->dev;
	mutex_lock(&dev->mode_config.idr_mutex);
	held = drm_bsd_master_holds(master, id);
	mutex_unlock(&dev->mode_config.idr_mutex);
	drm_master_put(&master);
	return (held);
}

/* Restrict a possible_crtcs mask to the CRTCs the file's master holds. */
uint32_t
drm_lease_filter_crtcs(struct drm_file *file_priv, uint32_t crtcs_in)
{
	struct drm_master *master;
	struct drm_device *dev;
	struct drm_crtc *crtc;
	uint32_t crtcs_out = 0;

	if (file_priv == NULL)
		return (crtcs_in);
	master = drm_file_get_master(file_priv);
	if (master == NULL)
		return (crtcs_in);
	if (master->lessor == NULL) {
		drm_master_put(&master);
		return (crtcs_in);
	}
	dev = master->dev;
	mutex_lock(&dev->mode_config.idr_mutex);
	drm_for_each_crtc(crtc, dev) {
		uint32_t mask = drm_crtc_mask(crtc);

		if ((crtcs_in & mask) != 0 && drm_bsd_master_holds(master, crtc->base.id))
			crtcs_out |= mask;
	}
	mutex_unlock(&dev->mode_config.idr_mutex);
	drm_master_put(&master);
	return (crtcs_out);
}

/* Called for a master going away; without lessees there is nothing to unlink. */
void
drm_lease_destroy(struct drm_master *master)
{
	WARN_ON(master->lessor != NULL);
	WARN_ON(!list_empty(&master->lessees));
}

/* Called when an owner drops master; psi915 owners have no lessees to revoke. */
void
drm_lease_revoke(struct drm_master *master)
{
	WARN_ON(!list_empty(&master->lessees));
}

int
drm_mode_create_lease_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return (-EOPNOTSUPP);
	drm_dbg_lease(dev, "lease creation is not supported\n");
	return (-EOPNOTSUPP);
}

int
drm_mode_list_lessees_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_mode_list_lessees *arg = data;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return (-EOPNOTSUPP);
	if (arg->pad != 0)
		return (-EINVAL);
	arg->count_lessees = 0;		/* no lessees are ever created */
	return (0);
}

int
drm_mode_get_lease_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_lease *arg = data;
	uint32_t __user *ids = u64_to_user_ptr(arg->objects_ptr);
	uint32_t room = arg->count_objects, n = 0;
	struct drm_mode_object *obj;
	struct drm_master *master;
	struct idr *table;
	int id, ret = 0;
	void *entry;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return (-EOPNOTSUPP);
	if (arg->pad != 0)
		return (-EINVAL);
	master = drm_file_get_master(file_priv);
	if (master == NULL)
		return (-EINVAL);

	mutex_lock(&dev->mode_config.idr_mutex);
	/* An owner holds every object that needs a lease; a lessee holds its lease set. */
	table = master->lessor == NULL ? &dev->mode_config.object_idr : &master->leases;
	idr_for_each_entry(table, entry, id) {
		if (master->lessor == NULL) {
			obj = entry;
			if (obj == NULL || !drm_mode_object_lease_required(obj->type))
				continue;
		}
		if (n < room && put_user((uint32_t)id, ids + n) != 0) {
			ret = -EFAULT;
			break;
		}
		n++;
	}
	mutex_unlock(&dev->mode_config.idr_mutex);
	drm_master_put(&master);
	if (ret == 0)
		arg->count_objects = n;
	return (ret);
}

int
drm_mode_revoke_lease_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return (-EOPNOTSUPP);
	return (-ENOENT);	/* no lessee with that id can exist */
}
