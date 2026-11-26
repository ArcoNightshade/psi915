/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Writeback connectors, psi915 stub version.  i915 has no writeback connectors; the DRM atomic
 * core (MIT) still references the job structure and four functions, which this header and
 * drm_bsd/drivers/gpu/drm/drm_writeback.c provide.  The structure members are the ones the MIT
 * callers use (drm_atomic.c, drm_atomic_helper.c, drm_atomic_state_helper.c, drm_atomic_uapi.c).
 * There is no drm_writeback_connector_init(): a driver that needs writeback cannot use this stub.
 */

#ifndef _DRM_WRITEBACK_H_
#define _DRM_WRITEBACK_H_

#include <linux/kernel.h>

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>

struct dma_fence;
struct drm_connector_state;
struct drm_framebuffer;
struct drm_property_blob;

struct drm_writeback_connector {
	struct drm_connector		base;
	struct drm_encoder		encoder;
	/* Supported pixel formats (u32 fourcc array); never set by the stub. */
	struct drm_property_blob	*pixel_formats_blob_ptr;
};

struct drm_writeback_job {
	struct drm_writeback_connector	*connector;
	bool				prepared;
	struct drm_framebuffer		*fb;
	struct dma_fence		*out_fence;
	void				*priv;
};

static inline struct drm_writeback_connector *
drm_connector_to_writeback(struct drm_connector *connector)
{
	return (container_of(connector, struct drm_writeback_connector, base));
}

int drm_writeback_set_fb(struct drm_connector_state *conn_state, struct drm_framebuffer *fb);
int drm_writeback_prepare_job(struct drm_writeback_job *job);
void drm_writeback_cleanup_job(struct drm_writeback_job *job);
struct dma_fence *drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector);

#endif /* _DRM_WRITEBACK_H_ */
