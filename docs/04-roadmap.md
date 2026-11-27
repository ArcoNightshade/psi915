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

### P1 status (2026-11-26, complete in the dev container)
Done in the Linux dev container; nothing here has been built or loaded on illumos. Per-area detail in
doc 03 "Implementation status".

- [x] Platform layer: `linuxkpi/platform.h` selects `platform_illumos.h` (kmutex/krwlock/kcondvar,
  kmem, lbolt, cmn_err, thread_create) or `platform_userland.h` (pthread shims, malloc, 1000 Hz clock).
- [x] Atomics and barriers (`linux/atomic.h`, `asm/barrier.h`); `refcount_t` on `atomic_t`.
- [x] Bits and arithmetic: bits, bitops, bitmap, bitfield, log2, math, math64, minmax, limits, overflow.
- [x] Logging: printk/pr_*/dev_* to cmn_err, ratelimit, WARN*/BUG* (`linuxkpi_bug_panic` tunable,
  default 0 in the kernel), errno, err, jiffies.
- [x] Memory: kmalloc/kvmalloc/vmalloc with size header, kmem_cache wrappers, gfp, string helpers.
- [x] Locks: spinlock_t, rwlock_t, mutex, rw_semaphore, seqlock, completion, lockdep-lite;
  kref_put_lock/kref_put_mutex. IRQ-safe spinlocks above LOCK_LEVEL need
  `spin_lock_init_irqsafe(lock, iblock_cookie)` in the port.
- [x] list_sort, rbtree (+ augmented callbacks), RCU (epoch emulation, call_rcu thread, kfree_rcu,
  SLAB_TYPESAFE_BY_RCU), rculist, xarray (basic), idr, ida.
- [x] Userland tests for every piece (`make test`, 12 suites, gcc/clang/ASan+UBSan); `make check-kmod`
  compiles all sources and the test module in the kernel configuration and checks symbols.
- [x] Kernel test module scaffold `lkpi_ktest` (tests/kernel, build/lkpi_ktest) running the same suites.
- [ ] Build `linuxkpi` and `lkpi_ktest` on OpenIndiana and Tribblix; run lkpi_ktest under a DEBUG
  kernel with `kmem_flags=0xf`, then `::findleaks`, and with lockstat (owner). This is the P1 exit test.
- [x] Second slice: platform primitives (hires clock and waits, TSD, signal checks, copyin/out,
  detached threads); task_struct/`current`, schedule/wake_up, wait queues and wait_event*, delays,
  ktime; timer_list/hrtimer (timer thread); workqueues (own worker pools), delayed/rcu work,
  kthread and kthread_worker; uaccess; module params and module_init glue; ww_mutex, semaphore;
  kstrto*, sort, bsearch, hash, hashtable; xarray marks; mempool; SRCU. Suites sched, workqueue,
  misc (userland and lkpi_ktest).
- [x] devres arrived with struct device in P2.

Known limitations carried forward: mutex_lock_interruptible never returns -EINTR; kmalloc does not give
natural alignment for power-of-two sizes; HZ is not a compile-time constant; RCU readers pay an atomic
plus a full barrier and cannot run in interrupt handlers above LOCK_LEVEL; rwlock_t readers are not
recursive while a writer waits; printk does not interpret Linux %p extensions.

## P2 Hardware plumbing (6-8 wk)
- pci_dev/DDI binding, MMIO/WC mapping, MSI, DMA/sg_table/IOMMU, firmware loader, ACPI/DMI, i2c, cpu/cache ops.
- Exit: toy PCI driver (hello-BAR + IRQ + DMA) built unmodified-style against KPI.

### P2 status (2026-11-26, complete in the dev container)
Everything below is untested on illumos: the illumos backend (src/plat_illumos_dev.c,
src/linux_pci_ddi.c) is only compiled against stub headers. Detail in doc 03.

- [x] Backend interface `<linuxkpi/plat_dev.h>`: illumos implementation over pci_config(9F),
  ddi_regs_map_setup(9F), ddi_intr(9F), ddi_dma(9F), gfx_private, hat_devload, kobj file reads,
  ksmbios, ACPICA, ddi_log_sysevent, rdmsr; userland implementation with fake PCI devices, a fake
  IOMMU and simulated interrupts (`<linuxkpi/fakedev.h>`).
- [x] Device model (kobject, device, class, devres, uevent -> sysevent), pm/pm_runtime.
- [x] pci_dev wrapper, capabilities, BAR mapping, enumeration, driver registry and
  attach/detach/suspend/resume glue (`<linuxkpi/ddi.h>`, LKPI_PCI_DRIVER_MODULE).
- [x] IRQs (MSI-X/MSI/INTx, threaded, devm), tasklets, irq_work.
- [x] MMIO accessors, ioremap/_wc/_cache, memremap, io_mapping.
- [x] DMA API, sg_table, dma_pool; struct page allocator, vmap, virt_to_page.
- [x] request_firmware, ACPI handles/_DSM, DMI matching, I2C core + i2c-algo-bit, CPUID/clflush/MSR,
  debugfs/procfs/sysfs stubs, seq_file.
- [x] Exit artifact: `drivers/toy/toy_edu.c`, a Linux-style driver for QEMU's edu device
  (BAR0 identification/liveness, software and factorial interrupts, coherent + streaming + pool
  DMA round trip). It passes in userland against a simulated edu device (`tests/test_toy.c`) and
  builds as the `lkpi_toy` kmod (`build/lkpi_toy`, stub-header check only).
- [ ] Owner: build linuxkpi and lkpi_toy on OpenIndiana, boot a QEMU VM with `-device edu`,
  `add_drv -i '"pci1234,11e8"' lkpi_toy`, expect the console line
  "edu 0x10000ed: live 1 irq 1 fact 3628800 (1) dma 1 pool 1". This is the P2 exit test.
- [ ] Owner: run lkpi_ktest (now 15 suites) under kmem_flags=0xf.

Requests from drmglue (docs/10 section 5): R2 (`file` rename in platform.h), R5
(`lkpi_page_release`, `put_page`, `page_to_pfn`) and R8 (`<linux/fcntl.h>` with illumos O_* values)
are done; `lkpi_plat_uevent` is in src/plat_userland_dev.c (linked into every test). Still open,
first items of P3: R1/R3 (move drmglue's struct file/inode/address_space/vm_area_struct into
include/linux/fs.h, mm_types.h, together with its interval tree) and R10 (BSD dma-fence, dma-resv,
dma-buf, sync_file headers with v6.14 layouts).

Known P2 limitations: set_pages_*/set_memory_* cannot change cache attributes (memory stays WB);
wbinvd_on_all_cpus flushes only the calling CPU; DMA mappings must be bus-contiguous (no swiotlb);
no bridge topology; runtime PM always-on; interrupt vectors above LOCK_LEVEL refused.

## P3 DRM core (8-12 wk)
- Import drm core at pinned Linux; dma-fence/dma-resv/dma-buf (+fd), GEM, vma_manager, mm, atomic KMS, EDID, DP/HDMI helpers, syncobj, scheduler, ttm; cb_ops ioctl/mmap/poll glue; /dev/dri nodes (card, render).
- Exit: `vkms`-style virtual device exposes KMS, `modetest`-equivalent enumerates; dumb buffer mmap works.

### P3 KPI status (2026-11-26, in progress)
Measured with `scripts/p3-compile-check.py`, which syntax-checks every imported translation unit
(third_party/IMPORT_FILES sets core + link, plus drm_bsd/MANIFEST) with gcc -fsyntax-only in the
illumos kernel configuration (stub illumos headers, kconfig.h + generated autoconf.h, include/ first,
drm_bsd/ ahead of drm/include, gpl/include only for gpl/ units). Results in tests/out/p3/
(results.tsv per file, summary.txt with the most frequent missing headers and names). The first error
in a file hides later ones, so counts move in steps.

| Date | Units compiling | drm core | drm_bsd | gpl core | i915 core | i915 link |
|------|-----------------|----------|---------|----------|-----------|-----------|
| 2026-11-26 start | 2/374 | 2/69 | - | 0/19 | 0/184 | 0/100 |
| after first header batches | 22/363 | 9/69 | - | 4/8 | 4/184 | 4/100 |
| after hdmi.h, cache.h, seq/fs fixes | 46/370 | 25/69 | 4/7 | 5/8 | 5/184 | 6/100 |
| after compiler_types.h, BITS_PER_LONG, pseudo_fs move, batch 4 | 80/370 | 53/69 | 4/7 | 6/8 | 9/184 | 7/100 |
| after BUILD_BUG_ON as compiletime_assert, cleanup.h, batch 5 | 208/368 | 64/69 | 5/7 | 6/8 | 91/184 | 41/98 |
| after batch 6 (pid, sysctl, writeback, radix iter, mipi_display, cmdline) | 235/368 | 65/69 | 6/7 | 6/8 | 106/184 | 51/98 |

Module build syntax-check (`scripts/build-gfx.sh syntax-check HOST_STUBS=1`, docs/11; stub
illumos headers, so it measures KPI header coverage only; per-module pass counts):

| Date / state | linuxkpi_gpl_g1 | linuxkpi_drm | linuxkpi_gpl_g2 | i915 | total |
|--------------|-----------------|--------------|-----------------|------|-------|
| docs/11 snapshot (merge 9cfcdba) | 17/18 | 48/77 | 3/3 | 16/287 | 84/385 |
| after batch 6, compiler_types.h, -DI915 in build/i915 | 17/18 | 73/77 | 3/3 | 259/287 | 352/385 |
| after batch 7, drmglue in linuxkpi (R-K2: g1 loses 10 drmglue TUs) | 8/8 | 75/77 | 3/3 | 270/287 | 356/375 |
| 2026-11-26 import/drm_bsd fixes below (2 non-units dropped, 1 drm_bsd unit added) | 8/8 | 78/78 | 3/3 | 285/285 | 374/374 |
| after batch 8 (string helpers, sscanf, uuid, unaligned, of/mmap_lock stubs) | 8/8 | 76/77 | 3/3 | 273/287 | 360/375 |
| after batch 9 (bitmap regions, anon_inode_getfd, DEFINE_SHOW_ATTRIBUTE, compiler_types.h without platform.h) | 8/8 | 76/77 | 3/3 | 277/287 | 364/375 |

Done:
- [x] R1/R3: struct file (lkpi_file), inode, address_space, path, file_operations, poll_table,
  kdev_t in include/linux/fs.h (+poll.h, kdev_t.h, file.h, anon_inodes.h); vm_area_struct, vm_fault,
  vm_operations_struct, VM_* in mm_types.h with the helper prototypes in mm.h; interval tree moved
  to include/linuxkpi/itree.h + src/linux_itree.c. drmglue includes them (its make check passes).
- [x] R10: BSD clean-room dma-fence, dma-fence-array/chain/unwrap, dma-resv, dma-buf, sync_file
  headers with v6.14 member layouts (implementations: gpl/ module and the MIT dma-resv.c).
- [x] Notifier chains (atomic, blocking, raw); xas_* not needed (no user in the imported tree).
- [x] Linux printf: %pV, %pe, %ph, %pa, %p4cc, %pR in printk and the snprintf/scnprintf/kasprintf
  family (lkpi_vsnprintf).
- [x] Headers: kconfig.h (IS_ENABLED), export.h, asm/ioctl.h (Linux encoding), string_helpers,
  string_choices, llist, average (EWMA), ctype, random, radix-tree (over XArray), iosys-map,
  interval_tree_generic (INTERVAL_TREE_DEFINE), iopoll, sizes, byteorder, cache, notifier, shrinker
  (registry), seq_buf, capability (drv_priv), uidgid, poison, pfn_t, uapi sync_file, tracepoint and
  trace/events/dma_fence no-ops, hdmi.h (MIT, imported unchanged from v6.14 like hdmi.c), and
  stubs for backlight, component, pm_qos, pwm, perf_event, stackdepot, dynamic_debug, vga_switcheroo,
  aperture, hypervisor, mmu_notifier, shmem_fs (declarations only), pagevec, pagemap, eventfd,
  platform_device, ref_tracker, relay, gpio, debugobjects, kmsg_dump, ...
- [x] drmglue pseudo-fs types moved into include/linux (fs.h super_block/file_system_type,
  dcache.h, mount.h, fs_context.h, pseudo_fs.h); drmglue/include/drmglue/pseudofs.h only gathers them.
- [x] linux/compiler_types.h force-included like kbuild (-include), BITS_PER_LONG usable in #if,
  mutex.h pulls list.h and kobject.h completion.h as in Linux; removed non-Linux tasklet_is_locked()
  and __sg_next() that collided with i915's own.
- [x] Batch 4: stat, prefetch, cpufeature, sysrq, vt, wait_bit, pnp, asm/intel-family,
  asm/cpu_device_id (x86_match_cpu, tested), auxiliary_bus, mei_aux, device/bus, gpio/machine, cec,
  media/cec-notifier, media-bus-format, asm/byteorder, video/of_display_timing; illumos EREMOTE,
  ENOLINK, EBADMSG, EMSGSIZE, EBADSLT in the stub errno.h.
- [x] Batch 5 (docs/11 R-K5, R-K6): BUILD_BUG_ON in Linux's compiletime_assert form (error-attribute
  call removed by the optimiser; static_assert stays _Static_assert), typecheck.h, jump_label.h
  (static keys as booleans), cleanup.h (DEFINE_FREE/__free, DEFINE_CLASS/CLASS, guards) with
  CLASS(fd), MIN_T/MAX_T, strscpy with 2 or 3 arguments, print_hex_dump/hex_dump_to_buffer (tested),
  printk level taken from the formatted text as in Linux, asm/fpu/api.h (kernel SSE not supported:
  SSE4.1 hidden in the kernel so MOVNTDQA paths are never taken), uapi/linux/dma-buf.h,
  uapi/linux/magic.h, video/of_videomode.h, video/videomode.h, mfd/intel_soc_pmic.h, nomodeset.h
  prototypes (video_bsd_set_firmware_drivers_only), aperture.h prototypes, pfn_t renamed away from
  illumos pfn_t, DECLARE_FLEX_ARRAY, __init_waitqueue_head(wq, name, key), RB_DECLARE_CALLBACKS
  ends with ';' as in Linux, shrinker.h from mm.h, seq_file.h/interrupt.h from perf_event.h.
  The compile loop skips .c files that other units #include.
- [x] Batch 6: linux/pid.h (struct pid table: get_task_pid/task_tgid/get_pid/put_pid), sysctl.h
  (tables accepted, not published), writeback.h, address_space_operations (a_ops; shmem backend still
  to do), radix_tree_for_each_slot over the XArray, castable_to_type, __diag_*, lockdep acquire/release
  no-ops, irq_work node.llist, io_mapping prot as pgprot_t, ___wait_var_event, DEFINE_SIMPLE_ATTRIBUTE
  / DEFINE_DEBUGFS_ATTRIBUTE, VMA_ITERATOR (no VMAs visible: i915 userptr refused), tsc_khz (CPUID
  0x15/0x16), cpufreq_policy, FBINFO_STATE_*, vga_switcheroo_client_ops, fwnode_handle,
  pci_bus_for_each_resource (no bus windows), PCIBIOS_MIN_MEM, PCI_STD_RESOURCES,
  tasklet_unlock_spin_wait, container_of_const, video/cmdline.h, video/mipi_display.h (values checked
  numerically against v6.14), illumos ENOTCONN/ENOPKG/EADDRINUSE in the stub errno.h; build
  Makefile.lkpi force-includes compiler_types.h; the compile loop passes -DI915 for i915 units.
- [x] Batch 7: of_graph.h, pinctrl/consumer.h + machine.h, agp_backend.h, pm_generic_* callbacks,
  irq_chip/irq_data (no irq_desc allocator), relay callbacks (relay_open fails), bin_attribute.mmap,
  power_group_name, platform_device_info, acpi_bus_event, VGA_SWITCHEROO_CAN_SWITCH_DDC,
  pci_bus_alloc_resource (fails) and pcibios_align_resource, PTR_IF, ratelimit flags, SEEK_*,
  kdev_t.h and dcache.h from fs.h, writeback.h from swap.h, sysctl.h from perf_event.h, uaccess.h
  from mm.h, ECHRNG/EWOULDBLOCK; slab.h no longer redefines L1_CACHE_BYTES.
- [x] Batch 8 (from the implicit-declaration warnings of the syntax-check logs): src/linux_string.c
  (strim, strsep, strchrnul, strnchr, strrchr, strlcat, match_string, a small sscanf, uuid_is_valid,
  uuid_gen/guid_gen; tested), linux/unaligned.h (tested), mem_is_zero, strtomem_pad, isascii,
  BUILD_BUG_ON_INVALID, local_clock, seqprop_sequence, dma_fence_is_container in dma-fence.h,
  mmap_lock.h (no-ops: the KPI does not manage user address spaces), vm_mmap (fails: legacy GEM mmap
  unsupported), folio helpers, of/fwnode no-ops, register_chrdev (no effect; drmglue owns the cdev),
  simple_pin_fs/simple_release_fs (drmglue pseudofs.c), pci_bus_write_config_byte/word, port I/O
  outb/inb, pat_enabled, KHZ2PICOS, xa_alloc_cyclic_irq, time_after32, old_encode_dev, bitmap.h from
  sched.h, byteorder.h and asm/processor.h from kernel.h.
- [x] docs/11 R-K2: drmglue/src is linked into linuxkpi (build/Makefile.files DRMGLUE_OBJS,
  drmglue_<file>.o; linuxkpi_mod.c starts drmglue_init last and stops it first); Makefile.lkpi
  defaults to DRMGLUE_IN=none, so linuxkpi_gpl_g1 has no BSD infrastructure. tests/Makefile
  check-kmod compiles the drmglue objects as part of linuxkpi (both allowlists).
- [x] docs/11 R-K4: module parameter descriptors are __lkpi_param_<LKPI_MODNAME>_<name>
  (Makefile.lkpi passes -DLKPI_MODNAME=$(LINUX_MODNAME)); LKPI_PARAM_SYM/LKPI_PARAM_DECLARE, used by
  drivers/i915_glue/i915_drm_glue.c; tested in test_misc (param_modname).
- [x] docs/11 R-B1: build/Makefile SUBDIRS = linuxkpi linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915
  lkpi_ktest lkpi_toy; `gmake install` runs scripts/install-all.sh (ROOT, DEBUG) plus the test
  modules' own install targets.
- [x] docs/11 R-K1: core/postcore/arch/fs/rootfs_initcall and __exitcall in <linux/module.h>.
- [x] docs/11 R-K3: LKPI_PCI_DRIVER_MODULE_OPS(name, desc, cbops, getinfo, attach, detach, init,
  exit); LKPI_PCI_DRIVER_MODULE now accepts a cb_ops address (NULL told apart by type).

- [x] Batch 9: bitmap_shift_right, bitmap_find_free_region/release_region (tested), anon_inode_getfd
  (drmglue anonfile.c, tested), DEFINE_SHOW_ATTRIBUTE producing real name_fops, MAX_LFS_FILESIZE,
  acpi_resource (I2C serial bus), acpi_device_handle/acpi_dev_get_resources stubs,
  i2c_acpi_find_adapter_by_handle, vmalloc_to_page, find_lock_page/unlock_page/clear_page_dirty_for_io
  (no page cache yet), copy_highpage/clear_page, call_mmap, device_iommu_mapped (false),
  device_for_each_child, dev_get_platdata, backlight_enable/disable, devm_gpiod_get_index, U64_C,
  IOMEM_ERR_PTR, WARN_RATELIMIT, acpi.h from i2c.h, file.h from sync_file.h. compiler_types.h no
  longer pulls in platform.h (it broke the illumos-first include rule of glue files such as
  i915_ddi.c under the force-include).

The 11 syntax-check failures that were outside the KPI are fixed (import/drm_bsd, 2026-11-26).
`scripts/build-gfx.sh syntax-check HOST_STUBS=1` now passes 374/374:
- [x] Import (third_party/drm_bsd, R-G1): drm/drm_format_helper.h is not on the allow list
  (drm_atomic_helper.c, intel_atomic_plane.c).
  Resolution: drm_format_helper.h really is GPL-2.0-or-later. It was reached only through the
  upstream GPL drm_gem_atomic_helper.h, and the one function the MIT code calls,
  drm_gem_plane_helper_prepare_fb, lived in the unbuilt GPL drm_gem_atomic_helper.c, so the link
  would have failed as well. drm_bsd/ now carries a BSD clean-room drm_gem_atomic_helper.h/.c. The
  GPL pair is set "replaced" and drm_gem_atomic_helper.h is off the allow list. gpl-policy.conf
  records why drm_format_helper.h is not listed.
- [x] drm_bsd: drm_bsd/drivers/gpu/drm/i915/i915_trace.h and display/intel_display_trace.h replace
  upstream headers that include "i915_drv.h", "gt/intel_engine.h", "i915_irq.h" (resp.
  "intel_crtc.h", "intel_display_types.h", ...); several units relied on that (gen6_ppgtt.c,
  gen8_ppgtt.c, intel_ppgtt.c, intel_gtt.c, i915_gem_shrinker.c, i9xx_wm.c, intel_pmdemand.c).
  Resolution: tools/gen-drm-bsd-trace.py now makes the no-op headers include "i915_drv.h",
  "i915_irq.h" and "gt/intel_engine.h" (resp. "i915_drv.h", "intel_crtc.h", "intel_display_types.h"
  and "intel_vblank.h").
- [x] drm_bsd: drm_sysfs.c should define `struct class *drm_class` (referenced by the imported
  drm_privacy_screen.c; found by scripts/kpi-linkcheck.sh).
  Resolution: defined in drm_bsd drm_sysfs.c and created by drm_sysfs_init() (class_create("drm")).
  Minor and connector devices use it too.
- [x] Module build: build/i915/Objects.mk lists gt/hsw_clear_kernel.c and gt/ivb_clear_kernel.c,
  which are #included by gen7_renderclear.c and are not translation units.
  Resolution: new manifest set "inc" (imported by default, never compiled). The two files are in it,
  so scripts/gen-module-objs.sh, which compiles only core and link, no longer lists them.
- [ ] Link check after these fixes (scripts/kpi-linkcheck.sh, 374 objects, 12 unresolved), new
  requests:
  - [x] Import/patch (mine): intel_gmch_* from intel_ggtt_gmch.c (Gen2-5, upstream from
    drivers/char/agp/intel-gtt.c, not imported). Fixed by patch 0004: local stubs, and probing
    fails with -ENODEV. intel-gtt.h is now off the GPL allow list.
  - [x] Import (mine): intel_pxp_gsccs_is_ready_for_sessions from pxp/intel_pxp.c. Not eliminated:
    the illumos flags (-fno-inline-small-functions ...) keep the call. Fixed by patch 0005: an
    inline "false" stub when PXP is off (upstream candidate).
  - After 0004/0005: syntax-check 374/374, kpi-linkcheck 3 unresolved (the KPI items below).
  - KPI: intel_graphics_stolen_res (x86 early quirks resource; i915_gem_stolen.c) and
    register_/unregister_vmap_purge_notifier (i915_gem_shrinker.c).
- [ ] Import (P3 loop only, set-dependent): drm/intel/i915_hdcp_interface.h, intel-gtt.h,
  drm_mipi_dsi.h, drm_accel.h, drm_dp_mst_topology_internal.h, intel_acpi.h for the
  third_party/IMPORT_FILES layout used by scripts/p3-compile-check.py (the module build has them).

- [x] shmem backing store: src/linux_shmem.c (page cache over address_space.i_pages, zero-filled
  pages on first use, truncate, find_lock_page, write_begin/write_end; unit-tested in test_hw) and
  drmglue/src/shmem.c (shmem_file_setup/_with_mnt: internal file + inode, pages freed with the
  inode). No swap: pages stay resident until truncated or the object is freed; writeback of
  purgeable objects does nothing (no writepage).
- [x] Link-time closure check: scripts/kpi-linkcheck.sh compiles the module set to objects
  (HOST_STUBS=1, `objects` target added to build/drm/Makefile.lkpi) and lists symbols that neither
  the set, nor linuxkpi (check-kmod), nor the illumos allowlists define. 2026-11-26: 364 objects,
  104 unresolved, all defined in the 11 units that do not compile yet (other owners, above) except
  `drm_class` (drm_bsd/drivers/gpu/drm/drm_sysfs.c does not define it; drm_privacy_screen.c uses
  it). Units that fail to compile are not covered, so the list grows as they are fixed.

- [x] Module-load risk review (docs/11 section 10): scripts/kpi-modcheck.py checks load-order
  resolution, duplicate globals across modules, weak symbols, clashes with illumos kernel symbols
  (heuristic over usr/src/uts), allowlisted illumos symbols, relocation types krtld supports, sizes,
  and the linuxkpi export namespace. No blocking finding; three internal helpers renamed to lkpi_.
  Not covered: usr/src/common (absent from the reference tree), units that do not compile yet.

- [x] Link closure reached zero unresolved symbols (374 objects) after the import fixes: added
  intel_graphics_stolen_res (Gen9 BDSM/GGC decoding before probe, tested), intel_gmch_* (Gen2-5
  unsupported), vmap purge notifiers.
- [x] R-K7: device_iommu_mapped() reflects illumos immu translation of the GPU (immu_running,
  immu_dvma_enable, immu_gfxdvma_enable; override linuxkpi_iommu_mapped), so intel_vtd_active()
  and the Gen9 VT-d workarounds follow reality; docs/11 section 10.1 has the first-boot advice.
- [x] Test robustness: rcu_list_stress readers always look up once; toy_probe_remove waits for the
  fake backend's claimed-interrupt count (both failed only under heavy host load).

- [x] Independent review findings fixed (status tables at the end of docs/12 and docs/13): DMA by
  page for merged sg entries, size-aware GFP, signal holds, thread_join, interruptible mutexes,
  all-CPU wbinvd, cache attributes, no devinfo walk from attach, no self-hold of the attached dip,
  INTx fallback, kmalloc alignment without over-allocation, I2C block reads. Syntax-check 374/374,
  link check 0 unresolved.

- [x] Real illumos headers: scripts/illumos-hdr-check.sh compiles every module (linuxkpi,
  lkpi_ktest, lkpi_toy, G1, drm, G2, i915) against an /usr/include-like overlay of an illumos-gate
  tree (scripts/illumos-hdr-overlay.sh) with the real -Werror flags and gcc 14's default errors;
  0 errors after renaming list_move_tail, struct sysinfo, sema_init, resume and timer_setup in
  platform.h, guarding/undefining the macros illumos also defines, and declaring the ACPICA,
  xc_call and HAT_PLAT_NOCACHE interfaces locally (their headers are not installed). The CI build
  on OmniOS/OpenIndiana (.github/workflows/build.yml) is the authority; this check reproduces it
  locally.

KPI work still open (behaviour, not syntax):
- [ ] User address-space features refused for now: i915 userptr (no VMAs visible), legacy
  I915_GEM_MMAP (vm_mmap), kernel SSE (SSE4.1 hidden), relay (GuC log), sysctl publishing.

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
