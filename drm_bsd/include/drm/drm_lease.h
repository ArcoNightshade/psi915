/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * DRM lease interface, psi915 clean-room version.  Declarations follow the MIT-licensed callers
 * (drm_auth.c, drm_crtc.c, drm_encoder.c, drm_ioctl.c, drm_mode_config.c, drm_mode_object.c,
 * drm_plane.c, drm_vblank.c).  psi915 does not grant leases; see drm_bsd/drivers/gpu/drm/drm_lease.c.
 */

#ifndef _DRM_LEASE_H_
#define _DRM_LEASE_H_

#include <linux/types.h>

struct drm_device;
struct drm_file;
struct drm_master;

struct drm_master *drm_lease_owner(struct drm_master *master);
void drm_lease_destroy(struct drm_master *lessee);
bool drm_lease_held(struct drm_file *file_priv, int id);
bool _drm_lease_held(struct drm_file *file_priv, int id);
void drm_lease_revoke(struct drm_master *master);
uint32_t drm_lease_filter_crtcs(struct drm_file *file_priv, uint32_t crtcs);

int drm_mode_create_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv);
int drm_mode_list_lessees_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv);
int drm_mode_get_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv);
int drm_mode_revoke_lease_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv);

#endif /* _DRM_LEASE_H_ */
