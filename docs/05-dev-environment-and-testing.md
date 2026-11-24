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
- Core i7-11700K (Rocket Lake, UHD 750, Gen12 Xe-LP): second i915 target and build box. Gen12 is the stretch-goal generation,
  so keep Gen12 code paths compiling and test them here after Gen9.5 works. (UHD 750 PCI ID expected 8086:4C8A/4C8B; confirm.)
  Note: a 11700K with a discrete GPU installed can run the iGPU headless, giving a safe debug box.
- Pentium II-class PC: too old for 64-bit illumos and has no usable GPU for this project. Useful at most as a serial terminal
  (if it has a serial port) or a plain network host; not a test target.
- Laptop debug: serial port available (use `-B console=ttya` plus kmdb) and netconsole. Prefer serial for early boot/panic output,
  netconsole/crash dumps for later. Need a USB-serial or null-modem cable and a host that can run a terminal (any of the PCs).

## Proposed setup
- Build host: OpenIndiana (VM or the target laptop) with `build-essential` equivalents (gcc, `onbld` not required;
  out-of-gate Makefiles modelled on gfx-drm `Makefile.files`/`Makefile.rules`).
- Test target: the Rugged laptop, boot environments (`beadm`) for safe rollback; serial or second machine for console; `kmdb` + `-k` boot flag;
  crash dumps (`dumpadm`), `mdb -k`/`::findleaks`, `::kmem_verify`.
- Pure-logic KPI parts (list, idr, xarray, ww_mutex logic, dma-fence) also compile on Linux userland with a test harness
  so CI can run here, no kernel needed.
- Debug flags: `set kmem_flags=0xf`, DEBUG kernel if available, DTrace SDT probes for KPI tracing.
- Static checks: sparse/`-Wall -Wextra`, `cppcheck`, `smatch` over KPI.

## Commands (P0)
- Any host: `make test` (userland unit tests), `make check-kmod` (compile kernel sources against stub illumos
  headers in `tests/stubs/illumos`; a type check, not a kernel build), `make check` (both).
- illumos: `gmake kmod [DEBUG=1]`, `gmake install [DESTDIR=...]`; details and knobs in README.md.
- As of P0 nothing has been built or loaded on illumos: the build harness, install script and module are
  untested there. The first owner-side run should record gcc version, illumos-gate commit and any harness fixes.

## Tests
1. KPI unit tests (userland harness + kernel `ktest`).
2. Toy device drivers (P2) and `vkms` (P3).
3. i915: IGT GPU Tools (needs libdrm, ported alongside), piglit, Mesa CTS/dEQP subsets, kms tests.
4. Soak: module load/unload loops, stress-ng GPU/memory, hotplug loops.

## Source control
Branch `psl/epic-gates-dx5xvk` for now. Upstream imports tracked under `third_party/` with pinned commit IDs.
