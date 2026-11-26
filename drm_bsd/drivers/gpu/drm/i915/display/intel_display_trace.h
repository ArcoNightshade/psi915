/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * i915 display trace points.
 * psi915 builds with CONFIG_TRACEPOINTS off, so every trace point is a no-op.  This header replaces the
 * GPL-2.0 upstream trace header.  It lists the trace points the MIT code calls, by name, taken from the
 * call sites, and is not derived from the upstream header.  The arguments go to an empty variadic inline
 * function, so they are evaluated as with upstream's disabled trace points and no unused-variable
 * warnings appear.
 * Generated list; regenerate when the import is re-pinned.
 */

#ifndef _PSI915_INTEL_DISPLAY_TRACE_H_
#define _PSI915_INTEL_DISPLAY_TRACE_H_

#ifndef PSI915_TRACE_NOOP
static inline void psi915_trace_noop(int unused, ...) { (void)unused; }
#define PSI915_TRACE_NOOP(...)	psi915_trace_noop(0, ##__VA_ARGS__)
#endif

#include "i915_drv.h"
#include "intel_crtc.h"
#include "intel_display_types.h"
#include "intel_vblank.h"

#ifndef trace_g4x_wm
#define trace_g4x_wm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_cpu_fifo_underrun
#define trace_intel_cpu_fifo_underrun(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_crtc_flip_done
#define trace_intel_crtc_flip_done(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_crtc_vblank_work_end
#define trace_intel_crtc_vblank_work_end(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_crtc_vblank_work_start
#define trace_intel_crtc_vblank_work_start(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_fbc_activate
#define trace_intel_fbc_activate(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_fbc_deactivate
#define trace_intel_fbc_deactivate(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_fbc_nuke
#define trace_intel_fbc_nuke(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_frontbuffer_flush
#define trace_intel_frontbuffer_flush(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_frontbuffer_invalidate
#define trace_intel_frontbuffer_invalidate(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_memory_cxsr
#define trace_intel_memory_cxsr(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pch_fifo_underrun
#define trace_intel_pch_fifo_underrun(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_crc
#define trace_intel_pipe_crc(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_disable
#define trace_intel_pipe_disable(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_enable
#define trace_intel_pipe_enable(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_update_end
#define trace_intel_pipe_update_end(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_update_start
#define trace_intel_pipe_update_start(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_pipe_update_vblank_evaded
#define trace_intel_pipe_update_vblank_evaded(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_plane_async_flip
#define trace_intel_plane_async_flip(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_plane_disable_arm
#define trace_intel_plane_disable_arm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_plane_update_arm
#define trace_intel_plane_update_arm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_plane_update_noarm
#define trace_intel_plane_update_noarm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_vlv_fifo_size
#define trace_vlv_fifo_size(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_vlv_wm
#define trace_vlv_wm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif

#endif /* _PSI915_INTEL_DISPLAY_TRACE_H_ */
