# Independent review findings

Status 2026-11-26. Reviewer: independent agent, read-only on include/, src/, drmglue/, drivers/, build/.
Method: the psi915 sources at the merge of `origin/psl/epic-gates-dx5xvk` were read against the
illumos-gate sources in `ref/illumos/usr/src/uts` and the imported Linux v6.14 code in drm/, i915/,
gpl/. Nothing was run on illumos. Where a bug is in portable code, a userland reproducer is in
`review/` (`make -C review`; each program exits 1 while the bug is present).

Severity: **blocker** (panic, memory corruption or hang in the normal i915 path), **high** (same, in
a plausible but less common path, or unprivileged DoS), **medium** (misbehaviour under specific
conditions, resource leak, unload-only crash), **low** (performance, diagnostics, latent).
Confidence: how sure the reviewer is that the failure happens as described on illumos.

## Summary (top first)

| ID | Sev | Area | Title | Confidence |
|----|-----|------|-------|------------|
| M1 | blocker | memory/DMA | `dma_map_sg` binds `sg_virt()..+length`; merged sg entries map the wrong kernel memory to the GPU | high (reproduced in model) |
| L1 | high | locking/signals | `signal_pending()` ignores `MUSTRETURN` (fork/exec/proc holds): interruptible waits spin, other lwps' fork hangs | high |
| M2 | high | memory | user-sized `kvmalloc` (execbuffer) becomes `KM_SLEEP`, never fails: unprivileged hang | high (reproduced) |
| L2 | medium | threads | joinable service threads: joiner frees and module unloads while the thread still runs module text | medium |
| D1 | medium | drmglue | zap (`devmap_unload`) of an `F_SOFTLOCK`ed range; aperture rebinding under kernel I/O | medium |
| L3 | medium | workqueue | 16-worker cap on system workqueues (Linux: 256+, concurrency managed) | medium-low |
| M3 | medium | memory | one `ddi_dma` handle + `contig_alloc` per 4 KiB page; per-page DMA bind | high (performance), medium (exhaustion) |
| C1 | low-medium | cache | `wbinvd_on_all_cpus()` flushes only the local CPU | high |
| C2 | low | cache | `set_memory_*`/`set_pages_*` are no-ops although `kmem_override_cache_attrs()` exists | high |
| L4 | low | sched | `task_tgid()`/`task_pid()` create unreferenced `struct pid` entries; pointer validity differs from Linux | high (leak), low (impact) |
| M4 | low | memory | `kmalloc` gives 16-byte, not natural, alignment for power-of-two sizes | high (fact), low (no consumer found) |
| L5 | medium | locking | `mutex_lock_interruptible/_killable` never interrupt: processes unkillable behind a hung holder | high (documented limitation) |
| I1 | low | IRQ | `synchronize_irq` can miss a handler that entered but has not yet incremented `active` | medium |
| S1 | low | module graph | KPI function `free_pages` collides with genunix global variable `free_pages` (vm_page.c) | high (fact), low (impact) |
| A1 | low | attach | console-only fallback after a failed probe may leave the GGTT/plane reprogrammed (invisible console) | low |

Checked and found correct (no finding): see "Verified OK" at the end.

## Area 1: locking, context, RCU, workqueues, timers

### L1 (high) Interruptible sleeps ignore illumos "must return" holds: busy loop and blocked fork

Files: `src/plat_illumos.c:117-124` (`lkpi_plat_signal_pending`), `src/linux_sched.c:239-275`
(`schedule_timeout`), and every Linux wait loop built on them (`wait_event_interruptible*`,
`dma_fence_default_wait`, `mutex_lock_interruptible` paths that sleep, `i915_request_wait`).

illumos `cv_wait_sig()` (uts/common/os/condvar.c, `cv_wait_sig`) returns 0 not only for a signal but
also when `lwp->lwp_sysabort` is set or `MUSTRETURN(p, t)` is true (`ISHOLD(p)`, i.e. another lwp of
the process is in `fork1()`/`forkall()`/`exec`/`/proc` stop and is waiting in `holdlwps()` for this
lwp to reach the user boundary; or `t_activefd.a_stale`), and for schedctl cancellation. It also
returns 0 immediately, without sleeping, while that condition stays true (it calls `setrun(t)` before
`swtch()`).

`lkpi_plat_signal_pending()` only tests `ISSIG(t, JUSTLOOKING)`. Linux wait loops are
"condition false -> signal_pending()? -> schedule()"; with a hold pending, `signal_pending()` is false,
so the loop calls `schedule()` again, `cv_wait_sig()` returns 0 at once, and the thread spins at 100%
CPU until the wait condition becomes true or the timeout expires. Meanwhile the forking lwp is stuck in
`holdlwps()`.

Scenario: Mesa/Firefox (multi-threaded) with one thread in `DRM_IOCTL_I915_GEM_WAIT` (timeout up to
infinite) or `DRM_IOCTL_SYNCOBJ_WAIT`, another thread calls `fork()`/`posix_spawn()`. The spawn blocks
until the GPU work finishes; with an unbounded wait on a fence that only the forking thread would
eventually submit, the process hangs with one CPU spinning. Also hangs `pstack`, `truss -p`, `gcore`.

Fix: make `lkpi_plat_signal_pending()` return true when
`ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t) || lwp->lwp_sysabort || schedctl_cancel_pending()`
(the same set `cv_wait_sig` tests), so Linux code returns `-ERESTARTSYS` -> `EINTR` (already mapped
in `lkpi_errno_to_user`), the lwp reaches the boundary, and libdrm's `drmIoctl()` restarts the ioctl.
`fatal_signal_pending()` stays as is. Also record in `schedule_timeout()` that a 0 return from
`cv_wait_sig` with no signal must end the sleep (currently it does for `intr`, but the caller loops).
Confidence: high (read in condvar.c and proc.h:576).

### L2 (medium) Joinable kernel threads: unload while the thread is still in module text

File: `src/plat_illumos.c:48-65, 99-109`.

The thread sets `done`, `mutex_exit()`s and then calls `thread_exit()`; the joiner wakes on `done`,
frees the descriptor and returns. `lkpi_*_fini` (RCU, timer, workqueue manager and every worker of
`destroy_workqueue`) then lets `_fini` return and krtld unmaps the module text, while the exiting
thread may still be executing the return path of `mutex_exit()`/the call to `thread_exit()` inside
`lkpi_plat_thread_start` (module text). Result: page fault in kernel mode at `modunload` time, rare
but real (the window is a few instructions, wider under preemption by an interrupt).

Fix: use illumos `thread_join(kt_did_t)` (uts/common/disp/thread.c): record `t->t_did` from
`thread_create()` and have `lkpi_plat_thread_join()` call `thread_join(did)` after `done` (or instead
of the done flag). `thread_join` returns only after the thread is off CPU in `thread_exit`.
Confidence: medium (standard illumos unload race; thread_join exists for exactly this).

### L3 (medium-low) System workqueues capped at 16 workers

File: `src/linux_workqueue.c:48, 266-271`.

Every queue, including `system_wq`/`system_unbound_wq`, gets at most `LKPI_WQ_MAX_WORKERS` = 16
threads. Linux system queues allow 256 (per pool, `WQ_DFL_ACTIVE`) and start a new worker when one
sleeps. DRM/i915 queue work items that block on other work items or on fences signalled from work on
the same queue (atomic commit tails `commit_work` on `system_unbound_wq` wait on fences and on
previous commits; `intel_wakeref` delayed puts; `i915_sw_fence` work; `drm_vblank` work). With more
than 16 such items blocked, queued items behind them never run: hang. With 4+ outputs and nonblocking
commits plus GEM fence work this is reachable, but the reviewer did not construct a concrete
17-deep chain from the i915 code. Fix: raise the cap for `max_active <= 0` to 256 (threads are only
created on demand) or spawn a worker whenever all workers of a queue are blocked for >N ms.

### L4 (low) `task_tgid()`/`task_pid()` allocate unreferenced pid objects

File: `src/linux_sched.c:303-369`.

`task_tgid()` creates a list entry with `count == 0` that is never freed unless somebody later takes
and drops a reference; and a pointer obtained from `task_tgid()` is freed as soon as an unrelated
`get_pid()/put_pid()` pair on the same number drops the count back to 0 (Linux keeps it valid for the
task's lifetime). DRM's current uses (`drm_file_alloc`, `drm_file_update_pid`) take a reference
immediately, so no use-after-free was found; the leak is bounded by distinct pids. Fix: give the
task_struct its own reference (taken at `lkpi_task_alloc`, dropped at free) and return that.

### L5 (medium) Interruptible/killable mutex acquisition cannot be interrupted

File: `include/linux/mutex.h:13-15, 103-112`. Documented, but the consequence is worth stating:
i915 and DRM take `mutex_lock_interruptible()` on locks that are held across long GPU waits or
reset handling (e.g. `ggtt->vm.mutex` in eviction/binding paths, `drm_device` master/modeset
locks, `dev->struct_mutex`-like client locks). If the holder is stuck (GPU hang before reset
completes, or the L1 livelock), every other thread queueing on the mutex is in an uninterruptible
`mutex_enter`: `kill -9` does not work and the process cannot exit until the holder returns. Fix:
implement `mutex_lock_interruptible/_killable` as a sleep on a condition variable (owner field +
`cv_wait_sig`, as `lkpi_ww_lock_common` already does for ww mutexes), keeping `mutex_lock` on the
kmutex fast path; with the L1 fix applied to the signal test.

### Area 1, verified OK

- Static `kmutex_t`/`krwlock_t`/`kcondvar_t` zero initialisers are valid (mutex.c). MSI handler
  priority is below LOCK_LEVEL, so adaptive mutexes for spinlocks taken in the i915 IRQ handler are
  legal; `spin_lock_init_irqsafe` exists for anything above.
- `DDI_MERGING_OK_ACC` really gives write-combining: rootnex.c -> `HAT_MERGING_OK` ->
  `PT_NOCACHE|PT_PAT_4K` = PAT index 6 = WC in `PAT_DEFAULT_ATTRIBUTE` (x86_archext.h).
- RCU epoch scheme: the reader (slot store, full fence, data load) / writer (update, seq-cst epoch
  RMW, slot load) pairing is a correct store-buffering pattern; nesting and overflow slots look right.
  `SLAB_TYPESAFE_BY_RCU` frees are deferred by `call_rcu` (stricter than Linux, correct).
- `current` in interrupt threads returns a static task (no `tsd_set` KM_SLEEP in interrupt context).
- Timer engine stale-firing logic and delayed-work cancel/re-arm races (`lkpi_delayed_work_timer_fn`
  re-checks TIMER and `timer_pending`) are correct; lock order wq -> timer is respected.
- `HZ`/`jiffies` are runtime (`hz`, lbolt), consistently used in conversions.

## Area 2: memory, DMA, pages

### M1 (blocker) `dma_map_sg` maps kernel VA ranges that do not exist; GPU DMA hits unrelated memory

Files: `src/linux_dma.c:278-300` (`dma_map_sg_attrs`), `src/plat_illumos_dev.c:550-583`
(`lkpi_plat_dma_map`), `src/linux_page.c:33-80` (`alloc_pages`), `include/linux/dma-mapping.h:165`
(`dma_max_mapping_size` = `SIZE_MAX`).

Each `alloc_page()` is its own `lkpi_plat_contig_alloc()` -> `ddi_dma_mem_alloc` -> `contig_alloc`
(ddi_impl.c), i.e. its own `vmem_alloc(heap_arena)` VA with a freelist page behind it. Consecutive
allocations very often get physically consecutive pages but unrelated kernel VAs. i915's
`shmem_sg_alloc_table()` (i915_gem_shmem.c:150-170) and `sg_alloc_table_from_pages()` merge pages
with consecutive PFNs into one scatterlist entry (max segment = `i915_sg_segment_size()` = 4 GiB-4K
here). `dma_map_sg_attrs()` then calls `lkpi_plat_dma_map(sg_virt(sg), sg->length)`, binding the
kernel VA range starting at the first page. Beyond the first page that VA belongs to some other
allocation (or is unmapped; rootnex then derives a bogus PFN from `hat_getpfnum`). The GGTT/PPGTT
PTEs for pages 2..n of the entry point at foreign memory: GPU reads leak, GPU writes corrupt
arbitrary kernel memory. This is the normal path for every shmem GEM object.

Without an IOMMU (illumos `immu` is off by default) a mapping whose pages are not physically
contiguous fails `ccount != 1` and i915 retries with `max_segment = PAGE_SIZE`, but the merged case
here *is* physically contiguous, so the bind succeeds silently with the wrong pages.

Reproducer: `review/repro_dma_sg_kva.c` (real KPI sources, userland platform, only
`lkpi_plat_virt_to_phys` replaced by a model with consecutive physical but non-consecutive virtual
pages): "device page 1 -> kva ...60000, expected ...61000: FAIL".

Fix (any of): (a) map by physical address, not VA: per sg entry, walk its pages with
`page_to_phys()`/`lkpi_phys` and bind them through a `struct buf` with `B_PAGEIO`/`b_pages`
(`ddi_dma_buf_bind_handle`) or, since the pages are already locked contig pages, bind each page and
require the returned cookies to be contiguous (else split the entry); (b) at minimum return
`PAGE_SIZE` from `dma_max_mapping_size()`/`dma_get_max_seg_size()` and make
`sg_alloc_table_from_pages*` honour it, so no entry ever spans two separately allocated pages; note
that order>0 `alloc_pages()` blocks are VA-contiguous and would be fine. (b) also needs every
other place that builds multi-page entries from independent pages (dma-buf import, userptr when
added) to be checked. Confidence: high.

### M2 (high) Unbounded user-sized allocations become `KM_SLEEP` (unprivileged hang)

Files: `include/linux/gfp.h:74-83` (`lkpi_gfp_to_plat`), `include/linux/slab.h:53`
(`LKPI_KVMALLOC_MAX` = 16 GiB), `src/linux_slab.c:32-58`.

`GFP_KERNEL` (with or without `__GFP_NOWARN`) maps to `KM_SLEEP`, which never fails: `kmem_alloc`
waits in `vmem_alloc(VM_SLEEP)` and drives system-wide reaping until memory appears. Linux
`kvmalloc` of an unsatisfiable size fails. User-controlled sizes reach this path, e.g.
`i915_gem_execbuffer2_ioctl` -> `kvmalloc_array(count + 2, eb_element_size(), __GFP_NOWARN |
GFP_KERNEL)` with `buffer_count` from the user (only bounded by `SIZE_MAX / sz`), DRM property
blobs, syncobj arrays (`drm_syncobj_array_find`, `kvmalloc_array(count_handles, ...)`). renderD128 is
0666, so any local user can make the ioctl thread block forever and starve the system of memory
(ARC, page cache) while it waits. Reproducer: `review/repro_gfp_huge_sleep.c` (evaluates the mapping
for an 8 GiB request).

Fix: in `lkpi_alloc_hdr`, use `KM_NOSLEEP | KM_NORMALPRI` for any request above a threshold
(e.g. 1 MiB, or anything kvmalloc-sized without `__GFP_NOFAIL`), and refuse requests above a
fraction of `physmem` outright. Keep `KM_SLEEP` only for small `GFP_KERNEL` and `__GFP_NOFAIL`.
Confidence: high.

### M3 (medium) Per-page `ddi_dma` handles and `contig_alloc` for every 4 KiB page

Files: `src/linux_page.c:33-80`, `src/plat_illumos_dev.c:629-670` (`lkpi_plat_contig_alloc`),
`src/linux_dma.c` (one `lkpi_dma_rec` + one `ddi_dma_handle` per mapped segment).

`ddi_dma_attr` with `dma_attr_align = PAGESIZE` and `sgllen = 1` makes `i_ddi_mem_alloc` compute
`minctg = btopr(len + align) = 2 > 1`, so even a 4 KiB `alloc_page()` goes through
`contig_alloc()` (physically-contiguous search under the global contig lock, plus a `putctgas`
hash entry). A 256 MiB shmem object is 65536 such allocations, 65536 root-node DMA handles, and after
M1 is fixed, up to 65536 bind handles and hash records in a 256-bucket table under one spinlock
(`lkpi_dma_lock`). Expect very slow object creation and high kernel memory overhead; with many
objects, possible exhaustion of the `heap_arena` and long `lkpi_dma_lock` hold times
(`lkpi_dma_sync_single` walks the whole table on a miss). Fix: allocate order-0 pages with
`page_create_va()` on a private vnode (or `kmem_alloc(PAGESIZE)` from a page-aligned cache and
`hat_getpfnum`), not DMA memory; keep DMA mapping for `dma_map_*`. Size the DMA hash with the
number of mappings or keep the record in `sg->lkpi_dma` only.

### M4 (low) `kmalloc` alignment

File: `src/linux_slab.c:23-58`. The 16-byte header gives `ARCH_KMALLOC_MINALIGN` (16) but not the
natural alignment Linux guarantees for power-of-two sizes since 5.4 (`kmalloc(4096)` is
page-aligned on Linux, `kmem_alloc(4112)` + 16 here is not). No consumer in the imported drm/i915 set
was found (`virt_to_page`/`sg_set_buf` on kmalloc memory only in ttm_pool.c, not used by Gen9). Note
for future imports. Also wastes memory: `kmalloc(PAGE_SIZE)` uses the 4608-byte kmem cache.

## Area 3: MMIO, cache coherency

### C1 (low-medium) `wbinvd_on_all_cpus()` is local only

Files: `src/linux_cpu.c:135-140`, `src/plat_illumos_dev.c:906-911`. Used by i915 in
`i915_ggtt_resume` (intel_ggtt.c:1622), `i915_gem_suspend_late`/`i915_gem_freeze_late`
(i915_gem_pm.c:186/223) and dma-buf import of LLC-bypassing objects (i915_gem_dmabuf.c:262), and by
drm_cache.c fallbacks. Dirty lines of other CPUs are not written back; after resume, objects written
by other CPUs and scanned out or read uncached by the GPU can show stale data. The thread may also
migrate during the call. Fix: `xc_call(0, 0, 0, CPUSET_ALL, wbinvd_xc)` (uts/i86pc/os/x_call.c) or
`xc_sync`, with the handler issuing `wbinvd`. Low impact on KBL-R (LLC) except around suspend/resume.

### C2 (low) Cache attribute changes are silently ignored

File: `src/plat_illumos_dev.c:690-706`. The comment says illumos has no interface to change a kernel
mapping's cache attribute; `kmem_override_cache_attrs(caddr_t, size_t, uint_t order)`
(uts/i86pc/os/ddi_impl.c:1169, global) does exactly that for heap mappings (unload + `hat_devload`
with the new `HAT_*` order). Gen9 i915 does not call `set_pages_*`/`set_memory_*` in its normal path
(TTM does), so impact is low; but reporting success for UC/WC requests can hide future aliasing bugs.
Fix: implement with `kmem_override_cache_attrs()` for VA from `heap_arena`, or return `-EOPNOTSUPP`.

### Area 3, verified OK

- `readl/writel` are volatile accesses with compiler barriers; `wmb()` is `sfence`, `mb()` is
  `mfence`; same as Linux x86 for UC and WC.
- `clflush_cache_range` fences both sides; the `clflushopt` encoding (`0x66` + `clflush`) is the
  one Linux uses.
- `ioremap_wc`/io-mapping go through `ddi_regs_map_setup(DDI_MERGING_OK_ACC)` or
  `gfxp_map_kernel_space(GFXP_MEMORY_WRITECOMBINED)`; both reach PAT WC (see Area 1 note).
- `lkpi_plat_vmap` uses `hat_devload` on RAM pages; this would panic for unlocked relocatable pages
  (hat_i86.c `hat_devload`), but KPI pages come from `contig_alloc`, which leaves them
  `page_downgrade`d (SE_SHARED held), so `PAGE_LOCKED` holds. Keep this invariant if M3 changes the
  allocator (pages must stay locked or `PP_ISNORELOC`).

## Area 4: drmglue (in progress)

### D1 (medium) Zap of a soft-locked range; no protection for kernel I/O through GEM mmaps

Files: `drmglue/src/vma.c:74-85, 383-461`, `drmglue/src/segdev_illumos.c:124-153`.

`lkpi_vma_fault(F_SOFTLOCK)` loads translations with `HAT_LOAD_LOCK` for kernel I/O (raw/direct
I/O, aio, `as_fault(F_SOFTLOCK)` fallbacks of `as_pagelock`) on a GEM mmap. A concurrent zap
(`unmap_mapping_range` from i915 eviction or `i915_gem_object_release_mmap_gtt`) calls
`devmap_unload` -> `hat_unload(HAT_UNLOAD)` on the softlocked range. The x86 HAT tracks lock counts
per htable, so this does not panic, but (a) the kernel I/O already holds the old PFNs: for GTT
(aperture) mmaps, i915 rebinds that aperture range to another object, so a disk read lands in another
object's memory (data corruption), and (b) the later `F_SOFTUNLOCK`'s `hat_unlock` finds no PTE and
leaks an htable lock count (htable never freed). Linux refuses `get_user_pages` on `VM_PFNMAP|VM_IO`
mappings, so this cannot happen there. Fix: fail `F_SOFTLOCK` in `drmglue_dm_access`
(`return (FC_MAKE_ERR(ENOTSUP))`-equivalent EFAULT) for `VM_PFNMAP|VM_IO` VMAs, which makes
`as_pagelock` callers fall back to copying (physio then fails cleanly with EFAULT), or keep a
per-VMA softlock count and make zaps wait. segdev_unmap already refuses unmap while
`softlockcnt > 0`, so only the zap side is exposed. Confidence: medium (path is uncommon).

### Area 4, verified OK so far

- segdev_unmap unloads user translations before calling the `devmap_unmap` callback, so
  `vm_ops->close` (possible last object reference) never runs while user PTEs still point at the pages.
- Zappers hold `i_mmap_rwsem` (read) and `lv->lock`; `lkpi_vma_destroy` removes the VMA under the
  write lock before the dhp can be freed, so `devmap_unload` never sees a freed handle; DEAD VMAs are
  skipped.
- The fault path copies window translations to the user under `lv->lock`, so a zap cannot unload the
  window between "present" and `devmap_load` (no PFN_INVALID loads).
- drmglue anonymous files: `ufalloc` + `setf(fd, NULL)` is the correct illumos reserve/unreserve
  pair for `get_unused_fd_flags`/`put_unused_fd`; `falloc(vp, flag, &fp, NULL)` returns with
  `f_tlock` held and the code drops it. Not verified: which reference the new `file_t` consumes
  (falloc does not `VN_HOLD`); the KPI side (`fd_install` in drmglue/src/file.c) must hand over one
  vnode hold per fd, otherwise a dma-buf fd passed over a socket and closed twice would free the
  vnode early. Reviewer did not finish tracing this; marked for the drmglue owner.
- `lkpi_errno_to_user` maps `ERESTARTSYS` and the other >=512 Linux-only codes.
- illumos `_IOC` size field is 8 bits; `lkpi_ioc_translate` recovers sizes >= 256 from the driver
  table and checks the low byte.

## Area 5: attach/detach, interrupts

### I1 (low) `synchronize_irq` race window

File: `src/linux_irq.c:77-94, 172-182`. `synchronize_irq` waits for `d->active == 0`, but the
counter is incremented inside the handler, so a handler that the hardware already dispatched (the
interrupt thread is running `lkpi_intr_handler` in `plat_illumos_dev.c` but has not reached the
`atomic_inc`) is not waited for. i915 uses `intel_synchronize_irq()` after masking interrupts in
`intel_irq_suspend()`/runtime-PM disable and then powers the device down; the late handler reads
`GEN8_MASTER_IRQ` of a powered-down device. i915 checks `intel_irqs_enabled()` first, so the
practical effect is a spurious `IRQ_NONE`, not corruption. `free_irq` is safe because
`ddi_intr_disable`/`ddi_intr_remove_handler` wait for running handlers (`rem_avintr` ->
`wait_till_seen`). Fix: also call `ddi_intr_disable`+`ddi_intr_enable` (or keep the handler entry
count in the illumos wrapper before calling into the KPI) when strict synchronisation is needed;
low priority.

### A1 (low) Console-only fallback after a partially successful probe

File: `drivers/i915_glue/i915_ddi.c` (`psi915_attach`, fallback branch). The fallback keeps tem on
the firmware framebuffer, but `i915_driver_probe` may already have reset/cleared the GGTT outside the
initial-plane reservation, sanitised the display, or disabled the primary plane before failing
(e.g. firmware load or GuC/DMC errors are late in probe). tem then draws into an aperture range whose
PTEs point at scratch, or into a plane that is off: no crash, but the "probe failure must not leave
the system without a console" requirement (docs/11 section 2) is not guaranteed. Suggest: on
fallback, log loudly to the serial console and document that a probe failure after
`i915_ggtt_init_hw` loses the framebuffer console; or keep `psi915_nomodeset` as the recovery path.

### Area 5, verified OK

- `_init` runs the Linux module init before `mod_install` and undoes it on failure; `_fini` runs
  `mod_remove` first. `pci_register_driver` only records the driver (no probe of foreign dips).
- `DDI_SUSPEND` rolls the Linux side back with `DDI_RESUME` when `gfxp_fb_detach` fails; detach
  refuses while the console or any DRM node is open; the panic callback is removed before the softc
  is freed.
- The IRQ path refuses vectors above LOCK_LEVEL; MSI on KBL-R is below it.

## Area 6: module graph and symbol collisions

### S1 (low) `free_pages` is also a genunix global

Method: `nm -g --defined-only` of the 56 objects built by `make -C tests check-kmod` (linuxkpi with
drmglue, 776 global symbols) compared with every function defined at line start in
`ref/illumos/usr/src/uts/**/*.c` and the global variables of common/os, common/vm, i86pc/os.
Overlaps: `_init/_info/_fini` (expected), `cancel_work`, `get_device`, `i2c_transfer` (all static
or in unrelated sun4u/IB/RSM modules: no conflict), and `free_pages`: KPI function
(`src/linux_page.c:118`) versus `int free_pages = 1;` in genunix (`uts/common/vm/vm_page.c:2834`).

krtld (`kobj_lookup_all`, common/krtld/kobj.c) resolves a module's undefined symbols through its
own DT_NEEDED list before the primary modules, and i915/drm list `misc/linuxkpi` directly, so the
calls bind to the KPI function. Consequences are diagnostic: `mdb -k`'s `free_pages` and DTrace
`fbt::free_pages:` name the genunix variable / both; any future module that uses `free_pages`
without listing linuxkpi first would call into a data object (panic). Fix: rename to
`lkpi_free_pages` with `#define free_pages lkpi_free_pages` in `<linux/gfp.h>`, the scheme already
used for `kmem_cache_*` and `mutex_init`. Not checked: the imported drm/i915 objects (they do not
link yet); the same comparison should be rerun on `build/*/obj64/objs` once they compile
(`scripts/kpi-linkcheck.sh` is the place).

## Not verified / limits of this review

- Nothing was compiled against real illumos headers or run on illumos; all illumos behaviour above
  is from reading `ref/illumos/usr/src/uts` (f71cb08e8c53).
- Not reviewed in depth: `src/linux_xarray.c`, `linux_idr.c`, `linux_devres.c`, `linux_device.c`,
  `linux_pci.c` (beyond driver registration), `linux_i2c.c`, `linux_firmware.c`, drmglue `file.c`,
  `anonfile.c`, `pseudofs.c`, `vmoff.c`, `i915_drm_glue.c`, `build/` makefiles, SRCU.
- The workqueue cap (L3) has no concrete 17-deep i915 chain; the D1 scenario was not traced to a
  specific userland program.
- `segdev_faultpage` behaviour for a window page that is not loaded (PFN_INVALID) was not read; the
  code avoids that state under `lv->lock`, which the reviewer did verify.
