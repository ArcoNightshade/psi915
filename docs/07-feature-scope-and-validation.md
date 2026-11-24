# Feature scope, validation ladder, work order

## Kernel (i915 + DRM core)
| Feature | Notes |
|---------|-------|
| GEM | create, pread/pwrite, set_domain, tiling, caching ioctls |
| Execution | execbuffer2, HW contexts, full 48-bit ppGTT (Gen9 default), GPU hang detection + reset (mandatory: one bad batch must not wedge the machine) |
| Sync | dma-fence, dma-resv, sync_file, drm_syncobj (iris needs syncobj + exec fences; anv needs timeline syncobj) |
| mmap | CPU mmap and GTT/offset mmap with fault handler on illumos devmap/segdev callbacks. Hardest illumos-specific part; precedent in gfx-drm |
| dma-buf/PRIME | export/import as fds (needs pseudo-vnode for fds). Required by DRI3 and Wayland |
| Render node | /dev/dri/renderD128 alongside card0 |
| Interrupts / GT PM | solid IRQ path; RC6 and RPS frequency control (functional without, but slow and hot) |

## Userspace
- libdrm: has illumos support; verify ioctl numbers and struct layouts against the imported uAPI headers.
- Mesa: iris for GL (Gen8+; do not use i965). anv for Vulkan later (needs timeline syncobj, memfd, eventfd; check recent illumos support).
  Mesa tolerates older kernel uAPIs.
- Optional later: VA-API via intel-media-driver.

## Validation ladder
1. kmscube (KMS + GBM + GL, no X)
2. Xorg modesetting + glamor + DRI3; glxgears; `glxinfo -B` reports iris
3. igt-gpu-tools subset: gem_exec_*, kms_*, prime_*
4. Wayland compositor (separate track; needs input/seat infrastructure)

## Work order
1. Modeset
2. GEM
3. execbuffer2, single context
4. Hang reset
5. ppGTT + multiple contexts
6. Sync primitives
7. dma-buf
8. Mesa iris
9. Xorg/glamor
10. Power management
11. anv

## Planning notes on this order
- i915 uses dma-fence internally for requests from step 3, so the KPI-level dma-fence/dma-resv (P1/P3) must exist before step 3;
  step 6 is the uAPI exposure (sync_file, syncobj), not the first implementation.
- Steps 2 and 8 depend on GEM mmap (devmap) being correct; schedule it early in step 2.
- Step 7 gates DRI3, so it must land before step 9.
- Kernel uAPI target: "5.4-era" is the stated minimum. The pinned source base (drm-kmod) is Linux 6.x, whose uAPI is a superset; open question below.
- igt needs libdrm, plus a port of its few kernel-facing helpers (debugfs, sysfs) which our KPI stubs; expect to patch igt.
