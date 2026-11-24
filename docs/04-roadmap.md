# Roadmap

Feature order and validation ladder are defined in `07-feature-scope-and-validation.md`; phases below map to its steps:
P4=modeset, P5=GEM..ppGTT, P5/P3=sync, P3/P6=dma-buf, P6=iris/Xorg, P7=PM, P8+=anv.

Each phase has an exit test that runs on real hardware.

## P0 Foundation (1-2 wk)
- Dev environment, kernel build harness, loadable skeleton module, CI (doc 05).
- Decide: i915 base, Linux version pin, repo layout, licensing review.
- Exit: empty `linuxkpi` kmod modload/modunload on OpenIndiana test box via `kmdb`/serial.

## P1 KPI core (6-8 wk)
- Headers + sources for types, list/rbtree/kref, atomics, memory, locks, waitqueues, workqueues, timers, completions, RCU, idr/xarray, logging, uaccess.
- KPI unit tests (ktest or userland harness for pure-logic parts).
- Exit: test module exercising each subsystem passes under DEBUG kernel (`kmem_flags`, `lockstat`).

## P2 Hardware plumbing (6-8 wk)
- pci_dev/DDI binding, MMIO/WC mapping, MSI, DMA/sg_table/IOMMU, firmware loader, ACPI/DMI, i2c, cpu/cache ops.
- Exit: toy PCI driver (hello-BAR + IRQ + DMA) built unmodified-style against KPI.

## P3 DRM core (8-12 wk)
- Import drm core at pinned Linux; dma-fence/dma-resv/dma-buf (+fd), GEM, vma_manager, mm, atomic KMS, EDID, DP/HDMI helpers, syncobj, scheduler, ttm; cb_ops ioctl/mmap/poll glue; /dev/dri nodes (card, render).
- Exit: `vkms`-style virtual device exposes KMS, `modetest`-equivalent enumerates; dumb buffer mmap works.

## P4 i915 display-only (8-12 wk)
- Import i915 limited to Gen9 (KBL-R first); disable GT/GuC (`i915.enable_guc=0`, no execbuf).
- DMC firmware, power wells, eDP/HDMI/DP, panel power/backlight, hotplug via sysevent, fb console handoff.
- Exit: native-res KMS on eDP, external monitor hotplug, X modesetting driver unaccelerated.

## P5 i915 GT/GEM (12-16 wk)
- Engines (execlists), GGTT/PPGTT, contexts, GEM execbuf2, relocations, userptr (optional), reset/hangcheck, RC6/RPS, GuC/HuC firmware where needed, perf/oa optional.
- Exit: Mesa iris smoke tests, piglit quick subset, no GPU hangs in 1 h stress.

## P6 Userland acceleration (6-10 wk, overlaps P5)
- libdrm (intel), Mesa iris + ANV, Xorg modesetting with DRI3/Present (needs dma-buf fd, sync_file/syncobj), glamor.
- Optional: VA-API (iHD), Wayland.
- Exit: glxgears/glmark2, desktop compositing, video playback.

## P7 Robustness/features (ongoing)
- Suspend/resume, multihead, FBC/PSR/DRRS, runtime PM, 32-bit compat ioctls, gen12 support, packaging (IPS), docs, upstream conversation with illumos/OpenIndiana.

## P8 Generalization
- Second consumer: amdgpu or a non-GPU driver; harden KPI API-coverage matrix and versioning (`LINUXKPI_VERSION`).

Estimates are single-developer and rough; P1-P3 are the highest-risk and gate everything.
