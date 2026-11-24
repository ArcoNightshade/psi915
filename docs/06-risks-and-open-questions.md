# Risks and open questions

## Decisions (resolved)
- Model: Dell Latitude 5420 Rugged (Tiger Lake, Gen12). Confirm GPU ID via lspci.
- i915 base: drm-kmod, Linux 6.x. Minimum uAPI 5.4.
- Licence: BSD-2-Clause for project code.
- Dev setup: VM (see doc 05 for the limits).

## Still open
1. Which OpenIndiana release; is oi-userland Mesa/Xorg recent enough (iris, DRI3)?
2. Bare-metal access: can the laptop boot OpenIndiana from external SSD/USB (separate boot env) for GPU testing?
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
- R11 Gen12 pulls in GuC submission, HuC, Type-C/TBT display code: larger surface than Gen9.
- R12 VM cannot exercise the iGPU (no passthrough of the primary iGPU, GVT-g not on TGL); i915 needs bare metal.
- R10 No cloud-side hardware testing; slow feedback loop.

## Unverified assumptions (check before relying)
- illumos-gate has no in-tree DRM; confirmed by tree scan, but OpenIndiana may carry extra patches.
- Exact firmware filenames/versions for KBL (DMC, GuC, HuC) per drm-kmod snapshot.
- OpenIndiana's Mesa version and Gen9 driver availability.
