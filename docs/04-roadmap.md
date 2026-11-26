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
- [x] docs/11 R-K1: core/postcore/arch/fs/rootfs_initcall and __exitcall in <linux/module.h>.
- [x] docs/11 R-K3: LKPI_PCI_DRIVER_MODULE_OPS(name, desc, cbops, getinfo, attach, detach, init,
  exit); LKPI_PCI_DRIVER_MODULE now accepts a cb_ops address (NULL told apart by type).

Next (from tests/out/p3/summary.txt):
- [ ] Import requests (third_party, not KPI): include/drm/intel/i915_hdcp_interface.h (blocks 96
  i915 units), intel-gtt.h, drm_mipi_dsi.h, drm_gem_atomic_helper.h, video/cmdline.h,
  drm_dp_mst_topology_internal.h.
- [ ] shmem file implementation (GEM backing store) in drmglue/KPI.
- [ ] Remaining undeclared names in summary.txt (handle_simple_irq, PIDTYPE_PID, WB_SYNC_NONE,
  SWAP_CLUSTER_MAX, EREMOTE, EBADSLT, ...), then fix-ups once units compile further.

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
