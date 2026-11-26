#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Regenerates the no-op trace headers in drm_bsd/ from the trace_*() names the MIT code calls.
import os, re, glob
TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
names = set()
for f in glob.glob(TOP + '/i915/**/*.[ch]', recursive=True):
    names |= set(re.findall(r'\b(trace_(?:i915|intel_context)_[a-z0-9_]+)\(', open(f, errors='replace').read()))
i915 = sorted(names)
disp = '''trace_g4x_wm trace_intel_cpu_fifo_underrun trace_intel_crtc_flip_done trace_intel_crtc_vblank_work_end
trace_intel_crtc_vblank_work_start trace_intel_fbc_activate trace_intel_fbc_deactivate trace_intel_fbc_nuke
trace_intel_frontbuffer_flush trace_intel_frontbuffer_invalidate trace_intel_memory_cxsr trace_intel_pch_fifo_underrun
trace_intel_pipe_crc trace_intel_pipe_disable trace_intel_pipe_enable trace_intel_pipe_update_end
trace_intel_pipe_update_start trace_intel_pipe_update_vblank_evaded trace_intel_plane_async_flip
trace_intel_plane_disable_arm trace_intel_plane_update_arm trace_intel_plane_update_noarm trace_vlv_fifo_size
trace_vlv_wm'''.split()
HDR = '''/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * {desc}
 * psi915 builds with CONFIG_TRACEPOINTS off, so every trace point is a no-op.  This header replaces the
 * GPL-2.0 upstream trace header.  It lists the trace points the MIT code calls, by name, taken from the
 * call sites, and is not derived from the upstream header.  The arguments go to an empty variadic inline
 * function, so they are evaluated as with upstream's disabled trace points and no unused-variable
 * warnings appear.
 * Generated list; regenerate when the import is re-pinned.
 */

#ifndef {guard}
#define {guard}

#ifndef PSI915_TRACE_NOOP
static inline void psi915_trace_noop(int unused, ...) {{ (void)unused; }}
#define PSI915_TRACE_NOOP(...)	psi915_trace_noop(0, ##__VA_ARGS__)
#endif

'''
def gen(rel, guard, desc, lst):
    p = os.path.join(TOP, 'drm_bsd', rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'w') as f:
        f.write(HDR.format(desc=desc, guard=guard))
        for t in lst:
            f.write(f'#ifndef {t}\n#define {t}(...)\tPSI915_TRACE_NOOP(__VA_ARGS__)\n#endif\n')
        f.write(f'\n#endif /* {guard} */\n')
    print(rel, len(lst))
gen('drivers/gpu/drm/drm_trace.h', '_PSI915_DRM_TRACE_H_', 'DRM core trace points (drm_vblank.c).',
    ['trace_drm_vblank_event', 'trace_drm_vblank_event_queued', 'trace_drm_vblank_event_delivered'])
gen('drivers/gpu/drm/i915/intel_uncore_trace.h', '_PSI915_INTEL_UNCORE_TRACE_H_', 'i915 MMIO register access trace point.',
    ['trace_i915_reg_rw'])
gen('drivers/gpu/drm/i915/display/intel_display_trace.h', '_PSI915_INTEL_DISPLAY_TRACE_H_', 'i915 display trace points.', disp)
gen('drivers/gpu/drm/i915/i915_trace.h', '_PSI915_I915_TRACE_H_', 'i915 GEM, request and context trace points.', i915)
