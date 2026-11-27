# Independent review, third pass: Gen9.5 bring-up past PCI probe

Status 2026-11-27 (merged up to 7f7d0e7). Reviewer: third independent agent. Read-only on include/, src/,
drmglue/, drm_bsd/, build/, drivers/, tools/; this file and `review/` are the only things written.

Method: the imported v6.14 i915 (`i915/drivers/gpu/drm/i915`) and DRM (`drm/`, `gpl/drivers/dma-buf`)
call paths that run after `i915_pci_probe()` succeeds were followed into the KPI (`include/linux`,
`src/`) and drmglue (`drmglue/src`), and the KPI's illumos backend was read against illumos-gate
master (`usr/src/uts`, sparse clone, Nov 2026). Findings already in docs/10, 11, 12 and 13 are not
repeated. Nothing was built or run on illumos; userland reproducers are in `review/`
(`make -C review`, each exits 1 while its bug is present).

## Summary (top first)

| ID | Sev | Area | Title | Confidence |
|----|-----|------|-------|------------|
| R3-K1 | high | GEM/uaccess | `pagefault_disable()` is a no-op and a kernel-mode fault that returns `VM_FAULT_NOPAGE` is retried forever: execbuffer relocations read from a GEM mmap self-deadlock, then spin unkillably | high |
| R3-M1 | high | GEM/memory | `alloc_pages()` ignores `__GFP_RETRY_MAYFAIL`/`__GFP_NORETRY`: GEM backing store is `KM_SLEEP` non-pageable heap with no cap; a render-node client can pin all RAM and hang the system | high (reproduced) |
| R3-M2 | low | GEM/memory | `mapping_gfp_constraint(m, g)` returns `g` instead of `mapping_gfp_mask(m) & g` | high (reproduced) |
| R3-T1 | low | tasklets | contended tasklet re-queues itself in a tight loop on `system_highpri_wq` | high (fact), low (impact) |

(The table is extended as each area is completed; see the per-area sections.)

## Area 1: GEM (object create, shmem backing, execbuffer2, mmap, dma-buf)

### R3-K1 (high) Kernel-mode faults on GEM mmaps: self-deadlock, then an unkillable fault loop

Where:
- `include/linux/uaccess.h:125-128`: `pagefault_disable()`/`pagefault_enable()` are empty,
  `pagefault_disabled()` and `faulthandler_disabled()` are 0; `__copy_from_user_inatomic` is plain
  `copy_from_user` (`uaccess.h:69-72`).
- `drmglue/src/vma.c:427-438`: when the driver's fault handler returns without installing a PTE
  (`VM_FAULT_NOPAGE`) 16 times, `lkpi_vma_fault()` returns 0 ("the access refaults").
- i915 relies on the Linux behaviour in `gem/i915_gem_execbuffer.c:1550-1563`
  (`eb_relocate_vma`, called from `eb_relocate_parse()` after `eb_validate_vmas()` has locked every
  object of the execbuffer with `eb->ww`): "we cannot handle a pagefault whilst holding the struct
  mutex lest the user pass in the relocations contained within a mmaped bo".

Linux semantics: with page faults disabled, a fault on a user address in the copy is not handled;
the copy returns the uncopied length, `eb_relocate_vma` returns -EFAULT and execbuffer retries in
`eb_relocate_parse_slow()` with every lock dropped. Separately, a kernel-mode fault on a user address
with a signal pending fails the access (`fault_signal_pending()` -> `kernelmode_fixup_or_oops()`), so
`copy_from_user` returns an error instead of retrying.

On illumos: the `copyin()` fault is resolved through `as_fault()` -> segdev -> `drmglue_dm_access()`
-> `lkpi_vma_fault()` -> i915's fault handler, in the ioctl thread that holds the execbuffer locks:
- `vm_fault_cpu()` (WB/WC/UC mmap_offset, `gem/i915_gem_mman.c:267`) calls
  `i915_gem_object_lock_interruptible(obj, NULL)`: `lkpi_ww_lock_common()` warns "recursive
  acquisition" and then waits on itself (`cv_wait_sig`).
- `vm_fault_gtt()` (GTT mmap, `i915_gem_mman.c:368-370`) locks with a fresh ww context whose
  `acquired` is 0, so wait-die does not make it back off: it also waits on itself.

The thread sleeps until a signal arrives. Then the lock returns -EINTR, i915 maps it to
`VM_FAULT_NOPAGE` (`i915_error_to_vmf_fault`), `lkpi_vma_fault()` retries 16 times and returns 0,
`trap()` re-executes the `copyin` instruction, which faults again, and so on: every iteration sees the
signal pending and returns immediately. The lwp never reaches the user boundary, so neither the
signal nor SIGKILL is ever delivered. It spins on a CPU forever, holding the dma_resv locks of every
object in the execbuffer; any other client that touches those objects (a shared window-system
buffer) blocks. Only a reboot clears it.

Trigger: any user with access to `/dev/dri/renderD128` (mode 0666): create a BO, mmap it with
`DRM_IOCTL_I915_GEM_MMAP_OFFSET` (or the legacy GTT mmap) without touching it, write the relocation
array into it through a second mapping or `pwrite`, and pass it as `relocs_ptr` of an object in the
same execbuffer. Gen9 still accepts relocations (they are only rejected on graphics version 12+).
Mesa iris uses softpin and does not do this, so ordinary desktops do not hit it; it is a local
denial of service.

The same refault loop exists for every kernel-mode access (copyin/copyout, `uiomove`) to a GEM mmap
whose fault handler returns `VM_FAULT_NOPAGE` while a signal is pending: i915 returns NOPAGE for
-EINTR, -ERESTARTSYS and -EAGAIN (e.g. a fault during a GPU reset).

Fix (both parts are needed):
1. Make `pagefault_disable()` a per-task counter (`current->pagefault_disabled++`, as Linux) and
   have `lkpi_vma_fault()` return `EFAULT` immediately when the faulting thread's counter is
   non-zero. The fault is taken in the copying thread, so `current` is the right task. `copyin` then
   fails, `__copy_from_user_inatomic` returns `n`, and i915 takes its slow path as on Linux.
   `faulthandler_disabled()` should report the counter.
2. In `lkpi_vma_fault()`, when a pass made no progress (no PTE installed) and the fault comes from a
   kernel access to a user address, return `EFAULT` if `lkpi_plat_signal_pending()` (or after the
   existing 16 tries) instead of 0. The fault handler cannot use `t_lofault` to tell (illumos
   `trap()` saves and clears it before calling `pagefault()`, `i86pc/os/trap.c:634-636`), so the KPI
   copy wrappers (`lkpi_plat_copyin/copyout/copyinstr`) should mark the task
   (`current->lkpi_in_uaccess++` around the copy) and `lkpi_vma_fault()` should test that mark.
   User-mode faults keep returning 0, so signals are delivered on the way back to user mode as today.

Confidence: high for the deadlock and the loop (all steps read: uaccess.h, vma.c, i915_gem_mman.c,
execbuffer, ww_mutex.c; illumos `trap()` T_PGFLT in kernel mode (`i86pc/os/trap.c:584-660`) calls
`pagefault()` for user addresses and returns to the faulting instruction when it returns 0, and
only uses `t_lofault` when the fault could not be resolved). Not run.

### R3-M1 (high) GEM pages are unbounded `KM_SLEEP` kernel memory

Where: `src/linux_page.c:41-43` (`alloc_pages()` passes only `gfpflags_allow_blocking(gfp)` to
`lkpi_plat_contig_alloc()`), `src/plat_illumos_dev.c:745-756` (single pages:
`kmem_cache_alloc(linuxkpi_page, cansleep ? KM_SLEEP : KM_NOSLEEP)`), `src/plat_illumos_dev.c:759-771`
(larger orders: `DDI_DMA_SLEEP`), `src/linux_shmem.c` (`lkpi_shmem_get()` passes the caller's gfp to
`alloc_page()`), `src/linux_intel_gfx.c`/KPI `totalram_pages()` (= `physmem`, the size i915 gives
its system memory region, `gem/i915_gem_shmem.c:724`).

What i915 expects: `shmem_sg_alloc_table()` (`gem/i915_gem_shmem.c:93-140`) tries each page first
without reclaim, then shrinks its own objects twice, and finally uses
`mapping_gfp_mask() | __GFP_RETRY_MAYFAIL`, "since graphics tend to be disposable, defer the oom
here by reporting the ENOMEM back to userspace". On Linux those pages are also swappable shmem.

On illumos the last attempt is `kmem_cache_alloc(KM_SLEEP)`, which never fails: it reaps kmem
caches and waits for free memory. GEM pages are locked kernel heap (no swap, no `availrmem`
accounting against the process, no rlimit), and one object may be as large as `physmem`
(`resource_size(&mr->region)` check at `i915_gem_shmem.c:83`). A render-node client that creates and
populates objects totalling more than free memory (e.g. `GEM_CREATE` + `SET_DOMAIN` in a loop, or a
WebGL page) drives the kernel heap until `freemem` is exhausted; ZFS ARC is squeezed, page-outs
cannot free kernel memory, other kernel `KM_SLEEP` allocations block, and the machine stops making
progress. On Linux the same program gets -ENOMEM (or its pages are swapped).

`review/repro_page_gfp_sleep.c` shows that `alloc_page(GFP_KERNEL | __GFP_RETRY_MAYFAIL)` reaches
the platform with `cansleep = true`.

Fix:
1. `alloc_pages()` should map gfp the way `lkpi_gfp_to_plat()` does for kmalloc: `__GFP_NORETRY` or
   `__GFP_RETRY_MAYFAIL` (and no `__GFP_NOFAIL`) means "may fail" (`KM_NOSLEEP | KM_NORMALPRI`,
   `DDI_DMA_DONTWAIT`). Pass the platform flag, not a bool.
2. Account KPI page allocations against a global limit (for example a tunable defaulting to half of
   `physmem`, or a check of `availrmem` against `tune.t_minarmem` plus a margin) and fail with NULL
   when it is reached, so GEM memory can never take the last of the kernel's memory. Report the same
   limit through `totalram_pages()` (it sizes i915's region and its shrinker targets).
3. Longer term (design): non-pageable GEM memory needs per-process accounting (the illumos
   equivalent is `page_resv`/`availrmem` plus a resource control); note it in docs/06.

Confidence: high for the mapping (reproduced); the system-level effect is from reading
`contig_alloc()`/`kmem_cache_alloc()` semantics, not measured.

### R3-M2 (low) `mapping_gfp_constraint()` returns its argument

Where: `include/linux/pagemap.h:21`. Linux: `mapping_gfp_mask(mapping) & gfp_mask`.

Effects: i915's "no reclaim" first pass (`i915_gem_shmem.c:96`, `~__GFP_RECLAIM`) carries every
other bit, including `__GFP_NOFAIL`, so the `kcalloc()` of the page descriptors and the XArray stores
in `alloc_pages()` become `KM_SLEEP` (the page itself stays `KM_NOSLEEP`, because there is no
`__GFP_DIRECT_RECLAIM`). `drm_gem_get_pages()` (`drm/drivers/gpu/drm/drm_gem.c:607`) would `BUG_ON`
every page above 4 GiB; i915 does not call it today, but any DRM helper that does panics on a machine
with more than 4 GiB of RAM.

Fix: `#define mapping_gfp_constraint(m, g) (mapping_gfp_mask(m) & (g))`, and keep a per-mapping mask
if `mapping_set_gfp_mask()` is ever honoured (i915 sets `GFP_HIGHUSER | __GFP_RECLAIMABLE`,
`i915_gem_shmem.c:603`). Reproduced in `review/repro_page_gfp_sleep.c`. Confidence: high.

### Area 1, verified OK

- `lkpi_shmem_get()` races are resolved with `xa_insert` and the cache reference is dropped on
  truncation and inode release; i915's `shmem_sg_alloc_table()` coalesces only PFN-contiguous pages
  and `dma_map_sg` handles the result (docs/12 M1 fix). Pages are zeroed at first use, as shmem does.
- Internal objects (`i915_gem_object_get_pages_internal`) try high orders first; on illumos a
  contiguous request is `page_get_contigpage()` with `PG_PHYSCONTIG`, which fails rather than waiting
  (`vm_machdep.c` `page_create_io`), so the order fallback works. It is a linear PFN search, slow on
  large machines but not a hang (only `page_resv(KM_SLEEP)` can wait, and only when `availrmem` is
  exhausted, see R3-M1).
- `SLAB_TYPESAFE_BY_RCU` caches (`i915_request`, `i915_dependency`) defer every free by a grace
  period, which is stricter than Linux, and the constructor runs once per buffer as with Linux slabs,
  so `dma_fence_get_rcu_safe()` on a recycled request is safe.
- Relocation and pread/pwrite fast paths use `io_mapping_map_atomic_wc()`, which here returns an
  address inside a permanent WC mapping of the aperture (no per-CPU slot), so blocking inside the
  copy cannot corrupt another thread's mapping. `kmap_atomic`/`kmap_local_page` are
  `page_address()`, likewise safe to block in.
- The mmap_offset path registers the VMA with the mapping of the file *after* the driver's mmap ran
  (`drmglue/src/vma.c:367`), so i915's `vma_set_file(vma, mmap_singleton())` puts GEM mmaps in
  `dev->anon_inode->i_mapping`, which is what `drm_vma_node_unmap()` and
  `i915_gem_object_release_mmap_gtt()` zap.
