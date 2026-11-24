# Dev environment and testing

## Constraint
This cloud session is Linux and cannot load illumos kernel modules or touch the laptop. All hardware testing runs on
the owner's machines; sandbox use is limited to editing, static checks, reference lookup.

## VM vs bare metal
- In a VM (works now): KPI core, DRM core, vkms-style virtual KMS device, toy PCI drivers (emulated devices), userland harness tests, build/CI.
- Needs the laptop: everything touching i915 (MMIO, DMA/IOMMU, interrupts, display, GuC/HuC, WC/PAT). Boot OpenIndiana from an external
  SSD/USB as a separate boot environment so the laptop's installed OS is untouched. Keep a second machine for ssh/log capture.
- Do not rely on iGPU passthrough to a VM.

## Proposed setup
- Build host: OpenIndiana (VM or the target laptop) with `build-essential` equivalents (gcc, `onbld` not required;
  out-of-gate Makefiles modelled on gfx-drm `Makefile.files`/`Makefile.rules`).
- Test target: the Rugged laptop, boot environments (`beadm`) for safe rollback; serial or second machine for console; `kmdb` + `-k` boot flag;
  crash dumps (`dumpadm`), `mdb -k`/`::findleaks`, `::kmem_verify`.
- Pure-logic KPI parts (list, idr, xarray, ww_mutex logic, dma-fence) also compile on Linux userland with a test harness
  so CI can run here, no kernel needed.
- Debug flags: `set kmem_flags=0xf`, DEBUG kernel if available, DTrace SDT probes for KPI tracing.
- Static checks: sparse/`-Wall -Wextra`, `cppcheck`, `smatch` over KPI.

## Tests
1. KPI unit tests (userland harness + kernel `ktest`).
2. Toy device drivers (P2) and `vkms` (P3).
3. i915: IGT GPU Tools (needs libdrm, ported alongside), piglit, Mesa CTS/dEQP subsets, kms tests.
4. Soak: module load/unload loops, stress-ng GPU/memory, hotplug loops.

## Source control
Branch `psl/epic-gates-dx5xvk` for now. Upstream imports tracked under `third_party/` with pinned commit IDs.
