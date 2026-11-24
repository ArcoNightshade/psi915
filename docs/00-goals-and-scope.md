# Goals and scope

## Goals
1. `illumos-linuxkpi`: a kernel module set providing the Linux kernel API surface needed to build
   unmodified (or minimally patched) Linux DRM drivers on illumos.
2. DRM core (from Linux, built on the KPI) providing KMS, GEM, dma-buf, dma-fence, syncobj, atomic modeset.
3. i915 on top, with full acceleration on the owner's laptop (display + 3D + video decode path).
4. Userland: libdrm, Mesa (iris/ANV), Xorg modesetting + DRI3/Present working on OpenIndiana.
5. KPI generic enough that further drivers (amdgpu, others: e.g. Wi-Fi/mac80211, USB) are later consumers.

## Design principle
Keep imported Linux driver source as pristine as possible. Differences live in the KPI, not in the driver
(FreeBSD drm-kmod model), so Linux updates can be re-imported cheaply. Bounded patches carried as a
numbered patch series, never as in-place divergence.

## Non-goals (initially)
- 32-bit kernels; SPARC.
- Legacy (UMS/DRI1) DRM, AGP GART.
- Binary compatibility with Linux modules.
- Upstreaming to illumos-gate before the project is proven (kept out-of-gate, licence-clean).

## Target hardware
- Dell Latitude 5420 Rugged (confirmed by owner). Launched 2021 (not 2018): Tiger Lake, 11th-gen Core, Intel Iris Xe / UHD (Gen12, Xe-LP).
  Expected PCI IDs 8086:9A49 / 9A40 / 9A78 (to be confirmed with lspci). Gen9 (KBL-R) is dropped as primary target.
- Consequences: Gen12 needs DMC firmware (tgl_dmc_ver2_xx.bin), GuC and HuC firmware (tgl_guc_*.bin, tgl_huc_*.bin);
  GuC submission is likely the default path on Gen12 in Linux 6.x (verify in pinned source). Full 48-bit ppGTT, display with
  DDI/TC ports (Type-C DP alt-mode, Thunderbolt-adjacent code), possible PCIe/IOMMU interactions.
- Gen9 can stay as a secondary/regression target only if hardware is available.

## Success criteria
- S1: Linux-derived module loads, probes, binds to the iGPU, no panics under load/unload.
- S2: native-resolution KMS console/X on eDP + external display hotplug.
- S3: Mesa iris OpenGL 4.6-class and ANV Vulkan run on hardware (glxgears -> glmark2 -> piglit subset).
- S4: Stable under 24 h stress; suspend/resume where illumos supports it.
- S5: A second, unrelated Linux driver builds against the KPI (proves generality).
