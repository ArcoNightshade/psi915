# Architecture

## Layering
```
 Mesa (iris/ANV) / Xorg modesetting / libdrm      userland (oi-userland)
 ------------------- /dev/dri/card0, renderD128 ---------------
 i915.ko (Linux source, ~410k LOC)                driver
 drm.ko  (Linux DRM core, GEM, KMS, sched, ttm)   core
 linuxkpi.ko (this project)                       compat layer
 illumos DDI/DKI: ddi_dma, ddi_intr, kmem, taskq, pci_config, devmap, ACPI-CA, immu
```
Modules: `linuxkpi` (core API) -> `linuxkpi_drm` (drm + helpers) -> `i915`. Each a separate kmod with
`_depends_on`, so other drivers reuse `linuxkpi`.

## Key design decisions (proposed)
1. **KPI model = FreeBSD LinuxKPI**, adapted. Headers laid out as `include/linux/*.h`, `asm/*`, `drm/*`
   so driver includes compile unchanged. Implemented as C in `src/`, built with illumos gcc (gcc10+ shadow).
2. **i915 source base = drm-kmod** (pristine Linux import), cross-checked against OpenBSD for fixes.
   Decide one base before Phase 3; do not mix.
3. **DDI binding**: a single `dev_ops` per driver. attach(9E) builds a Linux `struct pci_dev` wrapper
   (`pci_dev` holds `dev_info_t *`), calls `pci_driver.probe`. detach calls `remove`. Handle `DDI_RESUME/SUSPEND` -> PM ops.
4. **Memory**: `kmalloc/kzalloc/vmalloc` -> `kmem_alloc/kmem_zalloc/vmem` (size-tracked wrapper, since Linux
   `kfree` has no size). `struct page` emulated as lightweight object over `page_t`/`ddi_dma` memory;
   `alloc_pages`, `kmap`, `page_to_phys` supported. Shmem-backed GEM objects need a pager: implement as
   anon-segment/`vmem`-backed `swap-able` objects (illumos lacks shmem; start non-swappable, add `seg_vn` aobj later).
5. **DMA**: `dma_map_*`, `dma_alloc_coherent` -> `ddi_dma_*` with per-device attrs; scatter-gather tables
   via `sg_table` over DMA cookies. IOMMU (immu) engages transparently; i915 GGTT must program addresses
   from cookies, never assume identity mapping.
6. **Locking/atomics/RCU**: spin/mutex/rwsem -> `kmutex_t`/`krwlock_t` with owner tracking for `lockdep`-lite
   asserts; RCU implemented as grace-period via taskq/cyclic (simple `synchronize_rcu` = quiesce all CPUs
   via `xc_call`/epoch counter); `call_rcu` -> deferred taskq.
7. **Workqueues/timers/completions/waitqueues** -> `taskq_t`, `timeout()/cyclic`, `cv_t`; `schedule()` is the hard one (see doc 03).
8. **Interrupts**: `request_irq` -> `ddi_intr_add_handler` (MSI preferred, fixed fallback); handler returns mapped to `DDI_INTR_CLAIMED`.
9. **MMIO/BARs**: `ioremap/ioremap_wc` -> `ddi_regs_map_setup` / `devmap` with WC via PAT attributes
   (`DDI_..._MERGING_OK`, or direct PAT); `readl/writel` inlined with barriers.
10. **GEM mmap**: reuse the gfx-drm devmap pattern (`devmap_umem_setup`/custom `segdev` callbacks) for
    dumb-buffer/GTT/WC/WB offsets; fault handling via `devmap_access`.
11. **Firmware**: `request_firmware` -> read from `/usr/lib/firmware` via `kobj_open_file` (boot-time safe, kobj).
12. **Console**: KMS takes over from the VGA/VBE framebuffer; implement `drm_fb_helper` backend writing to a
    `gfx_private`-compatible framebuffer; handle `tem`/console redirection and clean handback on panic (kmdb needs plain framebuffer).
13. **No debugfs/sysfs**: stub into no-ops initially; later expose useful nodes via kstat/`/dev/dri/*` ioctl or sysfs-like pseudo-fs for intel_gpu_top.
14. **Panic/crash safety**: all Linux `BUG()/WARN()` map to `cmn_err` + optional `panic` (tunable).

## Repository layout (planned)
```
docs/          this
include/       linuxkpi headers (clean-room/BSD-derived)
src/           linuxkpi C sources
drm/           DRM core import + patches/
i915/          i915 import + patches/
build/         illumos Makefiles (Makefile.uts style), IPS manifests
tests/         KPI unit tests (kernel ktest / userland harness)
scripts/       fetch-refs, import tooling, sync-with-linux
```
## Import strategy
Scripted import (`scripts/import-linux.sh`) from drm-kmod at a pinned tag, apply `patches/` series with quilt-like tooling,
record upstream commit. Goal: rebase to new Linux LTS yearly.
