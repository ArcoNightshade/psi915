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

## Target hardware (needs owner confirmation, see doc 06)
- Dell Latitude Rugged, "2018, 5420". Dell's 2018 rugged was the Latitude 5424 Rugged (Kaby Lake-R,
  Core i5-8350U/i7-8650U, Intel UHD 620, Gen9.5, PCI 8086:5917). The model named "5420 Rugged" (2021)
  is Tiger Lake (Gen12, Xe-LP). Plan assumes Gen9.5 (KBL-R) as primary target; Gen12 is a later stretch.
- Needs: eDP internal panel, HDMI/DP out, backlight, DMC firmware (kbl_dmc_ver1_04.bin).

## Success criteria
- S1: Linux-derived module loads, probes, binds to the iGPU, no panics under load/unload.
- S2: native-resolution KMS console/X on eDP + external display hotplug.
- S3: Mesa iris OpenGL 4.6-class and ANV Vulkan run on hardware (glxgears -> glmark2 -> piglit subset).
- S4: Stable under 24 h stress; suspend/resume where illumos supports it.
- S5: A second, unrelated Linux driver builds against the KPI (proves generality).
