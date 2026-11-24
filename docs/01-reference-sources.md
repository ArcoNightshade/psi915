# Reference sources

Fetched with `scripts/fetch-refs.sh` (sparse + shallow clones in `$HOME/ref`, outside this repo).
Snapshot used for planning (2026-11-24):

| Tree | Commit | Role |
|------|--------|------|
| freebsd-src `sys/compat/linuxkpi` | e5477acb8a8b | Primary model for the KPI (~74k LOC, 251 linux/ headers). Most complete, exercised by i915 + amdgpu. |
| freebsd/drm-kmod | 98be24dc9994 | i915/amdgpu/DRM core as near-pristine Linux (LINUXKPI_VERSION 61400, Linux 6.x base). Candidate i915 source base. |
| openbsd `sys/dev/pci/drm` | 3936b62a9fc2 | Linux 6.x DRM+i915 (~410k LOC i915, ~84k DRM core) with a thin `include/linux` (13k LOC). Cross-check source; shows a lighter-weight compat. |
| NetBSD `sys/external/bsd/drm2` | 58668b77d4b4 | Clean `linux/` compat (11k LOC) with autoconf glue; reference for non-Linux attach/detach lifecycles. |
| illumos-gate `usr/src/uts` | f71cb08e8c53 | Target kernel: DDI, ddi_dma, PCI, IOMMU (immu*), ACPI-CA, gfx_private, vgatext. No DRM in-gate. |
| illumos/gfx-drm | 19482684dc82 | Historic illumos DRM+i915 (Linux ~3.x era, ~116k LOC): precedent for devmap/GEM mmap, agpgart, DRM minor nodes, build/packaging. Too old for Gen9. |

## Usage by area
- KPI headers/semantics: FreeBSD (primary), OpenBSD/NetBSD (alternatives where FreeBSD is heavy).
- DRM ioctl node plumbing, devmap/segmap GEM mmap, minor nodes: gfx-drm (drm_drv.c, drm_gem.c, i915 mmap paths).
- Console handoff, framebuffer: illumos `gfx_private`, `vgatext`, tem/`fb` code in illumos-gate.
- DMA/IOMMU: illumos `rootnex.c`, `immu_*.c`.
- PCI IDs, per-gen device tables: Linux `include/drm/intel/pciids.h` (in the OpenBSD and drm-kmod trees).

## Licensing of sources
- i915/DRM core: MIT/X11 style (compatible with CDDL as separate module); individual files vary, audit required.
- illumos: CDDL. FreeBSD/OpenBSD/NetBSD KPI: BSD-2/ISC; code reuse needs attribution preserved.
- Linux-headers-derived content: GPL-2.0 ONLY files must not be copied into the KPI. KPI is written clean-room
  from API behaviour, or derived from BSD-licensed KPI code (FreeBSD/OpenBSD/NetBSD).
- Firmware (DMC/GuC/HuC) from linux-firmware: redistributable, not modifiable; package separately.
