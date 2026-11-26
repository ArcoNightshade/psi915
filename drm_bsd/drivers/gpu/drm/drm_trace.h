/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * DRM core trace points (drm_vblank.c).
 * psi915 builds with CONFIG_TRACEPOINTS off, so every trace point is a no-op.  This header replaces the
 * GPL-2.0 upstream trace header.  It lists the trace points the MIT code calls, by name, taken from the
 * call sites, and is not derived from the upstream header.  The arguments go to an empty variadic inline
 * function, so they are evaluated as with upstream's disabled trace points and no unused-variable
 * warnings appear.
 * Generated list; regenerate when the import is re-pinned.
 */

#ifndef _PSI915_DRM_TRACE_H_
#define _PSI915_DRM_TRACE_H_

#ifndef PSI915_TRACE_NOOP
static inline void psi915_trace_noop(int unused, ...) { (void)unused; }
#define PSI915_TRACE_NOOP(...)	psi915_trace_noop(0, ##__VA_ARGS__)
#endif

#ifndef trace_drm_vblank_event
#define trace_drm_vblank_event(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_drm_vblank_event_queued
#define trace_drm_vblank_event_queued(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_drm_vblank_event_delivered
#define trace_drm_vblank_event_delivered(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif

#endif /* _PSI915_DRM_TRACE_H_ */
