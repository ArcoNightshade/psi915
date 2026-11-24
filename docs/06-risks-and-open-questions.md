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
