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

## Top hazards
1. Blocking/scheduling semantics (signals, `current`) vs illumos threads.
2. WC/PAT mapping and cache coherency on Intel (clflush policy).
3. shmem-backed GEM and memory pressure (no swap pager).
4. dma-buf fd export (needed for DRI3, PRIME) requires a pseudo-fs/vnode type.
5. Concurrency correctness: Linux code assumes spinlock-irq/BH semantics; illumos has interrupt priority levels (SPL), so IRQ-safe locks must use `MUTEX_DRIVER` with the right iblock cookie.
6. 32-bit userland ioctl compat (`COMPAT_32`) for existing 32-bit X/Mesa.
