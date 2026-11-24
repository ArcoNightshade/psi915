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

## Implementation status (2026-11-24, P1 first slice)
Done = implemented and unit-tested in userland (gcc, clang, ASan/UBSan) and compile-checked in the
illumos kernel configuration against stub headers. Nothing below has been built or run on illumos yet;
every kernel-side mapping is untested there. Partial = the listed subset only.

| Area | Status | What exists | Not yet |
|------|--------|-------------|---------|
| Types/utils | partial | types, list, list_sort, rbtree (+augmented callbacks), kref (+kref_put_lock/_mutex), bits, bitops, bitmap (basic), bitfield, log2, math, math64, minmax, limits, overflow, string (strscpy, kstrdup, kmemdup, ...), err, errno | kstrto*, sort, bsearch, hash, hashtable, rbtree _rcu |
| Atomics/barriers | done | atomic_t/atomic64_t/atomic_long_t with all ordering variants, xchg/cmpxchg/try_cmpxchg, smp_*/mb/rmb/wmb, smp_load_acquire/store_release, smp_cond_load_* (GCC `__atomic` builtins, inline on amd64) | |
| Memory | partial | kmalloc family with 16-byte size header over kmem_alloc, kvmalloc/vmalloc, krealloc, kmem_cache wrappers (incl. SLAB_TYPESAFE_BY_RCU), gfp mapping to KM_* | devm_*/devres (needs struct device, P2), mempool, natural alignment of power-of-two kmalloc sizes |
| Locking | partial | spinlock_t, rwlock_t, mutex, rw_semaphore, seqcount/seqlock, lockdep-lite owner assertions | ww_mutex, semaphore, interruptible mutex/rwsem waits (return 0, never -EINTR) |
| Sleeping/scheduling | not started | only might_sleep() | wait queues, schedule, current, msleep |
| Workqueue/timers | partial | jiffies/HZ (HZ is run-time `hz` on illumos) | workqueue, timer_list, hrtimer, kthread |
| RCU | partial | epoch-based rcu_read_lock/synchronize_rcu/call_rcu/rcu_barrier, poll-state API, kfree_rcu, rculist (list/hlist) | SRCU, rcu-protected rbtree |
| Completions/events | partial | completion (timeouts, interruptible) | kobject, notifier chains |
| Logging/debug | partial | printk/pr_*/dev_* (to cmn_err), ratelimit, WARN*/BUG* with linuxkpi_bug_panic tunable | Linux %p format extensions, dynamic_debug, tracepoints |
| Misc | partial | idr, ida, xarray basic API | xarray marks and xas_* API, uaccess, module params, ktime, debugfs/sysfs |

### Name and layout clashes with illumos (handled or to handle)
- Struct tags: illumos `kmutex_t` is `struct mutex`, so `<linuxkpi/platform.h>` renames the identifier
  `mutex` to `lkpi_mutex` after the environment headers. The same is needed when these arrive:
  `struct page` (P2), `struct file`, `struct module`, `struct cred`, `struct pid` (P3).
- Functions/macros: `mutex_init`, `mutex_destroy`, `kmem_cache_*`, `ffs`, `min`, `max`, `ARRAY_SIZE`
  are redefined with Linux meaning (macros), so illumos headers must be included before linux/ ones.
- `include/asm/`: illumos installs `/usr/include/asm/{atomic,bitmap,byteorder,clock,cpu,cpuvar,htable,
  mmu,sunddi,thread}.h`, which illumos headers include. Our `-I include` would shadow them, so the KPI
  must never add `asm/` headers with those names (Linux code wanting `<asm/atomic.h>` or
  `<asm/byteorder.h>` needs another mechanism, e.g. a separate include dir searched after the system).
- On Linux userland hosts `include/linux/{errno,limits}.h` shadow the UAPI headers glibc includes; both
  chain to the host header with `#include_next`. Other shadowed UAPI names may need the same.
- errno: standard names keep illumos values; Linux-internal codes (ERESTARTSYS, EPROBE_DEFER,
  ENOTSUPP, ...) use the Linux values 512+, and must be translated before reaching userland.

## Top hazards
1. Blocking/scheduling semantics (signals, `current`) vs illumos threads.
2. WC/PAT mapping and cache coherency on Intel (clflush policy).
3. shmem-backed GEM and memory pressure (no swap pager).
4. dma-buf fd export (needed for DRI3, PRIME) requires a pseudo-fs/vnode type.
5. Concurrency correctness: Linux code assumes spinlock-irq/BH semantics; illumos has interrupt priority levels (SPL), so IRQ-safe locks must use `MUTEX_DRIVER` with the right iblock cookie.
6. 32-bit userland ioctl compat (`COMPAT_32`) for existing 32-bit X/Mesa.
