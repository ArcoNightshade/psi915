# DRM character device, GEM mmap and dma-buf on illumos (drmglue)

Status 2026-11-25: designed and implemented in `drmglue/`. The portable core is unit-tested in
userland against a simulated illumos layer; the illumos backends are compile-checked against
stub headers only. **Nothing here has been built or run on illumos.** Section 6 lists exactly
what is and is not tested; section 3.7 lists the illumos behaviours the design relies on that
must be confirmed on a real system.

| Piece | Files | Tested |
|-------|-------|--------|
| ioctl/errno/poll translation | `drmglue/src/xlate.c`, `include/drmglue/xlate.h` | userland unit tests |
| Linux file layer (struct file, fput, ioctl/read/poll dispatch) | `src/file.c`, `include/drmglue/lkpi_fs.h`, `glue.h` | userland, simulated backend |
| /dev/dri nodes, clone table | `src/cdev.c` | userland, simulated backend |
| VMA emulation (mmap, fault, zap, fork, split) | `src/vma.c`, `include/drmglue/lkpi_mm.h` | userland, simulated segdev incl. a race test |
| interval tree, fake-offset manager | `src/itree.c`, `src/vmoff.c` | userland unit tests |
| anonymous files, fd plumbing | `src/anonfile.c`, `include/drmglue/anonfile.h` | userland, simulated fd/vnode layer |
| dma-buf core | `src/dma_buf.c`, `include/linux/dma-buf.h` | userland |
| sync_file | `src/sync_file.c`, `include/linux/sync_file.h` | userland, with a test-only dma_fence |
| cb_ops, minor nodes, poll heads | `src/cdev_illumos.c`, `include/drmglue/illumos.h` | stub compile only |
| devmap/segdev backend | `src/segdev_illumos.c` | stub compile only |
| pseudo vnodes for anonymous files | `src/anonfs_illumos.c` | stub compile only |

`make -C drmglue check` runs both the tests and the stub compile with a symbol allowlist
(`drmglue/kmod_allowed_syms.txt`).

## 1. Constraints and approach

- Licence: all drmglue code is BSD-2-Clause and clean-room from documented Linux API behaviour.
  Linux's `dma-buf.c`, `sync_file.c` and `include/linux/dma-buf.h` are GPL-2.0-only and were not
  used. illumos-gate (CDDL) and illumos/gfx-drm (MIT) sources were read to learn the illumos
  interfaces and the gfx-drm precedent; no code was copied from either. Where the design follows
  gfx-drm (`drm_sunmod.c` clone opens, `drm_gem.c`/`i915_gem.c` GTT kernel window) the source
  files say so.
- The imported DRM core and i915 run unchanged above this layer wherever possible: drmglue
  emulates the Linux objects they use (`struct file`, `file_operations`, `vm_area_struct`,
  `vm_fault`, `address_space`, anonymous inodes) and adapts illumos entry points to them, the way
  FreeBSD's LinuxKPI adapts its cdevsw and pager.
- The glue is split into a portable core and a small backend interface
  (`include/drmglue/backend.h`): kernel windows, user translations, poll heads, vnodes and
  descriptors. The illumos backend implements it with DDI/VFS calls; `drmglue/tests/sim_backend.c`
  implements it with a model of the same semantics, so the lifecycle and locking logic runs in
  every test build.
- Linux-named types that belong in the KPI (`linux/fs.h`, `mm.h`, `poll.h`, `anon_inodes.h`,
  `file.h`) live in `drmglue/include/drmglue/` for now because the KPI core is edited
  concurrently; section 5 requests moving them.

## 2. The DRM character device

### 2.1 Minor numbers and clone opens

One `cb_ops` (`drmglue_cb_ops`) serves every DRM minor of the driver. dev_t minor layout:

```
bits 0-7   node: the Linux DRM minor (0-63 card, 128-191 renderD); 0xff = anon mmap host
bits 8-31  clone id: 0 = the node in /devices, 1.. = one per open(2)
```

Every `open(9E)` allocates the lowest free clone id (`src/cdev.c`, KPI idr), creates a Linux
`struct file`, calls `f_op->open(inode, file)` (in the real system `drm_stub_open`, which replaces
the fops with the driver's and reaches `drm_open`, which allocates the `struct drm_file`) and
returns a new dev_t. Because each open file description has its own dev_t, specfs calls
`close(9E)` exactly once per description, after the last `dup`/`fork` copy is closed: that is
Linux's `f_op->release` time. gfx-drm used the same scheme (9-bit node, rest clone id).

Choice of layout: the low byte is the Linux minor, so `iminor()` and libdrm's minor-type logic
see Linux numbers after masking (`minor & 0xff`). Clone ids are allocated lowest first, so the
first 1023 simultaneous opens of a node give minors below 2^18, which a 32-bit process's
`fstat()` can represent (`L_MAXMIN32`); beyond that, 32-bit `fstat()` on the fd fails with
EOVERFLOW (64-bit processes are unaffected). Opening a clone dev_t directly (mknod) is refused.

Userland requirement: libdrm must derive the node type from `minor(st_rdev) & 0xff`. The
oi-userland libdrm patches for gfx-drm already special-case illumos minors; they need checking
against this layout (open item U1).

### 2.2 Node creation and /dev/dri

`drmglue_attach_node(dip, node, name, fops, kcmd)` registers the Linux minor with its fops and
creates the minor node with `ddi_create_minor_node(dip, "card0"|"renderD128", S_IFCHR,
minor(node, 0), DDI_NT_DISPLAY_DRM, 0)`. The driver calls it from attach after
`drm_dev_register()` has assigned DRM minors (requested change R11 covers wiring this to
`drm_minor_register`). The /dev/dri links come from one devlink.tab line in the driver package:

```
type=ddi_display:drm	dri/\M0
```

(`\M0` is the minor name, so links are /dev/dri/card0 and /dev/dri/renderD128; unverified on
illumos, item V9.) `drmglue_getinfo()` maps any of our dev_t values, including clone and anon host
minors, to the owning dip and instance.

### 2.3 Entry points

| cb_ops | Linux equivalent | Notes |
|--------|------------------|-------|
| open | `f_op->open` | FREAD/FWRITE -> FMODE_*, O_RDWR..., FNONBLOCK/FNDELAY -> O_NONBLOCK |
| close | last `fput` -> `f_op->release` | deferred by specfs until the last mapping is gone too |
| read | `f_op->read` per iovec | after the first bytes no further blocking (whole DRM events) |
| ioctl | `unlocked_ioctl` / `compat_ioctl` | section 2.4 |
| chpoll | `f_op->poll` | section 2.5 |
| segmap / devmap | `f_op->mmap` | section 3 |

`f_flags & O_NONBLOCK` is refreshed from the per-call file flags (`uio_fmode`, ioctl `mode`),
which illumos passes from the open file description, so `fcntl(F_SETFL)` is honoured.

Release timing matches Linux: specfs does not call `close(9E)` while the device is still mapped
(`spec_close` checks `s_mapcnt`; `spec_delmap` calls `device_close` when the last mapping goes),
and Linux keeps the file alive through `vma->vm_file`. On process exit, `closeall()` runs before
the address space is freed, so release happens from `relvm()` via `spec_delmap` in the exiting
thread. Release may run in a process other than the opener (fd passed or inherited), as on Linux.

### 2.4 ioctl adapter

Encodings. libdrm and Mesa's drm-uapi headers use `<sys/ioccom.h>` on illumos (their
`#ifdef __linux__` else branch). That encoding differs from Linux's: the direction bits are
swapped (`IOC_IN` = bit 31 means copy-in, where Linux bit 31 means copy-out) and the size field
is 8 bits (`IOCPARM_MASK 0xff`), so structures of 256 bytes or more are encoded with a truncated
size. An incoming word cannot be classified from its bits (an illumos `_IOR` has the bit pattern
of a Linux `_IOW`), so the accepted encoding is a tunable, `drmglue_ioctl_abi`
(0 = illumos, default; 1 = Linux), and `lkpi_ioc_translate()` converts to the Linux word the
driver expects:

- direction: IN -> WRITE, OUT -> READ, VOID -> NONE; VOID combined with data, or nonzero bits
  24-28, is ENOTTY;
- size: Linux `drm_ioctl` deliberately copies `min(user, kernel)` and zero-fills so old and new
  userland work, so the user's size must be kept when known. If the kernel's definition for
  (type, nr) is below 256 bytes the encoded size is exact and is kept. If it is 256 bytes or
  more, the low byte must match the kernel size and the kernel size is used; anything else is
  EINVAL rather than a guess. The kernel definition comes from a per-node callback (`kcmd`)
  that the DRM adapter implements from `drm_ioctls[]` and `driver->ioctls[]` (R11).

32-bit callers: when `ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32` the call goes to
`f_op->compat_ioctl` (DRM's `drm_compat_ioctl`, i915's `i915_ioc32_compat_ioctl`), which converts
the layouts that differ; a NULL `compat_ioctl` is ENOTTY, as on Linux. `in_compat_syscall()` must
answer true there; requested from the KPI as "current process data model is ILP32" (R6), which
needs no per-call state.

Kernel callers (`FKIOCTL`, e.g. an in-kernel `ldi_ioctl`): the adapter sets
`current->lkpi_uaccess_kernel` for the duration, which the KPI's user-copy primitives honour.

Results: negative Linux errno values go through `lkpi_errno_to_user()`: ERESTARTSYS and the other
restart codes -> EINTR (libdrm's `drmIoctl()` retries on EINTR, which is what Linux's transparent
restart achieves), ENOIOCTLCMD -> ENOTTY, ENOTSUPP -> ENOTSUP, EJUKEBOX -> EAGAIN, remaining
Linux-only codes -> EINVAL or EIO. Non-negative results become `*rvalp`.

### 2.5 poll and DRM events

DRM core implements the event queue (`drm_read`, `drm_poll`, `event_wait`); drmglue only has to
connect Linux wait queues to illumos poll heads. Each file has one backend poll head. When
`f_op->poll` calls `poll_wait(file, wq, pt)`, the glue's `_qproc` adds a persistent
custom-function entry (KPI `init_waitqueue_func_entry` + `add_wait_queue`) to that queue, once
per queue (at most 4 per file). Every `wake_up*()` on the queue then calls the entry, which
records the events (the wake key, or all) and wakes the file's poll head. Entries are removed
before `f_op->release` frees the queues. This differs from Linux, where poll-table entries live
for one `poll()` call, because an illumos poll head stays attached until the file goes away.

The illumos wakeup is deferred: `pollwakeup(9F)` must not be called while holding locks the
`chpoll` path takes, and Linux wake-ups run under the wait-queue spinlock and often in the
interrupt handler (vblank events). `ph_wake` ORs the events into the head and dispatches a
preallocated `taskq_ent_t` (`taskq_dispatch_ent`, cannot fail) on a one-thread taskq that calls
`pollwakeup`. Teardown: by the time `ph_free` runs no wake source remains, so only an
already-queued task can reference the head; whichever of `ph_free` and the task runs last frees
it (`cdev_illumos.c`).

`chpoll` returns the poll head when nothing is ready and `!anyyet`, or always for `POLLET`
(epoll edge-triggered, per current chpoll(9E)). EPOLL*/POLL* bits are translated explicitly
(`POLLRDHUP` differs; `POLLWRNORM` equals `POLLOUT` on illumos).

### 2.6 struct file lifecycle

`struct lkpi_file` (renamed from `struct file` by macro, as the KPI does for `struct mutex`,
because illumos `<sys/file.h>` owns the tag) carries the Linux members DRM uses plus glue state.
`f_count` counts Linux references: the clone table or the anon vnode holds one, VMAs hold one
each through `vm_file`, kernel users (`fget`, `dma_buf_get`, prime caches) hold their own.
`fput()` of the last reference removes the poll entries, calls `f_op->release` and frees.
Unlike Linux, which defers the final `__fput` to task work, the release runs synchronously in
the thread dropping the reference. A driver that calls `fput()` while holding a lock its own
release takes would deadlock; none is known in DRM/i915 6.x (risk K3, section 7).

## 3. GEM mmap over devmap/segdev

### 3.1 What has to be emulated

In Linux 6.x, `mmap(fd, off)` on a DRM fd reaches `drm_gem_mmap` (or `i915_gem_mmap`), which looks
the fake offset up in the `drm_vma_offset_manager`, takes an object reference, and sets
`vm_ops`/`vm_private_data`/`vm_page_prot` on the VMA. PTEs are installed later by the fault
handler (`vm_fault_cpu`, `vm_fault_gtt`, shmem helpers) with `vmf_insert_pfn*`,
`remap_pfn_range` (i915's `remap_io_sg`/`remap_io_mapping`), `vm_insert_page` or by returning
`vmf->page`. When an object moves (GGTT eviction, fence loss, purge, shrinker), the driver calls
`drm_vma_node_unmap` -> `unmap_mapping_range(dev->anon_inode->i_mapping, ...)`, which zaps every
PTE of every process mapping that offset range; the next access faults again. `vm_ops->open`
and `->close` keep the object reference per VMA across fork and partial munmap.

### 3.2 Fake offsets

The DRM core's own `drm_vma_manager.c` (MIT, imported) stays the GEM offset manager; offsets
reach drmglue only as the mmap offset. `lkpi_vmoff` (`src/vmoff.c`) is a clean-room manager with
the same contract (page-granular nodes, lookup of a wholly contained range under a read lock,
counted per-file allow tags). It exists so the mmap path can be tested without the DRM import
and as a bring-up fallback if `drm_mm` lags; allocation is O(n) first-fit, acceptable only as a
fallback.

32-bit userland: on 64-bit kernels DRM places fake offsets at 4 GiB and up
(`DRM_FILE_PAGE_OFFSET_START`). A 32-bit process can only pass such offsets through `mmap64`
(largefile builds). Mesa and libdrm are built with largefile support on OpenIndiana; any 32-bit
consumer that is not will get EINVAL (open item U2).

### 3.3 The mmap path

```
mmap(2) -> spec_map -> cb_segmap = drmglue_segmap
  drmglue_segmap_file: MAP_SHARED only; PROT_WRITE needs FMODE_WRITE
    lock f_lkpi_mmap_lock; f->f_lkpi_mmap = {prot, maxprot, flags}
    devmap_setup(dev, off, as, addrp, len, ...)            [segdev, same thread]
      cb_devmap = drmglue_devmap -> drmglue_devmap_file
        lkpi_vma_create: alloc VMA + kernel window, vm_file = get_file(f),
                         f_op->mmap(f, vma)  (DRM/i915 mmap; may prefault)
                         register VMA in vm_file->f_mapping->i_mmap at [vm_pgoff, +n)
        gfxp_devmap_umem_setup(dhp, dip, &callbacks, window cookie, 0, len, maxprot,
                               attribute from vm_page_prot)
        req->lv = VMA
      segdev_create -> devmap_map callback: take req->lv, bind to the handle
    f->f_lkpi_mmap = NULL; unlock; if the VMA was never claimed, destroy it
```

`cb_segmap` is used instead of the default `ddi_devmap_segmap` only to learn `prot`/`maxprot`/
`flags`, which `devmap(9E)` does not receive. `vm_start` is synthetic (0x10000000):
`devmap(9E)` runs before segdev assigns the user address, and drivers only use addresses relative
to `vm_start`. One devmap handle covers the whole mapping.

### 3.4 Kernel windows, cache attributes, PAT

illumos devmap cannot take arbitrary pfns per page from a driver, but `segdev_faultpage()`
resolves a KMEM_NON_PAGEABLE umem cookie through `hat_getpfnum(kas.a_hat, cookie_va + off)`.
So each VMA gets a kernel VA window with no translations (`gfxp_alloc_kernel_space`), wrapped in
a cookie (`gfxp_umem_cookie_init`). A Linux "PTE insertion" loads one window page
(`gfxp_load_kernel_space` = `hat_devload(kas, ..., HAT_LOAD_LOCK)` with the cache attribute);
`devmap_load()` then copies the translation to the user's hat with the handle's attribute. A zap
is `devmap_unload()` on the user range plus `gfxp_unload_kernel_space()` on the window. This is
gfx-drm's GTT scheme (`gtt_map_kaddr`) generalised from one fixed aperture range per object to
per-page, per-VMA windows with arbitrary pfns (system memory pages and aperture pages alike).

Cache attributes: `vm_page_prot` after `f_op->mmap` selects the handle attribute through the
access attributes `gfxp_devmap_umem_setup` understands: write-combining ->
`DDI_MERGING_OK_ACC` (gfx_private maps it to `HAT_MERGING_OK|HAT_PLAT_NOCACHE`, WC through PAT),
uncached -> `DDI_STRICTORDER_ACC`, write-back -> `DDI_STORECACHING_OK_ACC`. The window is loaded
with the same mode. A devmap handle carries one attribute, so a later insertion with a different
`pgprot` (`vmf_insert_pfn_prot`) uses the mapping's attribute and warns once. i915 sets one mode
per mmap type, so this does not arise in practice.

Aliasing: a WC user mapping of a system-memory page aliases the kernel's WB linear mapping, as on
Linux for i915 WC mmaps; i915's clflush discipline is what makes that safe there and here.

Cost: kernel VA equal to the mapped size per mapping (amd64 heap arena, ample) and 1 bit per page
of bookkeeping; window pages are locked translations, not memory.

### 3.5 Faults and insertion

`devmap_access(dhp, vma, off, len, type, rw)` -> `lkpi_vma_fault()`:

1. Under `lv->lock`: if every page of the range has a loaded window page, call `devmap_load()`
   while still holding the lock and return.
2. Otherwise drop the lock and call `vm_ops->fault(vmf)` for the first missing page
   (`vmf->pgoff`, `address`, `FAULT_FLAG_WRITE` from `rw`). The handler's insertions take
   `lv->lock` briefly and load window pages; a page that is already loaded is left alone (Linux
   `insert_pfn` semantics). A returned `vmf->page` is inserted and put. Continue with the next
   missing page, then go back to 1.
3. `VM_FAULT_SIGBUS/SIGSEGV` -> EFAULT, `VM_FAULT_OOM` -> ENOMEM, `VM_FAULT_RETRY` -> retry. A
   mapping without a fault handler (everything inserted at mmap time) gets EFAULT for a missing
   page.
4. Livelock guard: after 16 rounds in which a zap keeps removing pages, an ordinary fault
   returns success without loading, so the access refaults (exactly what a Linux fault that
   installed no PTE does); `F_SOFTLOCK` needs the translations and fails with EAGAIN.

Insertion API (`lkpi_mm.h`): `vmf_insert_pfn`, `vmf_insert_pfn_prot`, `remap_pfn_range`,
`io_remap_pfn_range`, `vm_insert_page`, `vmf_insert_page`, `zap_vma_ptes`,
`unmap_mapping_range`, plus `lkpi_vma_insert_pfns()` for batched insertion (i915 replacement of
`remap_io_sg`/`remap_io_mapping`, R7).

`unmap_mapping_range(mapping, start, len, even_cows)` walks `mapping->i_mmap` (an augmented
interval tree keyed by page offset, `src/itree.c`) under `i_mmap_rwsem` as reader and zaps the
overlap of every VMA under its `lv->lock`. `len == 0` means "to the end", as DRM's device unplug
path uses. The user unload is unconditional, not limited to pages the VMA's window has loaded,
because a VMA created by a partial munmap starts with an empty window while the user still has
translations from before the split (tested: `partial_munmap`).

### 3.6 Lifecycle and locking

Lock order (acquire left to right; never the reverse):

```
driver locks (object, GGTT mutex, ...)  ->  mapping->i_mmap_rwsem  ->  lv->lock  ->  segdev/hat internals
```

- `lv->lock` is never held across a call into the driver (`f_op->mmap`, `vm_ops->fault/open/
  close`) nor across `fput()`. Holding it across the fault handler would deadlock: two threads
  faulting two VMAs, each handler evicting the other's object, would each wait for the other's
  `lv->lock` inside `unmap_mapping_range`.
- `devmap_load()` is called with `lv->lock` held. It takes the handle's `dh_lock` only for
  `DEVMAP_ALLOW_REMAP` handles, which drmglue does not create, and requires the address-space
  lock, which the faulting thread holds (`segdev_fault` drops `dh_lock` before calling
  `devmap_access`).
- `devmap_unload()` takes no segdev locks (it is `hat_unload` of the user range), so it can be
  called from any thread with `i_mmap_rwsem` and `lv->lock` held, including for VMAs of the
  current process.

Races and how they resolve:

| Race | Resolution |
|------|------------|
| fault vs zap of the same pages | The handler computes and inserts under driver locks; the zapper changes the backing and zaps under the same locks (Linux drivers already guarantee this ordering). The remaining window, between the handler returning and the copy to the user, is closed by doing the "all present?" check and `devmap_load()` atomically under `lv->lock`, which the zapper also holds while unloading. A stale page therefore never reaches the user; the test `fault_zap_race` checks, after every zap and before the driver lock is released, that no user translation exists, and fails if the copy is moved outside the lock (verified by mutation). |
| unmap (munmap/exit) vs zap | Destruction first removes the VMA from `i_mmap` under `i_mmap_rwsem` as writer (draining running zappers), then marks it DEAD under `lv->lock`, unloads the window, calls `vm_ops->close` and finally `fput(vm_file)` with no glue locks held. |
| fault vs unmap of the same handle | Excluded by segdev: faults run under the address-space lock as reader, `as_unmap` holds it as writer. Hence no VMA reference count. |
| fork | `devmap_dup` -> `lkpi_vma_dup`: a new VMA (own window, empty; segdev does not copy translations), `vm_ops->open`, registered; the child refaults. Linux copies PFN-map PTEs instead; the difference is a refault. |
| partial munmap | `devmap_unmap` gives the handles of the surviving pieces; each gets a new VMA (`vm_ops->open`, adjusted `vm_start`/`vm_pgoff`), then the original is destroyed (`vm_ops->close`), the order Linux uses for a split. Surviving user translations stay valid (same physical pages, object kept alive by the pieces' references) and are still found by later zaps. |
| process exit | `closeall()` drops descriptors, then `relvm()` unmaps every segment: each VMA is destroyed as above; the last one's `fput` (or specfs' deferred `close(9E)`) runs `f_op->release` in the exiting thread. |
| zap from inside a fault handler | Allowed (i915 evicts while faulting); the faulting thread holds only driver locks when the handler runs. |
| release while mapped | The VMA's `vm_file` reference keeps the file; for clone opens specfs additionally defers `close(9E)`. Tested (`mmap_fault_zap`, `dma_buf_basics`). |

### 3.7 illumos behaviours to verify (V items)

Read from illumos-gate f71cb08e8c53 sources; must be confirmed on a running system:

- V1 `segdev_fault` calls `devmap_access` without `dh_lock`; `devmap_unload` takes no segdev lock
  and may be called from another process.
- V2 `devmap_unload` of a range currently soft-locked (`F_SOFTLOCK`, e.g. `read(2)` from disk
  into a GEM mapping) does not panic; if it does, zaps must be deferred while soft-locked, which
  requires tracking that segdev does not report to the driver.
- V3 `segdev_faultpage` with a KMEM_NON_PAGEABLE cookie uses `hat_getpfnum(kas, ...)` and maps
  device (non-`page_t`) pfns and system-memory pfns alike, with the handle's attribute.
- V4 `HAT_MERGING_OK|HAT_PLAT_NOCACHE` produces a WC PTE through PAT on Kaby Lake-R, for both the
  kernel window and the user mapping; check with `mdb ::vatopfn`/`::pte`.
- V5 `devmap_setup` called from `cb_segmap` with a single handle covering the whole mapping, and
  `devmap_map` running in `segdev_create` in the same thread.
- V6 Anonymous-file mmap: `devmap_setup` on a dev_t that was never opened, whose snode exists only
  through `makespecvp()`; `spec_addmap`/`spec_delmap` accounting on it, and `spec_delmap` calling
  our `close(9E)` for the host minor.
- V7 `falloc(vp, flag, &fp, NULL)` + `setf` + `f_setfd_or` as the two-phase install;
  `ufalloc`/`setf(fd, NULL)` as reserve/cancel.
- V8 `taskq_dispatch_ent` from interrupt context below LOCK_LEVEL.
- V9 the devlink.tab line of section 2.2.

## 4. dma-buf, PRIME, sync_file, syncobj

### 4.1 Anonymous files as pseudo vnodes

Linux exports dma-bufs, sync_files and syncobjs as anonymous files. drmglue gives every Linux
anonymous file (`anon_inode_getfile`) an illumos vnode of its own type when it is first installed
in a descriptor:

```
fd --> file_t --(VN_HOLD)--> vnode (drmglue_anon) --(one Linux ref)--> struct lkpi_file
                                                                         ^
                         kernel users: dma_buf_get, VMAs, prime caches --+ (own refs)
```

| vnodeop | Implementation |
|---------|----------------|
| open | 0 (never reached by lookup) |
| close | 0; release happens on inactive |
| read | `f_op->read` per iovec (same helper as cb_read) |
| write | EINVAL (no Linux user needs it) |
| ioctl | `unlocked_ioctl`/`compat_ioctl` through the section 2.4 adapter (no kcmd; all these uAPI structs are under 256 bytes) |
| getattr | VREG, mode 0600, `va_size = i_size`, unique `va_nodeid`; `lseek(fd, 0, SEEK_END)` therefore returns the dma-buf size, which Mesa and compositors use |
| seek | accept non-negative offsets (lseek computes SEEK_END from getattr) |
| map | section 4.6 |
| addmap/delmap | 0 (accounting happens on the host snode) |
| poll | `f_op->poll` through the section 2.5 machinery |
| inactive | `lkpi_anon_vnode_inactive`: re-check the count under `f_lkpi_lock`, detach, drop the vnode's Linux reference |
| frlock, space, getpage, putpage, shrlock | `fs_nosys` |

The vnodes belong to a placeholder vfs (`vfs_makefsops` with no operations, `VFS_INIT`) because
VOP wrappers dereference `v_vfsp`. Because a dma-buf fd is an ordinary `file_t`, SCM_RIGHTS
passing over AF_UNIX sockets, `dup`, `fork` and `exec` need nothing special (tested in the
simulation: `scm_rights_transfer`).

Repeated installs: `drm_gem_prime_handle_to_fd` installs the cached dma-buf file again for every
export, so one Linux file can be in several descriptors. The second `fd_install` takes another
vnode hold and drops the caller's Linux reference; the vnode holds exactly one. A concurrent
`fd_install` and vnode inactive are serialised by `f_lkpi_lock`, under which inactive re-checks
`v_count` (the standard illumos inactive re-check); tested with threads (`install_close_race`).

### 4.2 Descriptor plumbing

- `get_unused_fd_flags(flags)` -> `ufalloc(0)`; the flags are remembered (small table keyed by
  process and fd) because Linux applies O_CLOEXEC at reservation and illumos to an installed fd.
- `fd_install(fd, file)` -> `falloc(vp, FREAD|FWRITE..., &fp, NULL)` (no fd allocation), drop
  `f_tlock`, `setf(fd, fp)`, then `f_setfd_or(fd, FD_CLOEXEC)` if requested.
- `put_unused_fd(fd)` -> `setf(fd, NULL)`.
- `fget(fd)` -> `getf`, accept only drmglue anonymous vnodes (`vn_matchops`), take a Linux
  reference, `releasef`. DRM/i915 only `fget` dma-buf, sync_file and syncobj descriptors; a DRM
  device fd returns NULL (a documented narrowing).

### 4.3 dma-buf core

drm-kmod's BSD `dma-buf.c` is built on FreeBSD file objects (`falloc_noinstall`, `finit`,
`fileops`) and cannot be used; Linux's is GPL-2.0-only. `drmglue/src/dma_buf.c` is a clean-room
implementation over anonymous files: `dma_buf_export` (validates ops, allocates a reservation
object through the KPI if the exporter passes none, R8), `dma_buf_fd`, `dma_buf_get` (EBADF/
EINVAL), `dma_buf_put`, `get_dma_buf`, `dma_buf_[dynamic_]attach`, `detach`, `pin`/`unpin`,
`map_attachment`/`unmap_attachment` with `cache_sgt_mapping`, `begin/end_cpu_access`,
`dma_buf_mmap` (bounds-checked, `vma_set_file`), `vmap`/`vunmap`, and the file operations:
release, mmap (bounds check against the size), llseek, poll, `DMA_BUF_IOCTL_SYNC`,
`DMA_BUF_SET_NAME`.

Gaps against Linux 6.x (G items):
- G1 attachment list and vmap counter are protected by a dma-buf mutex, not by the reservation
  lock; `dma_resv_assert_held` is not checked.
- G2 `dma_buf_vmap` does not cache the mapping (needs `struct iosys_map` from the KPI).
- G3 dma-buf `poll()` reports ready immediately (no implicit-fence wait); affects only clients
  that poll dma-bufs for implicit sync (some Wayland compositors). Needs dma_resv fence
  iteration and callbacks (R10).
- G4 `DMA_BUF_IOCTL_EXPORT_SYNC_FILE`/`IMPORT_SYNC_FILE` return ENOTTY (same dependency).
- G5 no move_notify for dynamic importers; no debugfs/sysfs statistics.

### 4.4 sync_file

Clean-room `src/sync_file.c`: `sync_file_create`, `sync_file_get_fence`, `sync_file_get_name`,
poll (arms one fence callback on the first poll; the callback wakes the file's poll head from
fence-signal context; release removes the callback before freeing), `SYNC_IOC_FILE_INFO`
(one fence; status from `dma_fence_get_status`; timestamp 0) and `SYNC_IOC_MERGE` for the cases
that need no fence array (same fence, or one side already signaled; otherwise ENOTSUP, G6).
`SYNC_IOC_SET_DEADLINE` is ENOTTY. The GPL-2.0-only `drm/drivers/dma-buf/sync_file.c` that came
with the import must not be built (R9). sync_file needs the KPI `<linux/dma-fence.h>` (R10); the
tests use a minimal stand-in (`drmglue/tests/fakes/linux/dma-fence.h`, never for real builds).

### 4.5 syncobj

`drm_syncobj.c` (MIT, imported) needs only `anon_inode_getfile`, `get_unused_fd_flags`,
`fd_install`, `fget`/`fput` and the f_op comparison, plus `sync_file_create`/
`sync_file_get_fence` for the sync_file import/export ioctls; all are provided.
`DRM_IOCTL_SYNCOBJ_EVENTFD` needs `eventfd_ctx_fdget` on illumos eventfd descriptors (a
character device, not an anonymous file); not provided (G7). anv uses it only when available.

### 4.6 mmap of anonymous files

segdev needs a character dev_t whose `cb_devmap` builds the mapping and an existing snode for it
(`segdev_create` calls `specfind`). The anonymous `VOP_MAP` therefore:

1. assigns the file an mmap id (idr, released with the file);
2. builds dev_t (driver major, node 0xff, clone bits = id), creates its snode once with
   `makespecvp()` and keeps it held until the file is freed (backend `file_fini`);
3. calls the same `drmglue_segmap_file()` as the device path; `devmap(9E)` on node 0xff looks the
   file up by id, and everything else (VMA, window, faults, zaps) is shared with section 3.

The DRM driver's major is used as host because some DRM device must be attached for dma-bufs to
exist. For i915 dma-bufs, `i915_gem_dmabuf_mmap` redirects the VMA to the object's shmem file with
`vma_set_file`; the VMA registers in that file's mapping after `f_op->mmap` returns, so zaps by
the shmem emulation find it.

## 5. Requested changes for other owners

KPI (include/, src/):

- R1 Move the Linux file-layer types from `drmglue/include/drmglue/lkpi_fs.h` and `anonfile.h`
  into `include/linux/fs.h`, `poll.h`, `kdev_t.h`, `file.h`, `anon_inodes.h` (keeping the
  `f_lkpi_*` members or one opaque pointer for them); drmglue then includes those.
- R2 Move `#define file lkpi_file` into `<linuxkpi/platform.h>` next to the mutex rename.
- R3 Move the Linux-named parts of `lkpi_mm.h` (`vm_area_struct`, `vm_fault`, `vm_operations_struct`,
  `pgprot_t`, VM_*, insertion functions) into `linux/mm.h`, `mm_types.h`, `pgtable.h`; the
  implementation stays in drmglue (it needs the backend). `pgprot_t` must carry the cache mode
  as defined there.
- R4 (done by the KPI meanwhile) custom-function wait-queue entries; drmglue uses them. Keep
  `struct wait_queue_head` as the tag name.
- R5 Provide `unsigned long lkpi_page_to_pfn(struct page *)` and `void lkpi_put_page(struct page
  *)` with the P2 `struct page` (used for `vm_insert_page` and `vmf->page` faults).
- R6 `in_compat_syscall()`/`is_compat_task()` = current process data model is ILP32
  (`get_udatamodel() == DATAMODEL_ILP32`); `compat_ptr()`.
- R8 Provide `lkpi_dma_resv_alloc()`/`lkpi_dma_resv_free()` (dma_resv init/fini in an allocated
  object) for exporters that pass no reservation object.
- R10 Provide `<linux/dma-fence.h>` (and dma-fence-unwrap/array for full `SYNC_IOC_MERGE`), with
  the documented semantics drmglue relies on: callbacks run once under the fence lock;
  `dma_fence_add_callback` returns -ENOENT when signaled; `dma_fence_remove_callback` excludes a
  concurrent callback. Then G3/G4/G6 can be closed in drmglue.
- `fput()` is synchronous here (section 2.6); if a deadlock appears, the KPI workqueue can defer
  the final release.

drm/ and i915/ imports:

- R7 i915: replace the bodies of `remap_io_sg`/`remap_io_mapping` (`i915_mm.c`, built on
  `apply_to_page_range`) with loops over `lkpi_vma_insert_pfns()`; take the `#ifdef __linux__`
  branches of `i915_gem_mman.c` (anon_inode mapping for `drm_vma_node_unmap`, the mmap singleton
  `f_mapping`), not the FreeBSD ones, and the `unmap_mapping_range` branch in `drm_dev_unplug`.
- R9 Do not build drm-kmod's `drivers/dma-buf/dma-buf.c` (FreeBSD file objects) or
  `drivers/dma-buf/sync_file.c` (GPL-2.0-only); `drmglue/src/dma_buf.c` and `sync_file.c` provide
  the API, with `drmglue/include/linux/dma-buf.h` and `sync_file.h` ahead of the imported
  headers in the include path.
- R11 DRM adapter: call `drmglue_attach_node()` for each registered DRM minor (from attach after
  `drm_dev_register`, or from the KPI's `device_add` for DRM minors), with `drm_stub_fops` or the
  driver fops and a `kcmd` callback returning `drm_ioctls[nr].cmd` /
  `driver->ioctls[nr - DRM_COMMAND_BASE].cmd`; point the driver's `devo_cb_ops` at
  `drmglue_cb_ops` and `devo_getinfo` at `drmglue_getinfo`; call `drmglue_init`/`fini` from
  `_init`/`_fini`; `_depends_on` must include `misc/gfx_private`.

Userland (oi-userland):

- U1 libdrm: node type from `minor(st_rdev) & 0xff`; check the existing illumos patches.
- U2 32-bit consumers of fake offsets must be largefile builds (`mmap64`).

## 6. Verification status

| What | How | Result |
|------|-----|--------|
| ioctl translation, errno and poll maps | `tests/test_xlate.c` | pass (gcc, ASan/UBSan, clang) |
| interval tree vs brute force; offset manager | `tests/test_vmoff.c` | pass |
| mmap, fault, zap, partial munmap, fork, release deferral, error paths, fault-vs-zap race (5 threads, 3000 zaps) | `tests/test_vma.c` on the simulated segdev | pass; race test fails as expected when `devmap_load` is moved outside `lv->lock` |
| anonymous files: release timing, dup, SCM_RIGHTS, repeated install, O_CLOEXEC, install/close race; dma-buf; sync_file | `tests/test_anonfile.c` on the simulated fd/vnode layer | pass |
| clone opens, failed open, ioctl dispatch (illumos encoding, large structs, ilp32, FKIOCTL, errno), read, poll forwarding | `tests/test_cdev.c` | pass |
| illumos backends and core with kernel flags | `make -C drmglue check-kmod` against `drmglue/stubs` | compiles; 92 undefined symbols all in the allowlist |

What the simulation proves: the core's state machines, reference counting and lock ordering
are consistent under the stated semantics of the illumos interfaces (section 3.7). What it does
not prove: that illumos actually has those semantics (V1-V9), that the stubs match illumos
signatures, that the kmod links or loads, that WC/PAT attributes come out right, or anything
about real DRM/i915 code, which has not been compiled against drmglue yet. Untested on illumos:
all of `src/*_illumos.c`, the devlink entry, 32-bit compat ioctls with real structures, SCM_RIGHTS
over real sockets, process-exit ordering.

First tests to run on illumos, once the DRM adapter (R11) exists: open/close/ioctl
`DRM_IOCTL_VERSION` from 64- and 32-bit processes; dumb-buffer create + mmap + write + read
back; `kmscube`; igt `gem_mmap`, `gem_mmap_gtt` (including the eviction tests, which exercise
zaps), `gem_mmap_offset`, `prime_self_import`, `prime_mmap`, `syncobj_basic`; pass a dma-buf
over a socket between two processes and mmap it in the receiver; run with `kmem_flags=0xf` and
`::findleaks` after process kills during GPU work.

## 7. Risks and open items

- K1 V2 (zap of soft-locked ranges) is the most likely illumos-side surprise.
- K2 Kernel VA per mapping: large numbers of big mappings (e.g. 4 GiB of mmapped buffers) use
  equal kernel heap VA; fine on amd64, worth a kstat.
- K3 Synchronous `fput` release (section 2.6).
- K4 Mapping attribute fixed per handle (section 3.4).
- K5 Clone-id range: >1023 opens of one node break 32-bit `fstat` on the extra fds (section 2.1).
- K6 `drmglue_detach_node` keeps the first dip as anonymous mmap host until module unload;
  multi-GPU hot-unplug of that GPU while other GPUs' dma-bufs are mapped is not handled.
- G1-G7 functional gaps listed in sections 4.3-4.5.
