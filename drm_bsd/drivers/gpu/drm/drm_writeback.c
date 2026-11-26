/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Writeback stub for psi915 (replaces Linux drm_writeback.c, which is GPL-2.0 and needed only for
 * link closure: i915 creates no writeback connectors).  Written from the MIT callers' use of the
 * interface; not derived from Linux drm_writeback.c.
 *
 * Without writeback connectors the WRITEBACK_FB_ID and WRITEBACK_OUT_FENCE_PTR properties are
 * never created, so the atomic core can reach these functions only through a driver bug.
 * set_fb and cleanup_job are still implemented properly so that job lifetime stays balanced;
 * prepare_job has nothing to prepare, and there is no out-fence.
 */

#include <linux/dma-fence.h>
#include <linux/export.h>
#include <linux/slab.h>

#include <drm/drm_connector.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_writeback.h>

int
drm_writeback_set_fb(struct drm_connector_state *conn_state, struct drm_framebuffer *fb)
{
	struct drm_writeback_job *job = conn_state->writeback_job;

	WARN_ON_ONCE(conn_state->connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK);
	if (job == NULL) {
		if (fb == NULL)
			return (0);
		job = kzalloc(sizeof(*job), GFP_KERNEL);
		if (job == NULL)
			return (-ENOMEM);
		job->connector = drm_connector_to_writeback(conn_state->connector);
		conn_state->writeback_job = job;
	}
	if (fb != NULL)
		drm_framebuffer_get(fb);
	if (job->fb != NULL)
		drm_framebuffer_put(job->fb);
	job->fb = fb;
	return (0);
}
EXPORT_SYMBOL(drm_writeback_set_fb);

int
drm_writeback_prepare_job(struct drm_writeback_job *job)
{
	job->prepared = true;
	return (0);
}
EXPORT_SYMBOL(drm_writeback_prepare_job);

void
drm_writeback_cleanup_job(struct drm_writeback_job *job)
{
	if (job == NULL)
		return;
	if (job->fb != NULL)
		drm_framebuffer_put(job->fb);
	if (job->out_fence != NULL)
		dma_fence_put(job->out_fence);
	kfree(job);
}
EXPORT_SYMBOL(drm_writeback_cleanup_job);

/* No writeback hardware, no fence: the atomic ioctl turns NULL into -ENOMEM. */
struct dma_fence *
drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector)
{
	WARN_ON_ONCE(1);
	return (NULL);
}
EXPORT_SYMBOL(drm_writeback_get_out_fence);
