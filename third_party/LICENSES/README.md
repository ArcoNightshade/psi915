# Licences of imported code

psi915's own code (include/, src/, build/, tests/, scripts/, tools/) is BSD-2-Clause (top-level `LICENSE`).
drm/ and i915/ are imported unmodified from drm-kmod (third_party/IMPORT_PIN) plus patches in
third_party/patches/. Every imported file keeps its original licence header. The per-file licence, detected
from the pristine file, is the fourth column of third_party/IMPORT_FILES (`scripts/import-linux.sh` writes it;
SPDX tag when present, otherwise a text heuristic marked `(text)`). Detection is automated and has not been
reviewed file by file; treat it as an index, not a legal determination.

## Summary at the first import (922 files, default sets)

| Licence (as detected) | Files | Notes |
|-----------------------|-------|-------|
| MIT / MIT(text) | 803 | Bulk of DRM core, display helpers, TTM, scheduler, i915. X11/Expat-style permission notice. |
| HPND(text) | 34 | "Permission to use, copy, modify, distribute, and sell" notice (older DRM files: drm_crtc.c, drm_connector.c, ...). MIT-family, permissive. |
| GPL-2.0 OR MIT, GPL-2.0-only OR MIT | 29 | Dual licensed; used under MIT. |
| BSD(text), BSD-2-Clause, BSD-2-Clause-FreeBSD | 19 | drm-kmod/FreeBSD replacement files and shims. |
| public-domain(text) | 12 | drm-kmod files marked "Public domain." These are rewrites, not Linux originals (e.g. drm_managed.c, intel_acpi.c, drm_writeback.h). |
| NOASSERTION | 12 | No licence header: FreeBSD shim headers (drm_os_freebsd.h, i915_trace.h, ttm_sysctl_freebsd.h, ...) and a few headers whose notice starts after line 60. Need manual review. |
| GPL-2.0, GPL-2.0-only, GPL-2.0-or-later, GPL(text), GPL-1.0+ WITH Linux-syscall-note | 15 | See below. |

## GPL-only files (owner decision required)

These are GPL-licensed without an MIT alternative. They are in the imported driver trees, never in the KPI
(include/, src/), which stays BSD-2-Clause and clean-room (docs/06 R8). Whether a GPL-2.0 file may ship in the
same illumos kernel module as CDDL/BSD/MIT code is a licensing decision for the owner; FreeBSD ships the same
files in drm-kmod as separate modules. Replacement options are listed per file.

| File | Set | Licence | Replacement option |
|------|-----|---------|--------------------|
| drm/drivers/dma-buf/sync_file.c | core | GPL-2.0-only | write BSD sync_file in the KPI (small; NetBSD drm2 has a BSD one: sys/external/bsd/drm2/linux/linux_sync_file.c) |
| drm/drivers/dma-buf/dma-fence-unwrap.c | core | GPL-2.0-only | BSD rewrite (small) |
| drm/drivers/gpu/drm/display/drm_hdcp_helper.c | core | GPL-2.0 | HDCP SRM/revocation helpers; could be stubbed if HDCP is deferred |
| drm/drivers/gpu/drm/drm_writeback.c | core | GPL-2.0 | needed only for link closure (no writeback on i915 Gen9); a stub patch removes it |
| i915/drivers/gpu/drm/i915/intel_uncore_trace.c | core | GPL-2.0 | tracepoint stub; trivially replaceable |
| drm/drivers/gpu/drm/drm_panel_backlight_quirks.c | extra | GPL-2.0 | not built |
| drm/drivers/gpu/drm/drm_gem_ttm_helper.c, drm/include/drm/drm_gem_ttm_helper.h, drm/include/drm/drm_format_helper.h | extra/hdr | GPL-2.0-or-later | not built |
| drm/linuxkpi/gplv2/** (dma-buf.h, sync_file.h, dma-fence-unwrap.h, uapi sync_file.h) | kpi-ref | GPL | reference only; the KPI must not copy them |

drm-kmod's dma-buf.c, dma-fence.c, dma-fence-array.c and dma-fence-chain.c are BSD-2-Clause rewrites
(Beckhoff Automation, 2022/2023), not the GPL-2.0-only Linux originals; dma-resv.c is MIT. Only
dma-fence-unwrap.c and sync_file.c remain GPL-2.0-only.

## Licence texts

SPDX identifiers refer to the texts at https://spdx.org/licenses/ (MIT, HPND, BSD-2-Clause, GPL-2.0-only,
GPL-2.0-or-later). Each imported file carries its own notice, which is authoritative.
