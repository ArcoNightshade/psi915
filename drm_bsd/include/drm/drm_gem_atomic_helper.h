/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * GEM atomic plane helpers, psi915 clean-room subset.  It replaces the upstream GPL-2.0-or-later header,
 * which pulls in the GPL drm_format_helper.h.  It declares only what the MIT callers use
 * (drm_atomic_helper.c, i915 intel_atomic_plane.c).  The shadow-plane and simple-pipe helpers are not
 * provided.
 */

#ifndef _DRM_GEM_ATOMIC_HELPER_H_
#define _DRM_GEM_ATOMIC_HELPER_H_

struct drm_plane;
struct drm_plane_state;

int drm_gem_plane_helper_prepare_fb(struct drm_plane *plane, struct drm_plane_state *state);

#endif /* _DRM_GEM_ATOMIC_HELPER_H_ */
