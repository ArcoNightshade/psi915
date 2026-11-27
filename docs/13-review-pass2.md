# Independent review, second pass

Status 2026-11-27 (merged up to 646f1da). Reviewer: second independent agent. Read-only on include/, src/, drmglue/,
drm_bsd/, build/, drivers/; this file and `review/` are the only things written.
Method: the psi915 sources at the merge of `origin/psl/epic-gates-dx5xvk` (after the first pass,
docs/12) were read against the illumos-gate sources in `ref/illumos/usr/src/uts` (f71cb08e8c53)
and the imported Linux v6.14 code in drm/, i915/. Nothing was compiled against real illumos headers
or run on illumos. Portable bugs have a userland reproducer in `review/` (`make -C review`; each
program exits 1 while the bug is present). The findings of docs/12 are not repeated; where one of
them matters for an area below it is referenced by its ID (M1, L3, A1, ...).

Severity and confidence are used as in docs/12: **blocker** (panic, memory corruption or hang in
the normal i915 path), **high** (same, in a plausible but less common path), **medium**, **low**.

## Summary (top first)

| ID | Sev | Area | Title | Confidence |
|----|-----|------|-------|------------|
| W1 | blocker | PCI | `pci_get_*()` runs `ddi_walk_devs(ddi_root_node())` from attach(9E): lock-order deadlock with any devinfo snapshot (devfsadm, prtconf, fmd) | medium-high |
| G1 | high | console | after the initial framebuffer is released, gfx_private writes the console shadow into whatever GGTT now maps the old fb range (every resume, every return to KD_TEXT) | medium-high |
| P1 | medium | PCI/DDI | the attach-time `pci_dev` holds `ndi_hold_devi()` on its own dip: `detach_node()` refuses before detach(9E) is called; modunload/rem_drv can never detach i915 | high |
| P2 | medium | IRQ | no INTx fallback: without MSI `pdev->irq` is 0, `request_irq(0)` fails, i915 probe fails | high (reproduced) |
| G2 | low | console | `text-0` mmap goes to `drmglue_segmap` (ENXIO); routed like vgatext it would expose the aperture window (G1) | high |
| B1 | low-medium | build | `ctfconvert/ctfmerge -L VERSION` with VERSION unset in the environment | low (tool behaviour not verified) |
| D2 | low | DMA | rootnex silently bounces streaming maps through a copy buffer when a page is above the DMA mask; nothing detects it | high (fact), low (i915 sets 39-bit mask first) |
| P3 | low | PCI | `lkpi_pci_iomap_common()` dereferences an unchecked `kzalloc()` | high |
| C3 | low | I2C | bit-banging: `I2C_M_RECV_LEN` error path leaves the slave mid-byte (no NAK); `I2C_M_NO_RD_ACK` ignored | high (fact), low (impact) |
| F1 | low | locking | interruptible-mutex wake-up can be missed (store-buffer reordering after `mutex_exit`); 20 ms fallback | high |
| F2 | low-medium | memory | natural-alignment fix doubles the size of every power-of-two kmalloc | high |
| A2 | low | attach | `psi915_detach()` removes the DRM minors before a `lkpi_pci_detach()` that may fail | high (fact), low (unreachable today, see P1) |

The "first 10 places probe would fail or hang on real hardware" list is in section "Probe path".

## Area 1: PCI layer (pci_dev wrapper, config space, BARs, enumeration)

### W1 (blocker) Device-tree walk from attach(9E) inverts the devinfo lock order

Files: `src/linux_pci.c:783-801` (`lkpi_pci_match_cb`, `lkpi_pci_search`),
`src/plat_illumos_dev.c:205-232` (`lkpi_walk_cb`, `lkpi_plat_pci_walk`).

Every `pci_get_device/class/subsys/domain_bus_and_slot()` and `pci_dev_present()` calls
`ddi_walk_devs(ddi_root_node(), ...)`. `walk_devs()` (uts/common/os/devcfg.c:3578) does
`ndi_devi_enter()` on each node before walking its children, top-down from the root. i915 calls
these from its probe, i.e. from attach(9E), at least twice on KBL-R:
`intel_gmch_bridge_setup()` (soc/intel_gmch.c:26, host bridge 0:0.0) and `intel_detect_pch()`
(soc/intel_pch.c:250, ISA bridge loop). During attach the framework holds the parent nexus
(the root complex node `pci@0,0`) busy for the whole probe.

Deadlock: thread B takes a libdevinfo snapshot (devinfo.c `PUSH_STACK` holds every ancestor with
`ndi_devi_enter()` from the root down: root held, then blocks on `pci@0,0`, owned by the attaching
thread). The attaching thread's walk then blocks in `ndi_devi_enter(root)`. Both hang forever; the
boot stalls in i915 attach (or `prtconf`/`devfsadm`/`fmd` hang if attach happens later, e.g.
`modload`, `update_drv`). devfsadmd and syseventd take snapshots at boot, concurrently with driver
attach, so the window (the whole i915 probe, seconds) is realistic. The same inversion exists with
any other top-down walker. devcfg.c:3646-3662 states the rule explicitly: ddi_walk_devs "is not
callable from device autoconfiguration routines ... attach(9e)", and enters done by dependent code
must be at-or-below the node already entered.

Fix: from attach, never enter above the attaching dip's parent. The devices i915 looks up on
KBL-R (0:0.0 host bridge, 0:1f.0 LPC/ISA bridge) are siblings of 0:2.0 under the same root-complex
node, and the attaching thread already holds that parent busy, so iterating
`ddi_get_child(ddi_get_parent(attach_dip))` / `ddi_get_next_sibling()` is safe and legal without any
further `ndi_devi_enter()`. Implement `lkpi_plat_pci_walk()` as a sibling scan when called from
the attach thread (record the attaching dip/thread in `lkpi_pci_attach()`), and keep the full walk
only for callers outside autoconfiguration that are not waited for by an attach. Alternatively take
a PCI inventory once in `_init()`... but `_init` is in the same forbidden list, so the sibling scan is
the cleaner choice. Confidence: medium-high (lock order read in devcfg.c and devinfo.c; timing
dependent).

Related, verified OK: `pci_config_setup()` on a driverless sibling (host bridge) is what
npe itself does (uts/i86pc/io/pciex/npe_misc.c:290), so the wrapper creation for 0:0.0 is fine once
the walk is fixed.

### P1 (medium) The attach-time wrapper holds its own dip; detach(9E) is unreachable

Files: `src/linux_pci.c:95` (`lkpi_plat_dev_hold(dip)` in `lkpi_pci_dev_create`), `:60` (release),
`src/linux_pci_ddi.c:59-71, 93-98`.

`lkpi_pci_attach(DDI_ATTACH)` creates the `pci_dev` for its own dip, and the wrapper takes
`ndi_hold_devi()` (devi_ref++) that is dropped only when the last `pci_dev` reference goes, which
happens in `lkpi_pci_detach(DDI_DETACH)`. But `detach_node()` (devcfg.c:1372-1382) returns
DDI_FAILURE while `devi_ref != 0`, *before* calling the driver's detach(9E). So for i915 (and
lkpi_toy) `modunload -i`, `rem_drv`, `update_drv -f` and DR never detach; `_fini`'s `mod_remove()`
fails with EBUSY forever and the whole DDI_DETACH path (psi915_detach, i915_pci_remove, devres
unwind) is dead code that cannot be tested. DDI_SUSPEND is not affected (cpr calls
`devi_detach(DDI_SUSPEND)` directly). Auto-detach is already prevented by `ddi-no-autodetach=1`
in i915.conf, so the hold buys nothing for the attached dip.

Fix: do not `ndi_hold_devi()` the dip the driver itself is attached to (its lifetime is bounded by
attach/detach, and detach already refuses while DRM files are open); keep the hold only for
foreign dips found by enumeration. Confidence: high.

### P3 (low) Unchecked allocation in `lkpi_pci_iomap_common()`

File: `src/linux_pci.c:561-566`. `m = kzalloc(...); m->addr = addr;` without a NULL check, and the
BAR mapping is leaked on that path. GFP_KERNEL never fails today (docs/12 M2), so latent. Fix:
check and `lkpi_plat_unmap_regs()` on failure.

### Area 1, verified OK

- `lkpi_plat_pci_info()`: rnumber = index of the 5-cell entry in "reg" (entry 0 = config space), BAR
  index from the register offset (0x10->0 ... 0x24->5), 64-bit BARs at 0x10/0x18 become BAR 0/2 as in
  Linux (`GTTMMADR`=0, `GMADR`=2). BAR start from "assigned-addresses" phys_mid/phys_low.
  `IORESOURCE_UNSET` when unassigned matches what `i915_pci_resource_valid()` tests.
- Config accessors check alignment and `cfg_size`; capability walks are bounded (48 / 480 entries);
  the KBL iGPU has a PCIe (RCiEP) capability, so 4 KiB config space is assumed, as on Linux.
- `pci_find_next_capability()` starts after `pos` (reads `pos+1`), as Linux.
- `pci_get_class()` drops the `from` reference, matching Linux iteration (`intel_detect_pch`).
- `pci_enable_device()` enables MEM/IO decoding by BAR type; disabling only clears bus mastering
  (documented; keeps the boot console decoding).
- `request_mem_region()` returning `(struct resource *)1`: the only i915 consumer
  (`i915_gem_stolen.c:163-190`) tests it for NULL and never dereferences it.
- Gen9 stolen quirk (`lkpi_intel_gfx_quirk`): GGC 0x50 GMS decoding (32 MiB units below 0xf0,
  4 MiB from 4 MiB above) and BDSM 0x5c mask 0xfff00000 match Linux `gen9_stolen_size()` /
  `intel_stolen_base()`.
- S3: the PCIe nexus saves/restores child config space when the child did not
  (pcie_pwr.c:1022-1053), so the no-op `pci_save_state/pci_restore_state` is acceptable (not
  verified on hardware; npe uses pcie_pwr only for some topologies).

## Area 2: interrupts (MSI via ddi_intr, handler context)

### P2 (medium) No legacy interrupt when MSI is unavailable

Files: `src/linux_irq.c:313-356, 393-399`; i915 `i915_driver.c:560`, `i915_irq.c:1209-1223`.

Linux's `pdev->irq` always holds the INTx line; `pci_enable_msi()` failing is only logged by i915,
which then calls `request_irq(pdev->irq, ...)`. Here `pdev->irq` is set only by
`pci_alloc_irq_vectors()`, so after a failed `pci_enable_msi()` it is 0 and `request_irq(0)`
returns -EINVAL: `intel_irq_install()` fails and the whole probe with it (console fallback, A1).
Scenarios: MSI disabled on the system (`set pcplusmp:apic_support_msi`/apix tunables, some
hypervisors used for testing), or MSI vector allocation failure.

Reproducer: `review/repro_irq_no_intx.c` (fake INTx-only device; prints
`pci_enable_msi = -28, pdev->irq = 0, request_irq = -22`).

Fix: in `lkpi_pci_dev_create()` (or lazily on `request_irq(pdev->irq)`), when the device has an
interrupt pin, give `pdev->irq` a KPI irq number bound to a FIXED-type allocation
(`ddi_intr_alloc(DDI_INTR_TYPE_FIXED)` deferred to `request_irq`), and release it when
`pci_alloc_irq_vectors()` replaces it with MSI. Confidence: high.

### Area 2, verified OK

- `ddi_intr_block_enable(&h[idx], 1)` for a single MSI vector: valid (ddi_intr.c:819-864 only
  requires ADDED state, MSI type and the BLOCK cap; pci_common.c:600-631 enables the handles given).
  For multi-vector MSI enabling one handle at a time with count 1 would be wrong, but i915 uses
  one vector.
- Handler order: `ddi_intr_add_handler()` before enable, disable before
  `ddi_intr_remove_handler()`; `fn[idx]` is published before the handler is added.
- MSI priority is below LOCK_LEVEL (refused otherwise), so the i915 handler's spinlocks (adaptive
  kmutex), `queue_work()` (tasklets, threaded IRQs) and `wake_up()` are legal in the interrupt
  thread; worker threads are spawned by the manager thread, never from `queue_work()`.
- `free_irq()` disables then removes the handler (which waits for a running handler) before
  `synchronize_irq()`; I1 of docs/12 remains the only gap.

## Area 3: console handoff, DDI glue (drivers/i915_glue, linuxkpi/ddi.h)

### G1 (high) Console writes into GGTT space that belongs to other objects

Files: `drivers/i915_glue/i915_drm_glue.c:265-271` (`psi915_drm_console_setmode` no-op),
`drivers/i915_glue/i915_ddi.c:223-231, 341-351`; gfx_private
`uts/i86pc/io/gfx_private/gfxp_bitmap.c:162-166, 181-185, 187-206, 227-240`.

gfx_private maps the firmware framebuffer once, by physical address inside the GMADR aperture
(`bitmap_setup_fb`: `gfxp_map_kernel_space(fb_info.paddr, ..., WC)`). What the CPU sees there is
whatever the GGTT PTEs at that aperture offset point to. i915 keeps the BIOS fb object (stolen) at
its GGTT offset only as long as the initial plane state references it
(`intel_initial_plane_config`). The first DRM master (Xorg/modesetting) replaces the plane's fb;
the initial fb is freed, its GGTT node is released and later reused for rings, contexts, HWSP or
user BOs. From then on:

- every DDI_RESUME: `psi915_attach(DDI_RESUME)` calls `gfxp_fb_attach(DDI_RESUME)` ->
  `bitmap_resume()` -> `bitmap_kdsettext()`, which copies the whole console shadow (e.g. 8 MiB at
  1080p) into the aperture *regardless of the current KD mode*, i.e. also while X is running;
- every KD_TEXT (X exits or crashes, VT switch to the console): `bitmap_kdsetmode(KD_TEXT)` calls
  the no-op setmode and then copies the shadow over the aperture as well; subsequent console
  output keeps writing there.

Result: silent corruption of whatever GPU objects are bound at that GGTT range: user buffers,
or kernel-owned ring buffers / context images / status pages (GPU hang, wrong rendering, possibly
GPU writes to wrong memory through corrupted context state). Suspend/resume with X running on a
laptop is the common path. docs/11 and the setmode comment treat this as "console invisible"; it is
worse than that. A1 (docs/12) only covers the probe-failure fallback.

Fix (until R-P1..R-P3 land): keep the console's GGTT range permanently owned by the console. In the
i915 patch series, take an extra pin on the initial plane vma at probe (or reserve a `drm_mm` node
over the firmware fb range in the GGTT with the stolen backing kept), released only when the
console is handed over properly. Minimum stop-gap: make `psi915_blt_setmode(KD_TEXT)` fail once
the initial fb is gone (gfx_private then keeps writing only to the shadow) and skip
`gfxp_fb_attach(DDI_RESUME)` in that state. Confidence: medium-high (gfx_private paths read;
whether i915 frees the initial fb on the first master modeset follows from 6.14
`intel_plane_initial.c`, not traced line by line).

### G2 (low) Console minor mmap

File: `drivers/i915_glue/i915_ddi.c:461-467`. `psi915_cb_ops` overrides open/close/ioctl/devmap but
keeps drmglue's `cb_segmap`, `cb_read` and `cb_chpoll`. mmap(2) goes through `cb_segmap`, so on the
console minor it reaches `drmglue_segmap()` -> `lkpi_cdev_file(0xfe, 0)` = NULL -> ENXIO;
`psi915_devmap()`'s console branch is never called (vgatext uses `ddi_devmap_segmap` as
`cb_segmap`). Today that only breaks programs that mmap `/dev/fb`. If it is fixed by routing the
console minor to `ddi_devmap_segmap`/`gfxp_fb_devmap`, the mapping is the physical aperture window
and, once the initial fb is released (G1), lets the console owner read and write arbitrary GPU
objects; fix G1 first or refuse the console mmap while DRM owns the display.

### A2 (low) DDI_DETACH removes minors before a detach step that can fail

File: `drivers/i915_glue/i915_ddi.c:437-443`. `psi915_nodes_detach()` runs before
`lkpi_pci_detach()`; if the latter ever fails the driver returns DDI_FAILURE with card0/renderD128
gone. Today unreachable (P1) and `lkpi_pci_detach` cannot fail; order it after a successful
Linux remove, or re-create the nodes on failure.

### Area 3, verified OK

- First-minor rule: `ddi_append_minor_node()` appends (sunddi.c:5343), and `resolve_pathname()`
  without a minor name picks a DDM_DEFAULT minor, else the first DDM_MINOR (devcfg.c:4105-4136), so
  creating `text-0` before card0/renderD128 is what makes consconfig find the console minor.
  `ddi_pathname_to_dev_t()` also sets `ddi-no-autodetach` on the node (devcfg.c:4157-4174).
- `_init` runs the Linux init before `mod_install()` and undoes it on failure; `_fini` runs the
  exit only after `mod_remove()` succeeded; `dev_ops` are patched before `mod_install()`.
- DDI_SUSPEND unwinds the Linux suspend when gfx_private refuses; DDI_RESUME resumes Linux before
  the console (but see G1).
- getinfo: console minor and not-yet-registered DRM nodes resolve to the single softc's dip.
- Probe failure path: `lkpi_pci_probe()` releases devres (devm_drm_dev_alloc's action drops the
  drm_device, which drops its `get_device(parent)`), then the attach drops the wrapper reference.
- `psi915_drm_kcmd`: core numbers 0x00-0x3f/0xa0-0xff from the table, driver numbers bounded by
  `num_ioctls`.

## Area 4: drmglue file layer (file.c, anonfile.c, anonfs_illumos.c, pseudofs.c, poll)

No new defects found. This closes the item docs/12 left open:

- Vnode references: `falloc(vp, flag, &fp, NULL)` (fio.c:1370-1400) stores `vp` in the `file_t`
  *without* `VN_HOLD`; `closef()` later does the `VN_RELE`. `fd_install()` gives each new `file_t`
  exactly one hold: the `vn_alloc()` count of a new vnode, or a fresh `VN_HOLD` for an existing one
  (and then drops the caller's Linux reference, which can never be the last because the vnode holds
  one). dup/fork/SCM_RIGHTS share the `file_t` (f_count) and never touch the vnode, so passing a
  dma-buf fd over a socket and closing it on both ends releases the vnode once.
- `vn_rele()` calls VOP_INACTIVE with `v_count == 1`; `drmglue_vn_rele_last()` re-checks under
  `v_lock`, and `fd_install()`'s `VN_HOLD` vs. inactive is serialised by `f_lkpi_lock` (re-check
  sees `v_count > 1` and just decrements). After inactive clears `f_lkpi_vnode`, a later
  `fd_install()` builds a new vnode. Correct.
- `fget()`: `getf()` pins the `file_t`, hence the vnode, hence the Linux file's vnode reference,
  so `get_file()` is safe; `releasef()` afterwards.
- `alloc_file_pseudo()`/`dput()` follow Linux: the caller's inode reference moves to the dentry,
  the last `dput()` runs `d_op->d_release` (upstream `dma_buf_release`) then `iput()`.
- Poll: persistent wait entries are removed (under the queue lock, via `remove_wait_queue`) before
  `f_op->release` and before `ph_free`; `drmglue_ph_task` may re-dispatch its preallocated entry
  while running, which illumos allows (taskq.c:1882-1897: ownership returns to the caller when the
  function is called; the thread does not touch the entry afterwards); the dying/queued handshake
  frees the head exactly once.
- `fput()` from interrupt context defers the final release to a workqueue.

## Area 5: xarray, idr/ida, devres, kobject/device/class

No defects found that matter for i915/DRM.

- xarray: a randomized differential test (200k operations per seed, 6 seeds, store/erase/insert/
  alloc/reserve/release/marks, dense and sparse indices, checked against a plain array model,
  ASan/UBSan clean) found no mismatch. Not added to `review/` because it found nothing; the program
  is easy to recreate from this description.
- Locking: `xa_load()` takes `xa_lock` unless the caller holds it (owner check), so the i915/DRM
  patterns `xa_lock(); xa_load();` (i915_gem_context.c:357, drm_drv.c:241) and
  `xa_lock_irq(); xa_load();` (intel_guc_submission.c:4987) do not self-deadlock. Linux's lockless
  `xa_load()` becomes a mutex acquisition; no lock-order inversion was found in the imported users.
- `__xa_store(NULL)` in an ALLOC array reserves (XA_ZERO_ENTRY), as Linux; `xa_alloc` returns
  -EBUSY when full (drm_minor_alloc's fallback to the extended range relies on that).
- idr: `idr_alloc(NULL)` + `idr_replace(ptr)` (drm_gem_handle_create_tail) and
  `idr_replace(NULL)` + `idr_remove` (drm_gem_handle_delete) behave as Linux; `idr_alloc_cyclic`
  wraps correctly. `idr_preload` being a no-op only matters under memory pressure (documented).
- devres: release order newest-first, groups, `devm_add_action_or_reset`; data alignment is 8 bytes
  (header 56), which is what Linux x86-64 guarantees too (`ARCH_DMA_MINALIGN` = 8).
- `device_add()` takes one parent reference via kobject and one via `get_device()`, `device_del()`
  drops both. `kobject_get_path()` length arithmetic checked.

## Area 6: firmware, I2C/GMBUS, SRCU

### C3 (low) Bit-banging corner cases

File: `src/linux_i2c.c:421-441`. On an invalid SMBus block length (`I2C_M_RECV_LEN`) the code jumps
to the STOP after having ACKed the count byte, without the NAK cycle Linux's
`i2c-algo-bit` sends, so a slave that is already shifting out the next byte can hold SDA low and the
STOP fails (bus stuck until the next transfer's pre_xfer reset). `I2C_M_NO_RD_ACK` is ignored. i915
uses bit-banging only as GMBUS fallback for DDC (plain reads), so impact is low.

### Firmware, verified OK

`kobj_open_file/kobj_get_filesize/kobj_read_file` (kobj.c:3853, 3942, 4307) work before and after
root mount (boot archive vs vnode), handle gzip, and have no size limit below the KPI's own 64 MiB
cap. i915 loads DMC from a work item (`intel_dmc_init`), not from attach; GuC/HuC are off for Gen9.
Unverified: whether `/usr/lib/firmware` is on the root dataset on all target installs (a separate
`/usr` would make an early request fail with -ENOENT; DMC is optional, DC states just stay off).

### SRCU, verified OK

Single-flip design: reader `inc ctr[i]; mb; re-check idx`, writer `flip; mb; wait ctr[old]`. A
reader that re-checks after a later flip is post-flip for that synchronize and is waited for by
the next one; no starvation (readers retry only while flips happen). No `call_srcu` consumer in the
Gen9 set.

## Area 7: build/ flags vs the illumos kernel build

### B1 (low-medium) CTF label taken from an unset environment variable

File: `build/Makefile.master:51-52`. `-L VERSION` tells ctfconvert/ctfmerge to read the label from
the environment variable `VERSION`, which the gate's nightly sets and a plain `gmake` does not. If the
tools treat a missing variable as fatal, every object fails to build when onbld is installed; if
not, the CTF label is empty. Not verified (tools not in the reference tree). Fix: export
`VERSION ?= psi915-$(git describe)` in Makefile.master, or use `-l <label>`.

### Build, verified OK / notes

- Flags match uts/Makefile.uts + Makefile.master for amd64 gcc: `-mcmodel=kernel -mno-red-zone`
  (cw's `-xmodel=kernel`), `-ffreestanding -mno-mmx -mno-sse`, `-msave-args`
  (`-Wu,-save_args`), `-fno-shrink-wrap`, retpoline thunks, `-fno-asynchronous-unwind-tables`,
  the CCNOAUTOINLINE/CCNOREORDER/CCNOAGGRESSIVELOOPS sets; link `ld -ztype=kmod` as uts/Makefile.targ:40;
  dependencies as DT_NEEDED via `-dy -N misc/...` (gate style, `_depends_on` not used).
- Imported Linux code additionally gets `-fno-strict-aliasing -fno-strict-overflow
  -fno-delete-null-pointer-checks -fno-common` (build/drm/Makefile.lkpi:116). The KPI's own src/ is
  built without `-fno-strict-aliasing`, like the gate; acceptable for code written for it.
- `-mno-sse` plus hiding SSE4.1 in `boot_cpu_data` (src/linux_cpu.c:64-72) keeps i915 off its
  MOVNTDQA path; `kernel_fpu_begin()` panics if reached.
- Missing `-D_ASM_INLINES`: harmless. `-ffreestanding` implies `-fno-builtin` (Linux does not use
  it): constant-size `memcpy`/`memset` become calls; performance only.
- Stacks: kernel threads, lwps and interrupt threads get DEFAULTSTKSZ = 5 pages = 20 KiB
  (i86pc/sys/machparam.h:106-109, cpuvar.h:304); Linux x86-64 has 16 KiB plus separate IRQ stacks.
  Probably sufficient, but `-msave-args` frames are larger and the drmglue/KPI layers add depth;
  watch for `stack overflow` panics in deep atomic-commit/execbuf paths.

## Area 8: DMA notes

### D2 (low) Silent rootnex copy-buffer bouncing is not detected

File: `src/plat_illumos_dev.c:550-583`. For DDI_DMA_STREAMING binds, rootnex bounces every page
outside `[dma_attr_addr_lo, dma_attr_addr_hi]` through a copy buffer instead of failing
(rootnex.c:2950-2969, 3221-3228). A GPU-persistent mapping (GTT/PPGTT) of a bounced page would point
at the bounce buffer; CPU writes reach the GPU only at `ddi_dma_sync`. i915 sets a 39-bit mask
before any mapping and the KPI allocates below it, so it does not happen on KBL-R; it would for any
device that keeps the default 32-bit mask on a machine with RAM above 4 GiB. Fix: after bind, verify
that the cookie address equals the page's physical address when no IOMMU is active (or check
`hat_getpfnum` against the mask beforehand) and fail the map otherwise.

## Area 9: fixes for docs/12 that landed during this pass (ab8aa90, 9f90922, 646f1da)

Reviewed after merging; 646f1da is marked WIP by its author. With these fixes
`review/repro_dma_sg_kva` (M1) and `review/repro_gfp_huge_sleep` (M2) now pass;
`review/repro_irq_no_intx` (P2) still fails. `review/Makefile` now rebuilds the reproducers when
the KPI sources change (it previously kept stale binaries, which made a fixed bug look present).

### F1 (low) Interruptible mutex: missed wake-up, recovered only by the 20 ms poll

Files: `include/linux/mutex.h:91-98` (`mutex_unlock`), `src/linux_lock.c:186-231`.
`mutex_unlock()` does `lkpi_plat_mutex_exit()` and then loads `lkpi_waiters`. illumos' adaptive
`mutex_exit()` on amd64 is a plain store (lock_prim.s), and a seq-cst `__atomic_load_n` is a plain
`mov` on x86, so the load may be satisfied before the releasing store is visible (store-buffer
reordering). The waiter does `waiters++` (locked) then `tryenter` (locked): the classic
store-buffering pair, in which both sides can read the old values. The waiter then sleeps until its
timed fallback (`1 + hz/100` ticks, 20 ms at hz=100) instead of being woken. Not a hang, but every
contended `mutex_lock_interruptible()` (i915 uses it on hot paths such as `ggtt->vm.mutex`) can
take 20 ms extra. Fix: `__atomic_thread_fence(__ATOMIC_SEQ_CST)` (i.e. `smp_mb()`) between the
exit and the waiter-count load. Confidence: high (memory-model argument), impact low.

### F2 (low-medium, performance) Power-of-two kmalloc now costs twice its size

File: `src/linux_slab.c:36-53`. To give natural alignment (docs/12 M4), a power-of-two request of
size S > 16 allocates S + S bytes (`lkpi_kmalloc_total`), e.g. `kmalloc(4096)` -> `kmem_alloc(8192)`,
`kmalloc(64)` -> 128. `sg_alloc_table()` chunks, page arrays and many i915/DRM objects are powers of
two, so kernel memory use for them doubles. illumos' `kmem_alloc` caches already align power-of-two
cache sizes (64-byte alignment for multiples of 64, page alignment for multiples of PAGESIZE, in
kmem.c's `kmem_alloc_caches_create`); what costs the slack is the in-band header. Fix: keep the
size out of band for these sizes (or rely on M4's "no consumer found" and drop the guarantee) instead
of over-allocating.

### F3 (note) Size-aware GFP threshold

`lkpi_gfp_to_plat_size()` makes every `GFP_KERNEL` request above 1 MiB `KM_NOSLEEP|KM_NORMALPRI`.
That fixes M2, but legitimate large kernel allocations (page-pointer arrays of big GEM objects,
2 MiB for a 1 GiB object) can now fail transiently when memory is merely fragmented or caches have
not been reaped, where Linux would have reclaimed. Acceptable; consider one retry with
`KM_SLEEP` for requests below a fraction of free memory, or `__GFP_NOFAIL`-like treatment for
kernel-internal callers.

### Verified OK

- M1 fix (`src/linux_dma.c:277-372`, `src/plat_illumos_dev.c:594-660`): entries whose pages are not
  VA-contiguous are bound as a `B_SHADOW` buf with a `page_t` list. `ddi_dma_buf_bind_handle`
  (sunddi.c:7036-7066) turns that into `DMA_OTYP_BUFVADDR` with `v_priv = b_shadow`, `v_as = kas`
  for `b_proc == NULL`, and rootnex's `rootnex_get_sgl` takes each page's PFN from `pplist[pcnt]`
  (rootnex.c:2743-2790, 2891-2992), using `b_addr` only for the first page's offset. Correct.
  Caveats, both documented by the author: a copy-buffer bounce (D2) would copy from `b_addr` as if
  contiguous (wrong data), and the immu (IOMMU) bind path was not checked for `v_priv` handling.
  Further caveat: in `lkpi_sg_dma_map()` a page with no KPI `struct page` (`pfn_to_page() == NULL`)
  ends the PFN walk with `contig = true`, so an entry that starts with KPI pages and continues into
  memory the KPI does not know (an imported foreign buffer) would again be bound by VA range. All
  pages of an order-N `alloc_pages()` block are registered in `lkpi_pages_by_pfn`, so i915's own
  objects never hit this; failing the map (or treating it as non-contiguous) would be safer.
- L2 fix: `thread_join(t_did)` after the creator waited for the id; detached threads no longer
  touch their descriptor after `fn`.
- L1 fix: `lkpi_plat_signal_pending()` now reports exactly the `cv_wait_sig()` early-return set.
- L3 fix: per-queue cap 256, workers created on demand by the manager thread.
- C1 fix: `xc_call()` to all CPUs with preemption disabled, handler runs `wbinvd` on each.
- C2 fix: per-page `hat_unload(HAT_UNLOAD_UNLOCK)` + `hat_devload(HAT_LOAD_LOCK)` with the new
  attribute keeps the lock count balanced. As with illumos' own non-WB DMA memory
  (`kmem_override_cache_attrs`), the segkpm alias of the page stays write-back, i.e. a PAT alias
  that Intel documents as undefined; illumos tolerates it, so not raised as a finding.
- IOMMU detection: `immu_running`, `immu_dvma_enable`, `immu_gfxdvma_enable` are globals of
  drv/rootnex (immu.c:73-105; Makefile.files ROOTNEX_OBJS), so the extern references resolve with
  `MODDEPS drv/rootnex`.

## MMIO / forcewake / uncore path (task b)

Verified OK, no finding:

- i915 maps its registers with `ioremap(GTTMMADR + 0, 2 MiB)` (intel_uncore.c `uncore_mmio_setup`)
  and the GGTT with `ioremap_wc(GTTMMADR + 8 MiB, 8 MiB)` (KBL is not GEN9_LP, so WC); the aperture
  with `io_mapping_init_wc`. All three go through `gfxp_map_kernel_space()`
  (gfxp_vm.c:85-131): UC = `HAT_STRICTORDER|HAT_PLAT_NOCACHE` (PCD|PWT, strong UC), WC =
  `HAT_MERGING_OK|HAT_PLAT_NOCACHE` (PAT WC; overrides an MTRR UC range). `pci_iomap` is not used by
  i915.
- `readl/writel` are single volatile 32-bit accesses; `readq/writeq` single 64-bit `mov` on amd64
  (GGTT PTEs are written with `writeq`, as `gen8_set_pte`); no byte swapping (x86 LE,
  `DDI_NEVERSWAP_ACC`). `intel_uncore_read64_2x32` is built from `readl`.
- Barriers: `wmb`=sfence, `rmb`=lfence, `mb`=mfence, `smp_mb` seq-cst fence; GGTT update ordering
  relies, as on Linux, on the UC write to `GFX_FLSH_CNTL_GEN6` draining the WC buffer
  (intel_ggtt.c:218-231).
- Forcewake: `wait_for_atomic()` uses `local_clock()` = `ktime_get_ns()` = `gethrtime()`,
  `preempt_disable` is a barrier and `smp_processor_id()` the current CPU, so the migration
  re-basing in `_wait_for_atomic` is harmless. The auto-release `hrtimer` runs in the KPI timer
  thread instead of hardirq; it takes `uncore->lock` (adaptive mutex), and
  `intel_uncore_forcewake_reset()` cancels it without holding that lock, so no deadlock;
  `hrtimer_active()` covers queued or running, as the reset loop needs.
- `udelay` = `drv_usecwait`; sleeps >= 10 us use high-resolution `cv_timedwait`, so `usleep_range`
  in register polls is not rounded to the 10 ms tick.

## Probe path (task a): Gen9.5 walk and the first 10 likely failure points

Path walked in the imported tree: `i915_pci_probe` (i915_pci.c:956) -> `i915_driver_probe` ->
`i915_driver_create` (devm_drm_dev_alloc, pci_set_drvdata) -> `pci_enable_device` ->
`i915_driver_early_probe` (workqueues, `intel_region_ttm_device_init` -> `ttm_device_init`,
`intel_detect_pch`) -> `i915_driver_mmio_probe` (`intel_gmch_bridge_setup`,
`intel_uncore_init_mmio`, `intel_gmch_bar_setup`, fuses) -> `i915_driver_hw_probe`
(`i915_set_dma_info`, `i915_ggtt_probe_hw` incl. scratch page, `aperture_remove_conflicting_pci_devices`,
`i915_ggtt_init_hw`, memory regions/stolen, `pci_set_master`, `pci_enable_msi`,
`intel_opregion_setup`, pcode, `intel_dram_detect`) -> display noirq (VBT, power wells, DMC work,
GMBUS adapters, cdclk, `intel_vga_disable`) -> `intel_irq_install` -> display nogem (outputs, AUX/DDC,
initial plane, HW state readout) -> `i915_gem_init` (GT, engines, `__engines_record_defaults`) ->
display probe -> `i915_driver_register` (`drm_dev_register`, uevents, ACPI/opregion notifiers,
audio component) -> drmglue minor nodes.

KPI calls on this path that were checked and found present with Linux semantics (no stub that
returns a wrong answer): `pm_runtime_get_if_in_use/_active` report "active" (so display HW readout
is not skipped), `drm_privacy_screen_get` returns -ENODEV (no THINKPAD_ACPI/CHROMEOS lookup
compiled in, so no permanent -EPROBE_DEFER), `request_mem_region` (not dereferenced), `dma_set_mask`,
`hrtimer_*`, `ioremap*`, `io_mapping_*`, `pci_read_config_*` on the GPU and on 0:0.0,
`inb/outb` (argument order fixed in `<asm/io.h>`), firmware loader, i2c adapter registration.

Most likely failure or hang points on real KBL-R hardware, in probe order of impact:

1. **Hang in `intel_gmch_bridge_setup()` / `intel_detect_pch()`**: W1, devinfo lock-order deadlock
   (any concurrent snapshot during attach).
2. **GPU writes to foreign memory on the first shmem/internal object bound with merged sg entries**:
   docs/12 M1 (GGTT scratch and internal objects use order>0 pages, which are VA-contiguous and
   safe; the first shmem objects, e.g. engine default state from `__engines_record_defaults`, are not).
3. **`intel_irq_install()` fails if MSI cannot be allocated**: P2.
4. **`__engines_record_defaults()` / `intel_gt_wait_for_idle()` times out and wedges the GPU**:
   the first real submission needs MSI delivery, the execlists tasklet (a work item on the
   16-worker `system_highpri_wq`, docs/12 L3), `dma_fence` callbacks and `i915_request_wait`
   (docs/12 L1 if interrupted). Wedging is non-fatal (`i915_gem_init` turns -EIO into "wedged"),
   so the display still comes up, but there is no acceleration. Not traced further.
5. **Boot console lost after `intel_vga_disable()`** if the machine booted in CSM/VGA text mode
   (it turns the VGA screen off; tem keeps writing to 0xb8000). UEFI GOP boots are unaffected.
   Not a probe failure, but "no console" on such systems.
6. **No PCH found** if the LPC function were not visible as a node with "vendor-id"/"reg"
   properties (it is the `isa` nexus node on illumos; expected to be found). Without it,
   `HAS_PCH_SPLIT` is false and display (GMBUS pins, hotplug, backlight) is programmed for the wrong
   PCH; outputs would not be detected. Low likelihood, high impact; check `prtconf -v` for
   `isa@1f` vendor-id 0x8086.
7. **OpRegion mapping**: `memremap(ASLS, 8 KiB, MEMREMAP_WB)` uses `hat_devload`; fine for ACPI NVS
   (not in phys_install), but would hit the locked-page assertion noted in docs/12 if a firmware
   placed the OpRegion in E820 RAM. Without OpRegion there is no VBT and panel power sequencing uses
   defaults (eDP may not light up).
8. **DMC firmware missing** (`/usr/lib/firmware/i915/kbl_dmc_ver1_04.bin`): logged, DC states
   stay off. Not fatal.
9. **Console corruption on the first resume or VT switch after X started**: G1.
10. **Slow output probing**: GMBUS uses its interrupt (`gmbus_wait`); if GMBUS interrupts are not
    delivered (MSI routing problems), every DDC transfer waits for the timeout and falls back to
    bit-banging (`GMBUS_FORCE_BIT_RETRY`), adding tens of ms per transfer; DP AUX similarly polls
    after a timeout. Works, slowly.

## Not verified / limits of this review

- Nothing was built or run on illumos. In particular unverified: that `pci_config_setup()` succeeds
  on the host-bridge node in its actual DS state at i915 attach time (npe does it for driverless
  children, so expected to work); the ctfconvert behaviour behind B1; S3 config-space restore of the
  iGPU by the nexus.
- W1's timing (how often devfsadm/syseventd snapshot during i915 attach at boot) was not measured.
- G1 assumes 6.14 releases the initial plane fb (and its GGTT node) on the first full modeset by a
  DRM master; `intel_plane_initial.c` was not traced line by line.
- The probe walk (task a) checked the KPI calls listed above; GT/engine bring-up
  (`intel_engines_init`, execlists, GuC-less submission) and the atomic modeset paths were not
  traced call by call.
- Not reviewed: drmglue `vmoff.c`, `xlate.c`, `segdev_illumos.c` beyond what docs/12 covered;
  `linux_shmem.c`/drmglue `shmem.c` (M1/M3 area); ACPI method evaluation details.

## Fix status (KPI agent, 2026-11-27)

Userland-tested where possible, compile-checked against stub illumos headers; nothing run on
illumos.

| ID | Status | What was done |
|----|--------|---------------|
| W1 | fixed | `lkpi_plat_pci_walk(scope_dip, ...)`: from attach/resume (`current->lkpi_attach_dip`, set by `lkpi_pci_attach` around probe and resume) only the siblings under the parent the framework already holds are scanned, without `ndi_devi_enter`; `ddi_walk_devs(root)` only outside autoconfiguration. Not covered: enumeration from work items the attach waits for (none in the i915 probe path) |
| P1 | fixed | the wrapper of the attached dip holds no `ndi_hold_devi`; foreign dips found by enumeration are held (`lkpi_dip_held`), and an enumeration hold is dropped when the device attaches |
| P2 | fixed | implicit INTx vector when a driver binds and after a failed `pci_enable_msi()`, replaced when MSI/MSI-X is allocated, released at unbind if unused. `review/repro_irq_no_intx` passes |
| P3 | fixed | `kzalloc` checked, BAR mapping unmapped on failure |
| C3 | fixed | the block count byte is ACKed only when valid, NAKed otherwise before STOP; `I2C_M_NO_RD_ACK` skips the acknowledge clock; test in test_hw (valid and invalid counts, bus released) |
| F1 | fixed | `__atomic_thread_fence(SEQ_CST)` between the mutex release and the waiter-count load |
| F2 | fixed | no over-allocation: power-of-two sizes of PAGE_SIZE or more are headerless page-aligned `kmem_alloc` blocks (size in an XArray keyed by page), smaller sizes keep the 16-byte header (16-byte alignment, documented deviation from Linux's natural alignment below PAGE_SIZE) |
| R-K8 (docs/11) | fixed | the KPI no longer defines `EXPORT_SYMBOL_FOR_TESTS_ONLY`; `<drm/drm_util.h>` does |
| R-B2 (docs/11) | fixed | `build/Makefile.master` gives ctfconvert/ctfmerge `-l psi915-<git describe>` for every module |

Module set after these changes: syntax-check 374/374, `scripts/kpi-linkcheck.sh` 0 unresolved
(`ddi_devmap_segmap` and `kcred`, used by the new console glue, added to the illumos allowlist).
