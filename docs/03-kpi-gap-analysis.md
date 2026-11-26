# KPI gap analysis

Difficulty: L low, M medium, H high. Ref = where to look first.

| Area | Linux API | illumos mapping | Diff | Ref |
|------|-----------|-----------------|------|-----|
| Types/utils | types, list, rbtree, kref, bitops, bits, math64, minmax, container_of, string, kstrto*, sort, bsearch, hash | direct C, no kernel dependency | L | freebsd linuxkpi/common/include/linux |
| Atomics/barriers | atomic_t, atomic64, cmpxchg, READ_ONCE, smp_mb | `atomic_*`, GCC builtins, `membar_*` | L | freebsd |
| Memory | kmalloc/kfree, vmalloc, kmem_cache, mempool, devm_* | kmem/vmem wrappers w/ size header; devres list per device | M | freebsd, netbsd |
| Pages/shmem | struct page, alloc_pages, kmap, shmem_file, mm/vm_area | page_t shim + anon-backed object; no swap initially | H | freebsd (vm_page based), gfx-drm drm_gem.c |
| Locking | spinlock, mutex, rwsem, seqlock, lockdep annotations, ww_mutex | kmutex/krwlock; ww_mutex implemented in KPI (needed by GEM) | M | freebsd, openbsd |
| Sleeping/scheduling | schedule(), wait_event*, wake_up, signals, TASK_*, msleep, usleep_range, cond_resched | cv_wait_sig/cv_timedwait with Linux wait-queue struct; `current` shim (task_struct over kthread_t/proc) | H | freebsd (linux_current.c) |
| Workqueue/timers | workqueue, delayed_work, timer_list, hrtimer, kthread | taskq (ordered/serial), cyclic/timeout, kthread_create | M | freebsd linux_work.c |
| RCU | rcu_read_lock, synchronize_rcu, call_rcu, SRCU, rcu_dereference | epoch-based emulation | M | freebsd |
| Completions/events | completion, kobject/uevent, notifier chains | cv + mutex; uevent -> sysevent (hotplug) | L | freebsd |
| Interrupts | request_irq, threaded irq, tasklet, irq_work, synchronize_irq | ddi_intr_* (MSI/FIXED), soft intr for tasklets | M | illumos pci drivers |
| PCI | pci_dev, config r/w, BARs, pci_enable_device, MSI, pci_resource, bus walk, pcie caps, bridges, runtime PM | pci_config_*, ddi_regs_map_setup, ddi_intr; pci_get_device -> devinfo tree walk | M | freebsd, illumos pci |
| IO | ioremap(_wc), readl/writel, memcpy_toio, io-mapping, PAT/WC | ddi_regs_map_setup, devmap WC, `hat_*` attrs | M | illumos gfx_private, gfx-drm |
| DMA | dma_map*, dma_alloc_coherent, dma-mapping API, sg_table, swiotlb, dma_pool | ddi_dma_* handles/cookies; IOMMU via rootnex/immu | H | illumos rootnex, freebsd linux_pci.c |
| dma-fence/dma-buf | dma_fence, dma_resv, dma_buf (export/import/fd), sync_file | implement in KPI (pure logic) + fd via `getf/falloc` with custom vnode | H | freebsd linux_dmabuf?, netbsd drm2/linux |
| File/ioctl plumbing | file_operations, poll, mmap, ioctl, fasync, anon_inode, fd tables | cb_ops (open/close/ioctl/devmap/chpoll), `drm_ioctl` adapter incl. 32-bit compat | H | gfx-drm drm_drv.c/drm_ioctl.c |
| mmap/VMA | vm_operations_struct, remap_pfn_range, vm_insert_page, unmap_mapping_range | devmap/segdev callbacks + hat_devload; unmap = hat_unload | H | gfx-drm, freebsd |
| MM misc | mmu_notifier (userptr), get_user_pages, pin_user_pages, mm_struct | seg-level `as_pagelock`; userptr optional at first | H | openbsd, freebsd |
| ACPI/DMI | acpi_*, opregion, backlight, video, dmi_check_system, ACPI notifiers | ACPI-CA in illumos, smbios for DMI quirks | M | illumos acpica |
| Power mgmt | pm_runtime, dev_pm_ops, suspend/hibernate, cpufreq/pm_qos | DDI_SUSPEND/RESUME; runtime PM stubbed initially | M | illumos |
| Platform | platform_device, i2c, i2c-algo-bit, gpio, regulator, clk | i2c via KPI bit-banging over GMBUS; others stub | M | freebsd, netbsd |
| CPU/arch | cpu_feature (PAT/clflush), clflush_cache_range, wbinvd, cpu_data, MSR, topology | `x86_*`, clflush instruction, rdmsr via `rdmsr()` | M | illumos x86 |
| Firmware | request_firmware | kobj_* file read at attach | L | netbsd |
| Logging/debug | printk/dev_*, WARN, BUG, pr_*, dynamic_debug, tracepoints | cmn_err/ SDT/DTrace probes; tracepoints -> SDT | M | freebsd |
| Misc | debugfs/sysfs/procfs, module params, idr/xarray/ida, ktime, jiffies, random, uaccess (copy_*_user, get_user), seq_file | ddi_copyin/out; params via driver.conf; idr/xarray implement in KPI; sysfs/debugfs stubs | M | freebsd |
| Compiler | statement exprs, typeof, `__attribute__`, `static_assert`, `_Generic`, `-std=gnu11`, kconfig macros | illumos builds with gcc: set gnu11/gnu17; generate `autoconf.h` | L | drm-kmod kconfig.mk |
| TTM/scheduler | ttm, drm_sched, hmm | imported unchanged; sit atop KPI | M | drm-kmod |

## Implementation status (2026-11-26, P1 complete, P2 complete, P3 KPI in progress)
Done = implemented, unit-tested in userland (gcc, clang, ASan/UBSan; `make check`) and compile-checked
in the illumos kernel configuration against hand-written stub headers. NOTHING below has been built
or run on illumos: every kernel-side mapping (DDI calls, TSD, callouts, sysevents, ...) is untested
there. Hardware-facing pieces (P2) are tested in userland against fake devices
(src/plat_userland_dev.c), so their Linux-side logic is exercised but their illumos backend
(src/plat_illumos_dev.c) is only type-checked.

| Area | Status | What exists | Not yet / limits |
|------|--------|-------------|------------------|
| Types/utils | done | types, list, list_sort, rbtree (+augmented), kref, bits, bitops, bitmap, bitfield, log2, math, math64, minmax, limits, overflow, string, err, errno, kstrto*/simple_strto*/_from_user, sort/sort_r (heapsort), bsearch, hash_32/64/ptr, hashtable, uuid/guid | rbtree _rcu, %p printk extensions |
| Atomics/barriers | done | full atomic_t/atomic64_t/atomic_long_t, cmpxchg family, barriers | |
| Memory | done | kmalloc family (8/16-byte header), kvmalloc, kmem_cache, mempool, devres (devm_kmalloc/kstrdup/kasprintf/add_action, groups) | natural alignment of power-of-two kmalloc sizes |
| Pages | done (P2) | struct page (`lkpi_page`) over physically contiguous memory (order 0..10), page_address/kmap (no highmem), virt_to_page/pfn_to_page for KPI pages, vmap/vunmap with cache mode (hat_devload), set_pages_* | set_pages_*/set_memory_* do NOT change cache attributes on illumos (no HAT interface; returns 0, memory stays WB); shmem (P3/drmglue) |
| Locking | done | spinlock_t, rwlock_t, mutex, rwsem, seqlock, lockdep-lite, ww_mutex (wait-die and wound-wait), semaphore | mutex_lock_interruptible never returns -EINTR; ww_mutex/semaphore use one global internal lock |
| Sleeping/scheduling | done | task_struct/`current` (TSD, freed at thread exit), set_current_state/schedule/schedule_timeout/wake_up_process, signals (ISSIG for lwp threads; TASK_KILLABLE polls SIGKILL in 10 ms slices), wait queues and the whole wait_event* family, wait_var_event/wait_on_bit, msleep/usleep_range/fsleep on the hires callout, udelay | no preemption control (preempt_disable is a barrier), no freezer |
| Workqueue/timers | done | workqueues on KPI worker pools (grow on demand, non-reentrant, flush/cancel semantics, ordered, delayed, rcu_work, system_* queues), timer_list and hrtimer on one timer thread (rbtree of deadlines), kthread (create/run/stop/park), kthread_worker over ordered wq, ktime/timespec64 | not over taskq(9F) (cannot cancel entries; see workqueue.h); callbacks run in thread context, not softirq |
| RCU | done | epoch RCU, call_rcu, kfree_rcu, rculist, SRCU (two-counter) | call_srcu; readers above LOCK_LEVEL |
| Completions/events | done | completion, kobject (kref, names, paths), uevent -> sysevent (EC_linuxkpi/ESC_uevent), notifier chains (atomic, blocking, raw) | |
| Device model | done (P2) | struct device/bus_type/device_driver/class/device_type, get/put_device, device_create/destroy, dev_pm_ops, pm_runtime stubs | sysfs (no-op), runtime PM (always active) |
| PCI | done (P2) | pci_dev over dev_info_t ("reg"/"assigned-addresses"), config space, capability and extended-capability walks, pcie_capability_*, enable/master, BAR iomap UC/WC (DDI_MERGING_OK_ACC), pcim_*, ROM, enumeration (pci_get_device/class/domain_bus_and_slot via ddi_walk_devs), driver registry, probe/remove/PM, attach/detach glue (`<linuxkpi/ddi.h>`) | no bridge topology (pci_upstream_bridge = NULL), no I/O BARs via iomap, VGA arbitration stubbed, domain always 0 |
| Interrupts | done (P2) | MSI-X/MSI/INTx via ddi_intr, request_irq/threaded/devm, disable/enable/synchronize, tasklets, irq_work, iblock cookie export | vectors above LOCK_LEVEL refused; one handler per vector |
| IO | done (P2) | readl/writel family, memcpy_toio, ioremap/_wc/_cache, memremap, io_mapping (whole range WC, as Linux x86-64), devm_ioremap | depends on misc/gfx_private |
| DMA | done (P2) | dma_alloc_coherent (+dmam), map_single/page, map_sg/sgtable, sync, masks, sg_table (from_pages with coalescing, iterators, chains), dma_pool | each mapping must be bus-contiguous (sgllen 1): multi-page non-contiguous buffers need the IOMMU; no swiotlb |
| Firmware | done (P2) | request_firmware(/_nowarn/_direct/_into_buf/_nowait) from /usr/lib/firmware via kobj_open_file | no fallback/user helper |
| ACPI/DMI | done (P2) | ACPI_HANDLE (acpica_get_handle), acpi_get_handle/evaluate_object/has_method/_DSM, DMI matching over ksmbios | ACPI video/backlight/lid/notifiers are stubs |
| Platform | done (P2) | I2C core (adapters, transfer, client, smbus byte/block) and i2c-algo-bit (clock stretching, retries, RECV_LEN) | gpio/regulator/clk |
| CPU/arch | done (P2) | CPUID features (boot_cpu_has, X86_FEATURE_*), boot_cpu_data, clflush_cache_range (CLFLUSHOPT), rdmsr/wrmsr, smp_processor_id | wbinvd_on_all_cpus flushes the calling CPU only |
| Logging/debug | partial | printk/dev_* (with %pV %pe %ph %pa %p4cc %pR), print_hex_dump, WARN/BUG, ratelimit, debugfs/procfs (stubs returning "absent"), seq_file into buffers | dynamic_debug, tracepoints (no-ops) |
| Misc | done | idr, ida, xarray (+marks), uaccess (ddi_copyin/out with FKIOCTL mode), module params (driver.conf via explicit table; descriptors module-qualified with LKPI_MODNAME), module_init/exit and all initcall levels, struct pid table | xas_* advanced API (no user in the imported tree) |
| P3 headers | in progress | the headers and helpers the imported DRM/i915 v6.14 code needs to compile: dma-fence/dma-resv/dma-buf/sync_file (BSD, v6.14 layouts), file/inode/dentry/pseudo-fs and mm/VMA types (moved from drmglue), cleanup.h, jump_label, notifier, shrinker, sysctl, pid, string helpers with sscanf, unaligned, hex dumps, Linux printf extensions, many stubs for absent subsystems (OF, pinctrl, relay, CEC, sysrq, vga_switcheroo, cpufreq); drmglue is linked into linuxkpi. Module syntax-check (stub headers): 364/375 units | shmem backing store, user address-space features (userptr, legacy GEM mmap), kernel SSE, link-time closure check; see docs/04 P3 checklist |

### Name and layout clashes with illumos (handled)
- Struct tags: `<linuxkpi/platform.h>` renames `mutex`, `page`, `file`, `module`, `cred` and `pid` to
  `lkpi_<name>` after the environment headers (illumos `struct mutex`, `struct page`, `struct file`,
  `struct module`, `struct cred`, `struct pid`). Every identifier with those names is renamed, so
  `task->pid` is `task->lkpi_pid` and `struct page *page` is `struct lkpi_page *lkpi_page`
  consistently; illumos code keeps the typedefs (kmutex_t, page_t, file_t, cred_t). Every illumos
  header must be included before the first linux/ header.
- Functions/macros: `mutex_init`, `mutex_destroy`, `kmem_cache_*`, `ffs`, `min`, `max`, `ARRAY_SIZE`
  are redefined with Linux meaning (macros), so illumos headers must be included before linux/ ones.
- `include/asm/`: illumos installs `/usr/include/asm/{atomic,bitmap,byteorder,clock,cpu,cpuvar,htable,
  mmu,sunddi,thread}.h`. The KPI's asm/ headers avoid those names (it has barrier, io, page,
  set_memory, cpufeature, processor, msr, cacheflush, smp).
- On Linux userland hosts `include/linux/{errno,limits}.h` chain to the UAPI headers with `#include_next`.
- errno: standard names keep illumos values; Linux-internal codes (ERESTARTSYS, EPROBE_DEFER,
  ENOTSUPP, ...) use Linux values 512+ and must be translated before reaching userland
  (`lkpi_errno_to_illumos()` in the driver glue does it for _init).
- dev_t is the environment's (64-bit on illumos), not Linux's 32-bit encoding.

### Design deviations from doc 02 (with reasons)
- Workqueues run on KPI worker threads, not taskq(9F): taskq entries cannot be cancelled and
  TQ_NOSLEEP dispatch can fail in interrupt context, which would break cancel_work_sync() and
  queue_work() guarantees.
- Timers run on a KPI thread rather than timeout(9F): untimeout() waits for a running callout, which
  deadlocks del_timer_sync() under a lock the callback takes, and callouts cannot tell stale firings
  from re-armed ones without allocation.
- Module parameters need an explicit table at attach time: illumos krtld does not run constructors
  or expose section bounds portably.

## Top hazards
1. Blocking/scheduling semantics (signals, `current`) vs illumos threads.
2. WC/PAT mapping and cache coherency on Intel (clflush policy).
3. shmem-backed GEM and memory pressure (no swap pager).
4. dma-buf fd export (needed for DRI3, PRIME) requires a pseudo-fs/vnode type.
5. Concurrency correctness: Linux code assumes spinlock-irq/BH semantics; illumos has interrupt priority levels (SPL), so IRQ-safe locks must use `MUTEX_DRIVER` with the right iblock cookie.
6. 32-bit userland ioctl compat (`COMPAT_32`) for existing 32-bit X/Mesa.
