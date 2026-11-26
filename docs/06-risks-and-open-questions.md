# Risks and open questions

## Decisions (resolved)
- Model: Dell Latitude 5420 Rugged, i7-8650U (Kaby Lake-R, Gen9.5, UHD 620)..
- i915 base: drm-kmod, Linux 6.x. Minimum uAPI 5.4.
- Licence: BSD-2-Clause for project code.
- Dev setup: OpenIndiana 2026.10 VM; bare-metal tests on Tribblix via external drive (see doc 05).

## Still open
1. Are Mesa/Xorg/libdrm on OpenIndiana 2026.10 and Tribblix recent enough (iris, DRI3)? Tribblix is 0m41.1.
2. Bare-metal access: laptop boots Tribblix from external media (owner says yes). Resolved: laptop has a serial port; netconsole also available; spare PCs are an i7-11700K (Gen12 second target) and a Pentium II (terminal only).
3. Serial/second-machine debug access (Rugged 5420 may have a serial port option; netconsole/kmdb alternatives).
4. Exact 6.x tag to pin (suggest the LTS drm-kmod tracks).

## Risks
- R1 Scale: ~500k LOC of imported driver code; any KPI semantic bug becomes a heisenbug. Mitigate with strong tests, DEBUG asserts.
- R2 Scheduling/signal semantics mismatch (H).
- R3 No shmem/swap: memory pressure behaviour of GEM; may need an aobj pager.
- R4 dma-buf fd and PRIME require custom vnode/fs work; DRI3 depends on it.
- R5 Console handoff/panic output; kmdb with active KMS.
- R6 Userland ecosystem lag in OpenIndiana (Mesa, libdrm, xorg); may need updates.
- R7 Single maintainer bandwidth; Linux churn.
- R8 Licence taint: must not copy GPL-only Linux source into the KPI.
- R9 illumos suspend/resume maturity is limited.
- R12 VM cannot exercise the iGPU (primary iGPU passthrough is impractical); i915 needs bare metal.
- R13 Two target distros (OI + Tribblix) double the kernel/userland/packaging matrix.
- R10 No cloud-side hardware testing; slow feedback loop.

## Unverified assumptions (check before relying)
- illumos-gate has no in-tree DRM; confirmed by tree scan, but OpenIndiana may carry extra patches.
- Exact firmware filenames/versions for KBL (DMC, GuC, HuC) per drm-kmod snapshot.
- OpenIndiana's Mesa version and Gen9 driver availability.

## Decision: GPL-only files (owner, 2026-11-26)
GPL-2.0-only Linux files needed by the Gen9 build (sync_file.c, dma-fence-unwrap.c, drm_hdcp_helper.c, drm_writeback.c,
intel_uncore_trace.c, and any found by later audits) ship as a separate GPL-2.0 module/package, not linked into BSD-2 KPI code.
Open: confirm the module boundary is legally workable on illumos (CDDL kernel + GPL module) and that exported symbols are the only coupling.

## Decision: import source (owner, 2026-11-26)
Import DRM core and i915 from upstream Linux v6.14 (github.com/torvalds/linux, tag v6.14, object 35ff3b0f4596) instead of drm-kmod.
Reason: drm-kmod carries FreeBSD `#else` branches (131 files) and BSD rewrites of dma-buf/dma-fence. Upstream gives pristine Linux plus our own patch series.
drm-kmod and OpenBSD stay as reference for porting patterns. LINUXKPI_VERSION to be revised to match 6.14.

## Open: GPL split creates a module dependency cycle (import agent report, 2026-11-26)
Upstream GPL files (gpl/) and MIT files (drm/, i915/) call each other in both directions; illumos module deps cannot express a cycle.
Recommended resolution: clean-room BSD rewrites of small coupled files (drm_managed, drm_sysfs, drm_lease, intel_acpi, cmdline, nomodeset, trace stubs),
stub drm_writeback; keep a GPL module with one-way dependency (dma-buf, dma-fence*, sync_file, drm_hdcp_helper, drm_gem_framebuffer_helper).
Also open: licence of drm_device.h and drm_gem_framebuffer_helper.h (no notice), and whether a CDDL kernel may load a GPL module (legal review).

## Decision: gfx-drm package (owner, 2026-11-27)
Exclude OpenIndiana's gfx-drm package (it registers an older driver named i915). Our driver keeps the name i915. Installer refuses `/` when gfx-drm's i915 is installed.
