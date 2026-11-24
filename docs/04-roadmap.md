# Roadmap

Feature order and validation ladder are defined in `07-feature-scope-and-validation.md`; phases below map to its steps:
P4=modeset, P5=GEM..ppGTT, P5/P3=sync, P3/P6=dma-buf, P6=iris/Xorg, P7=PM, P8+=anv.

Each phase has an exit test that runs on real hardware.

## P0 Foundation (1-2 wk)
- Dev environment, kernel build harness, loadable skeleton module, CI (doc 05).
- Decide: i915 base, Linux version pin, repo layout, licensing review.
- Exit: empty `linuxkpi` kmod modload/modunload on OpenIndiana test box via `kmdb`/serial.

### P0 status (2026-11-24)
Done in the Linux dev container. "Untested on illumos" means written and reviewed, but not yet built or run there.

- [x] Repo skeleton per doc 02: `include/`, `src/`, `build/`, `tests/`, `scripts/`, `third_party/` (placeholder), `LICENSE` (BSD-2-Clause), top-level `Makefile`.
- [x] Out-of-gate kmod build harness modelled on gfx-drm / illumos-gate (`build/Makefile.master`, `Makefile.files`, `Makefile.rules`, `Makefile.targ`, `build/linuxkpi/Makefile`); amd64 kernel flags from illumos-gate f71cb08e8c53. Untested on illumos.
- [x] Neutral installer `scripts/install.sh` driven by `build/install.manifest`, with `-f ips` and `-f svr4` output for package wrappers. Exercised on Linux with a staging root only.
- [x] Skeleton `linuxkpi` misc module (`src/linuxkpi_mod.c`): modlmisc, `_init/_info/_fini`, cmn_err banner, exports only `linuxkpi_version` (= `LINUXKPI_VERSION`, 61400). No `.conf` (misc module). Untested on illumos.
- [x] First pure-logic KPI slice: `linux/{types,compiler,stddef,build_bug,container_of,kernel,list,refcount,kref}.h`, environment selection in `linuxkpi/platform.h`. `list.h` derived from FreeBSD LinuxKPI (notice kept); the rest clean-room.
- [x] Userland test harness (`tests/`, `make test`): list, hlist, kref/refcount (including saturation and an 8-thread race), kernel helpers. Passing with gcc 13, clang, and gcc ASan/UBSan.
- [x] Host-side compile check (`make check-kmod`): kernel sources and the kernel configuration of the headers compile with kernel flags against stub illumos headers; export list checked with `nm`. This is not a real build.
- [ ] `gmake kmod` on OpenIndiana 2026.10 and on Tribblix 0m41.1, each against its own `/usr/include` (owner).
- [ ] CTF present (`ctfdump`), `elfdump -d` shows module type, `modload`/`modinfo`/`modunload` loop with banner on serial console, under `kmem_flags=0xf` (owner). This is the P0 exit test.
- [ ] Record the illumos-gate commit and gcc version each build matched (doc 05).
- [ ] CI: run `make check` on every push (needs a CI host; not set up).
- [ ] Decisions still open: exact Linux tag pin (sets `LINUXKPI_VERSION`), formal licensing review of imported code.

Known gaps carried into P1: `kref_put_lock`/`kref_put_mutex` (need locks), `list_sort`, RCU list variants, list poisoning,
`refcount_t` on a real `atomic_t`, WARN/printk. Also a hazard: illumos `<sys/ddi.h>` (`min`/`max`) and `<sys/sysmacros.h>`
(`ARRAY_SIZE`) are redefined by `linux/kernel.h`, so KPI sources must include illumos headers before `linux/` ones.

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
