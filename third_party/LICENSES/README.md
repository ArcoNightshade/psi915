# Licences of imported code

psi915's own code (include/, src/, build/, tests/, scripts/, tools/) is BSD-2-Clause (top-level `LICENSE`).

drm/, i915/ and gpl/ come unmodified from upstream Linux v6.14 (third_party/IMPORT_PIN); all changes are
patches in third_party/patches/. Every imported file keeps its original notice.

The fourth column of third_party/IMPORT_FILES records each pristine file's licence, which `scripts/import-linux.sh`
detects from the SPDX tag. When a file has no tag it falls back to a text heuristic, marked `(text)`. Nobody has
reviewed the detection file by file, so treat it as an index, not a legal determination.

## Split by licence
- **drm/ and i915/** hold MIT, HPND and dual-licensed (GPL-2.0 OR MIT) files only. Dual-licensed files are used
  under MIT. The import script refuses to place a GPL-only file in these trees.
- **gpl/** holds every GPL-only file: GPL-2.0, GPL-2.0-only or GPL-2.0-or-later with no MIT/BSD alternative, or a
  .c file with no notice at all (the kernel's COPYING makes such files GPL-2.0).
  - gpl/ is built later as a separate GPL-2.0 module. docs/08 section 5 describes the module boundary and the
    symbols that cross it, and `tools/gpl-boundary.tsv` lists them.
  - The owner decided on this split (docs/06). It is still open whether the split is legally sufficient on
    illumos.

## Summary at the first import (944 files, default sets)

| Licence (as detected) | Files | Tree |
|-----------------------|-------|------|
| MIT, MIT(text) | 814 | drm/, i915/ |
| HPND(text) ("Permission to use, copy, modify, distribute, and sell") | 34 | drm/ (older DRM core files) |
| GPL-2.0 OR MIT, GPL-2.0-only OR MIT | 29 | drm/, i915/ (used under MIT) |
| GPL-2.0, GPL-2.0-only, GPL-2.0-or-later, GPL-2.0+, GPL(text) | 63 | gpl/ |
| NOASSERTION (no notice) | 4 | drm_trace_points.c (in gpl/); drm_device.h and drm_gem_framebuffer_helper.h (drm/); shaders/README (i915/) |

### GPL-only files compiled for Gen9 (gpl/, set core, 19 files)
dma-buf: dma-buf.c, dma-fence.c, dma-fence-array.c, dma-fence-chain.c, dma-fence-unwrap.c, sync_file.c
(GPL-2.0-only). DRM: drm_managed.c, drm_writeback.c, display/drm_hdcp_helper.c (GPL-2.0); drm_sysfs.c
(GPL-2.0-only); drm_lease.c, drm_gem_framebuffer_helper.c (GPL-2.0-or-later); drm_trace_points.c (no notice).
i915: display/intel_acpi.c, display/intel_display_trace.c, i915_trace_points.c, intel_uncore_trace.c (GPL-2.0).
video: cmdline.c, nomodeset.c (GPL-2.0).

The gpl/ tree also holds the GPL headers. Seven of them are included directly by MIT code in drm/ or i915/
(tools/gpl-headers.tsv):
- drm_managed.h (16 includers)
- i915_trace.h (25)
- intel_display_trace.h (8)
- drm_mipi_dsi.h (6)
- drm_writeback.h, i915_hdcp_interface.h, intel_uncore_trace.h (4 each)

drm_lease.h, drm_sysfs.h, intel-gtt.h, drm_accel.h, drm_gem_atomic_helper.h, drm_of.h, drm_trace.h,
drm_dp_mst_topology_internal.h and intel_ips.h have 1-3 includers each.

## Open licence questions (owner decision)
1. **MIT code compiled against GPL headers.** The headers listed above are compiled into MIT code. The trace
   headers expand to nothing with CONFIG_TRACEPOINTS off, so a patch can replace their includes with stubs.
   drm_managed.h, drm_mipi_dsi.h, drm_writeback.h, i915_hdcp_interface.h and intel-gtt.h declare APIs that the
   MIT code calls. Each one needs one of three decisions: accept it as a GPL interface header, rewrite it
   clean-room in BSD (they are small, mostly declarations), or remove its use with a patch.
2. **Headers with no notice in drm/.** drm_device.h and drm_gem_framebuffer_helper.h are included almost
   everywhere. Under the kernel's COPYING default they are GPL-2.0. They descend from MIT drmP.h, but that is
   not verified.
3. **Linux headers the KPI must supply.** tools/kpi-headers.tsv, column linux_licence, shows the licence of the
   upstream copy of each missing Linux header. 95 of the 99 missing headers are GPL or carry no notice, so
   include/ must write them clean-room (FreeBSD/NetBSD/OpenBSD BSD versions are references). Only
   linux/cgroup_dmem.h, linux/vgaarb.h and linux/aperture.h (MIT) and linux/cec.h (dual BSD-3-Clause) could
   be imported. This includes the dma-fence, dma-resv, dma-buf and sync_file headers. Their implementations
   are in gpl/, so the BSD KPI headers must match the structure layouts that those GPL implementations
   expect.
4. **CDDL kernel loading a GPL-2.0 module, and symbol exports between it and the MIT modules.** Still open
   (docs/06).

## Licence texts
SPDX identifiers refer to https://spdx.org/licenses/ (MIT, HPND, BSD-2-Clause, GPL-2.0-only,
GPL-2.0-or-later). Upstream's LICENSES/ directory and COPYING were not imported. Each file's own notice is
authoritative.
