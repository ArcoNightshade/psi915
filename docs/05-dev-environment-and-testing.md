# Dev environment and testing

## Constraint
This cloud session is Linux and cannot load illumos kernel modules or touch the laptop. All hardware testing runs on
the owner's machines; sandbox use is limited to editing, static checks, reference lookup.

## VM vs bare metal
- In a VM (works now): KPI core, DRM core, vkms-style virtual KMS device, toy PCI drivers (emulated devices), userland harness tests, build/CI.
- Needs the laptop: everything touching i915 (MMIO, DMA/IOMMU, interrupts, display, GuC/HuC, WC/PAT). Boot OpenIndiana from an external
  SSD/USB as a separate boot environment so the laptop's installed OS is untouched. Keep a second machine for ssh/log capture.
- Do not rely on iGPU passthrough to a VM.

## Platforms (owner-confirmed)
- Dev VM: OpenIndiana Hipster 2026.10.
- Bare-metal GPU testing: Tribblix 0m41.1 (the only distro the owner has booted successfully on the laptop), from external SSD/USB.
- Both are illumos, but kernel versions and userland differ: build and load-test the kmods on both. Build against each target's own kernel headers,
  not a shared copy; keep module `_depends_on`/ABI assumptions minimal and record the illumos-gate commit each build matches.
- Packaging: IPS for OpenIndiana, SVR4/zap for Tribblix. Keep the install layout (kmods, firmware under /usr/lib/firmware, driver.conf, minor-node perms) in one neutral install script and wrap it per package system.
- Userland (Mesa, libdrm, Xorg): check what Tribblix and OpenIndiana ship; may need to build our own Mesa/libdrm for either.

## Extra machines
Owner has 2 other PCs. Use one as a log/debug host (ssh, `dumpadm` crash dumps copied off, serial/netconsole if the laptop exposes a port) and
one as a build/second test box (also useful for a plain-Intel iGPU of a different generation for regression testing; record their GPU IDs).

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
