# Risks and open questions

## Owner decisions needed
1. Exact model: "2018 Latitude Rugged 5420" does not match a known SKU. Likely 5424 Rugged (KBL-R, Gen9.5, UHD 620).
   Please confirm CPU/GPU (`prtconf -v`/`lspci -nn`, device ID). Determines Gen9 vs Gen12 and firmware.
2. Primary i915 source base: drm-kmod (recommended) vs OpenBSD tree.
3. Licensing of the project's own code (CDDL, MIT/BSD-2). Recommended: BSD-2 for KPI so it stays reusable and remains compatible with its BSD-derived pieces.
4. Dev environment: bare-metal OI on the laptop only, or a second machine/VM? Serial/debug access?
5. Linux version pin (6.1 LTS or newer; drm-kmod currently tracks 6.x).
6. Which OpenIndiana release/kernel (Hipster date); is Mesa/X in oi-userland recent enough (iris, DRI3)?

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
- R10 No cloud-side hardware testing; slow feedback loop.

## Unverified assumptions (check before relying)
- illumos-gate has no in-tree DRM; confirmed by tree scan, but OpenIndiana may carry extra patches.
- Exact firmware filenames/versions for KBL (DMC, GuC, HuC) per drm-kmod snapshot.
- OpenIndiana's Mesa version and Gen9 driver availability.
