/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Implicit fencing for plane updates: psi915 clean-room replacement for
 * drm_gem_plane_helper_prepare_fb() from Linux drm_gem_atomic_helper.c (GPL-2.0-or-later, not built).
 * Written from the helper's documented contract, not derived from the Linux file.
 *
 * Contract: before a plane scans out a new framebuffer, the commit must wait for pending writes
 * (rendering) to every GEM object backing that framebuffer.  The helper therefore sets
 * plane_state->fence to a fence that signals when all DMA_RESV_USAGE_WRITE fences of those
 * objects have signalled.  An explicit fence that is already present (IN_FENCE_FD) is kept and
 * waited for as well.  Waiting for both is the conservative choice: it never scans out early.
 * The atomic commit code waits on and releases plane_state->fence.
 */

#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-resv.h>
#include <linux/export.h>
#include <linux/slab.h>

#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_plane.h>

int
drm_gem_plane_helper_prepare_fb(struct drm_plane *plane, struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct dma_fence **all = NULL, **got;
	unsigned int nall = 0, ngot, i, j, nplanes;
	struct dma_fence_array *array;
	int ret = 0;

	if (fb == NULL)
		return (0);
	nplanes = fb->format != NULL ? fb->format->num_planes : 1;

	for (i = 0; i < nplanes && i < DRM_FORMAT_MAX_PLANES; i++) {
		struct drm_gem_object *obj = fb->obj[i];
		struct dma_fence **grown;
		bool seen = false;

		if (obj == NULL)
			continue;
		for (j = 0; j < i; j++)	/* planes often share one object */
			seen |= (fb->obj[j] == obj);
		if (seen)
			continue;
		ret = dma_resv_get_fences(obj->resv, DMA_RESV_USAGE_WRITE, &ngot, &got);
		if (ret != 0)
			goto fail;
		if (ngot == 0) {
			kfree(got);
			continue;
		}
		grown = krealloc(all, (nall + ngot + 1) * sizeof(*all), GFP_KERNEL);
		if (grown == NULL) {
			for (j = 0; j < ngot; j++)
				dma_fence_put(got[j]);
			kfree(got);
			ret = -ENOMEM;
			goto fail;
		}
		all = grown;
		for (j = 0; j < ngot; j++)
			all[nall++] = got[j];
		kfree(got);
	}

	if (nall == 0) {
		kfree(all);
		return (0);
	}
	if (state->fence != NULL)
		all[nall++] = state->fence;	/* krealloc left room for it */
	if (nall == 1) {
		state->fence = all[0];
		kfree(all);
		return (0);
	}
	/* The array takes over the references and the array memory. */
	array = dma_fence_array_create(nall, all, dma_fence_context_alloc(1), 1, false);
	if (array == NULL) {
		if (state->fence != NULL)
			nall--;		/* the state keeps its own reference */
		ret = -ENOMEM;
		goto fail;
	}
	state->fence = &array->base;
	return (0);

fail:
	for (j = 0; j < nall; j++)
		dma_fence_put(all[j]);
	kfree(all);
	return (ret);
}
EXPORT_SYMBOL(drm_gem_plane_helper_prepare_fb);
