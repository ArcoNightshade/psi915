# Integration notes: module set, build, i915 attach glue, console, install

Status 2026-11-26. Written in the Linux dev container. **Nothing in this document has been built, loaded
or run on illumos.** What has run: `scripts/gen-module-objs.sh`, `gmake -n` of every module, the
`syntax-check` target with `HOST_STUBS=1` (stub illumos headers, so it measures the KPI header coverage
only), `scripts/install-all.sh` into a temporary root (firmware only; the kmods do not exist yet), and
`scripts/fetch-i915-firmware.sh` against gitlab.com. Section 9 lists every untested piece.

| Piece | Files |
|-------|-------|
| Module Makefiles | `build/drm/Makefile.lkpi` (shared rules), `build/{drm,linuxkpi_gpl_g1,linuxkpi_gpl_g2,i915}/Makefile` |
| Generated object lists | `build/*/Objects.mk` from `scripts/gen-module-objs.sh` |
| Module linkage of the misc modules | `build/drm/linuxkpi_drm_mod.c`, `build/linuxkpi_gpl_g1/gpl_g1_mod.c`, `build/linuxkpi_gpl_g2/gpl_g2_mod.c` |
| i915 attach glue | `drivers/i915_glue/i915_ddi.c` (illumos side), `i915_drm_glue.c` (Linux side), `i915_glue.h`, `i915.conf` |
| Install | `scripts/install-all.sh`, `build/drm/install-gfx.manifest`, `scripts/fetch-i915-firmware.sh` |
| Build driver | `scripts/build-gfx.sh` (all modules in dependency order) |

## 1. Module set and dependency graph

```
             i915  (drv, /usr/kernel/drv/amd64/i915)
            /  |  \_____________________
           /   |                        \
          v    v                         v
 linuxkpi_gpl_g2 --> linuxkpi_drm --> linuxkpi_gpl_g1 --> linuxkpi --> misc/gfx_private, misc/acpica
 (GPL helpers)       (DRM core)       (dma-buf group     (KPI core)
                                       + drmglue)
```

Every arrow is a DT_NEEDED entry (`ld -N misc/<name>`, the gate's mechanism; krtld reads it, see
`kobj.c`, and warns about the deprecated `_depends_on` array, which is not used). i915 lists all of
`misc/linuxkpi misc/linuxkpi_gpl_g1 misc/linuxkpi_drm misc/linuxkpi_gpl_g2 misc/gfx_private misc/acpica`
so its direct calls resolve without relying on transitive loading.

| Module | Type, path | Contents | Licence of the binary |
|--------|------------|----------|-----------------------|
| linuxkpi | misc, /usr/kernel/misc/amd64 | KPI core (`src/`, unchanged, built by `build/linuxkpi`) | BSD-2 |
| linuxkpi_gpl_g1 | misc | gpl/ `dma-buf.c dma-fence.c dma-fence-array.c dma-fence-chain.c dma-fence-unwrap.c sync_file.c` (G1), drm/ `dma-buf/dma-resv.c` (MIT), drmglue/ (BSD, with the default `DRMGLUE_IN=g1`) | GPL-2.0 (combined) |
| linuxkpi_drm | misc | drm/ sets core+link except dma-resv.c: DRM core, atomic/KMS helpers, display helpers (DP, MST, DSC, HDMI, SCDC, dual-mode), TTM, drm_buddy, mipi_dsi, panel/bridge/privacy-screen, drm/drivers/video (aperture, hdmi); drm_bsd/ DRM core and video (drm_managed, drm_sysfs, drm_lease, drm_writeback, cmdline, nomodeset); optionally the DRM scheduler (`WITH_DRM_SCHED=1`, set "extra", unused by i915) | MIT/HPND/dual + BSD-2 |
| linuxkpi_gpl_g2 | misc | gpl/ `display/drm_hdcp_helper.c`, `drm_gem_framebuffer_helper.c` (G2) | GPL-2.0 (combined) |
| i915 | drv, /usr/kernel/drv/amd64 + /usr/kernel/drv/i915.conf | i915/ sets core (184 files) + link (100 files), drm_bsd `intel_acpi.c`, drivers/i915_glue | MIT + BSD-2 |

Deviations from the brief, and why:

1. **dma-resv.c is in linuxkpi_gpl_g1, not linuxkpi_drm.** dma-buf.c calls `dma_resv_init/fini/
   get_singleton/...`; with dma-resv in drm, G1 would need drm and drm already needs G1
   (`dma_fence_*` from drm_syncobj, drm_prime, drm_gem, ...): a load-order cycle. docs/08 section 5.2
   already puts it in G1. It is MIT; linking it into the GPL module is allowed.
2. **drmglue is in linuxkpi_gpl_g1 by default, not linuxkpi_drm.** G1 needs drmglue's file layer
   (`fd_install`, `get_unused_fd_flags`, `fget/fput`, `anon_inode_getfile`, `alloc_file_pseudo`,
   `alloc_anon_inode`, `kern_mount`, `init_pseudo`, poll-wait glue; docs/10 R9) and may depend on
   linuxkpi only. drmglue cannot be split from outside drmglue/ (`anonfs_illumos.c` uses the host major
   that `cdev_illumos.c` records). `DRMGLUE_IN` in `build/drm/Makefile.lkpi` selects the module:
   `g1` (default, loadable), `drm` (as in the brief; then G1 needs `misc/linuxkpi_drm`, a cycle, not
   loadable), `none` (for the target layout: drmglue linked into linuxkpi, request R-K2). BSD code in a
   GPL combined module is licence-compatible, but it makes the BSD file layer unavailable without the
   GPL module; R-K2 fixes that.
3. **The scheduler** (drm/scheduler, set "extra") is not built by default: i915 does not use it.
   `gmake -C build/drm WITH_DRM_SCHED=1` adds it and its `module_init`.

Exports. illumos has no export control: every global symbol of a kmod is visible to the modules that
depend on it, and `EXPORT_SYMBOL*` is a no-op in the KPI (`<linux/module.h>`). In practice:

| Module | Linux EXPORT_SYMBOL* in its sources | Other interfaces |
|--------|----------------------------------|------------------|
| linuxkpi | (KPI, not Linux exports) | the whole KPI: `lkpi_*`, Linux-named functions of src/, `lkpi_pci_attach/detach`, `lkpi_ddi_getinfo`, `lkpi_module_params_apply`, `linuxkpi_version` |
| linuxkpi_gpl_g1 | 76 (58 in G1 proper per docs/08, plus dma-resv) | drmglue: `drmglue_cb_ops`, `drmglue_getinfo`, `drmglue_attach_node`, `drmglue_detach_node`, `drmglue_ioctl_abi`, `lkpi_node_*`, `lkpi_cdev_*`, `lkpi_vma_*`, the Linux file layer (`fget`, `fput`, `fd_install`, ...) |
| linuxkpi_drm | 986 (drm, kms helper, display helper, ttm, buddy, mipi_dsi, video, drm_bsd) | `video_bsd_set_option`, `video_bsd_set_firmware_drivers_only`, `drm_bsd_set_uevent_hook` |
| linuxkpi_gpl_g2 | 15 | - |
| i915 | 5 (i915 exports for mei/gvt, unused) | the driver's dev_ops; tunables `psi915_*` |

Consequence of the missing export control: two modules defining the same global symbol resolve
silently to the first one krtld finds. Module parameter descriptors are globals named
`__lkpi_param_<name>` (`<linux/moduleparam.h>`); drm (`debug`, `vblankoffdelay`, ...) and i915 must
not share a name, and within i915 the i915 and display parameter names must stay disjoint (they are in
v6.14). R-K4 asks for a module prefix in the descriptor name.

## 2. Load order; boot time versus run time

Load order follows the DT_NEEDED chain: loading i915 makes krtld load linuxkpi, linuxkpi_gpl_g1,
linuxkpi_drm, linuxkpi_gpl_g2 first, each `_init` in that order:

1. linuxkpi `_init`: KPI service threads, RCU, workqueues.
2. linuxkpi_gpl_g1 `_init`: `drmglue_init()` (poll taskq, segdev/anon backends, anonymous vfs),
   then `dma_buf_init` (pseudo mount through drmglue).
3. linuxkpi_drm `_init`: `mipi_dsi_bus_init` (postcore), `drm_core_init`,
   `drm_display_helper_module_init`, `drm_buddy_module_init` (and `drm_sched_fence_slab_init` with
   the scheduler). krtld walks no initcall section, so these are named explicitly; exits run in reverse
   from `_fini`.
4. linuxkpi_gpl_g2 `_init`: nothing to initialise.
5. i915 `_init` (LKPI_PCI_DRIVER_MODULE): `psi915_i915_init()` finishes the dev_ops/cb_ops, applies
   `psi915_nomodeset`, runs `i915_init` (slab caches, PMU, `pci_register_driver`), then `mod_install`.
6. attach(9E) for each bound PCI function (section 4).

Unload is the reverse: `modunload` of i915 succeeds only after detach; the misc modules stay until no
dependent is loaded. `ddi-no-autodetach=1` in i915.conf keeps i915 attached (no autodetach of an
unsuspendable display driver).

Boot time. If i915 is bound to the boot display (driver_aliases), it attaches during
`consconfig()`: consconfig resolves `plat_fbpath()` (the first VGA-class display dip, `consplat.c`)
with `ddi_pathname_to_dev_t()`, which attaches the node and, without a minor name, picks the **first
minor node** of the dip (`resolve_pathname()` in `devcfg.c`). So at boot the whole chain is loaded
from /usr/kernel and i915 probes (firmware read through kobj from /usr/lib/firmware) before the
console is configured. Requirements and consequences:
- /usr must be on the root pool (true for OpenIndiana and Tribblix default installs).
- The console minor `text-0` must be created before card0/renderD128 (it is, section 4).
- A probe failure must not leave the system without a console: the glue then stays attached in
  console-only mode (vgatext cannot bind the device once the i915 alias exists).
- A hang in probe hangs the boot. Bring-up therefore uses a new boot environment and a serial console
  (section 6), and `psi915_nomodeset=1` in /etc/system is the escape hatch (console-only attach).

Run time. Without the alias installed (or with a non-console GPU) the modules can be loaded by hand:
`modload /usr/kernel/misc/amd64/linuxkpi`, then `linuxkpi_gpl_g1`, `linuxkpi_drm`, `linuxkpi_gpl_g2`
(each must load and unload cleanly: the P3 smoke test), then `add_drv` / `devfsadm -i i915` for a
device not held by another driver. The console GPU is held open by tem through vgatext, so it can only
be taken over by a reboot with the alias in place.

## 3. Build system

```
scripts/build-gfx.sh                    # all modules in dependency order (gmake on illumos)
scripts/build-gfx.sh all DEBUG=1        # debug64
scripts/build-gfx.sh -n                 # gmake -n: every command, nothing run
scripts/build-gfx.sh syntax-check -j8   # per-TU -fsyntax-only report, never stops
scripts/build-gfx.sh syntax-check HOST_STUBS=1 -j8   # same on Linux against stub illumos headers
gmake -C build/i915 sizecheck           # section sizes of the linked kmod
scripts/gen-module-objs.sh [-c]         # regenerate (or check) build/*/Objects.mk
```

Object lists. `scripts/gen-module-objs.sh` reads `third_party/IMPORT_FILES` (the manifests resolved
against the pinned tag by `scripts/import-linux.sh`, so no Linux clone is needed), `drm_bsd/MANIFEST`
and `third_party/gpl-policy.conf`, and writes one `Objects.mk` per module (section 1 assignment). It
refuses compiled gpl/ files outside the G1/G2 lists. The patch series is already applied to drm/,
i915/, gpl/ by the import, so the lists name tree files. Rerun it after every re-import; `-c` fails when
a committed list is stale (suitable for CI).

Flags (`build/drm/Makefile.lkpi` on top of `build/Makefile.master`):
- illumos kernel flags unchanged (`-mcmodel=kernel -mno-red-zone -msave-args -fno-clone-functions`,
  retpolines, `-fno-omit-frame-pointer`, DWARF 4 for CTF, `-D_KERNEL ...`).
- kbuild semantics: `-D__KERNEL__ -DKBUILD_MODNAME='"<mod>"' -DKBUILD_BASENAME='"<file>"'`,
  `-include include/linux/kconfig.h` (which includes `drm/include/generated/autoconf.h`, generated
  from `third_party/config/gen9.config`), `-fno-strict-aliasing -fno-strict-overflow
  -fno-delete-null-pointer-checks -fno-common -fconserve-stack`.
- Warnings: imported code `-Wall` with kbuild's usual exceptions and no `-Werror` (`LINUX_WERROR=1`
  restores it); psi915 glue keeps `-Wall -Wextra -Werror`.
- Per-module `-D`: `MODULE_DEFS` (`PSI915_DRMGLUE_HERE`, `PSI915_WITH_DRM_SCHED`); per-file:
  `CFLAGS_<basename>` like kbuild's `CFLAGS_<obj>.o`.
- Include order: module dirs (drm_bsd i915 dirs for i915), drm_bsd/include, drm_bsd/drivers/gpu/drm,
  drmglue/include, KPI include/, drm/include, drm/include/uapi, i915/include, the driver root
  (`-I$(src)` of kbuild: drm/drivers/gpu/drm, i915/drivers/gpu/drm/i915 and its display/), then the
  GPL headers: gpl/include for G2 only; for MIT/BSD modules only the `allow-header` debts of
  gpl-policy.conf, linked into `obj64/gplallow/`, so the GPL boundary is enforced by the compiler; G1
  sees no DRM or GPL include directory at all (policy check 4).
- Objects mirror the source tree under `obj64/objs/` (no basename collisions; the i915/ tree cannot
  collide with the module file `obj64/i915`).
- CTF: `ctfconvert` per object and `ctfmerge` per module when the tools exist (as build/linuxkpi);
  failures on imported DWARF are warnings (`CTF_STRICT=1` makes them fatal), the kmod then has no CTF.

Module size. One kmod per module is kept; no split. Reasons: `ld -r -ztype=kmod` has no practical size
limit; krtld places module text in `heaptext_arena`, 128 MiB for all modules on amd64
(`HEAPTEXT_SIZE`, i86pc machparam.h), after the 2 MiB MODTEXT area; data comes from the kmem heap.
The i915 Gen9 set is 268k lines of C (drm: 76k). For scale, a full Linux x86_64 i915.ko (all
generations, ~410k lines) has roughly 3-4 MiB of text; the Gen9 set should be below that, plus
symbol and string tables and CTF that krtld keeps loaded. DWARF sections are not loaded (non-ALLOC)
but make the file large (expect tens of MiB). `sizecheck` warns above `MODSIZE_WARN_KB` (32 MiB
loaded size). Splitting i915 would not help: display and GT reference each other throughout, so two
kmods would need mutual DT_NEEDED entries, which krtld cannot load. If a size problem appears, first
drop set "link" files by stub patches (100 i915 files, docs/08 3.4).

Syntax-check snapshot (HOST_STUBS=1 against the KPI at merge 9cfcdba; stub headers, so this tracks
KPI header coverage, not illumos compatibility). Report per module in
`build/<mod>/obj64/syntax-report.tsv`, summary in `build/i915/obj64/gfx-syntax-summary.tsv`:

| Module | TUs | pass | first errors of the failures (most frequent) |
|--------|-----|------|--------------------------------------------|
| linuxkpi_gpl_g1 | 18 | 17 | `uapi/linux/dma-buf.h` missing (dma-buf.c) |
| linuxkpi_drm | 77 | 48 | `BUILD_BUG_ON` non-constant expression (10, R-K5), `video/cmdline.h`, `__read_mostly`, `video/mipi_display.h`, `video/of_videomode.h`, `drm/drm_format_helper.h` (R-G1) |
| linuxkpi_gpl_g2 | 3 | 3 | - |
| i915 | 287 | 16 | `BUILD_BUG_ON` non-constant in ttm_device.h (241, R-K5), `struct tasklet_struct` incomplete (16), `u32` |

All psi915 glue units pass (module linkage files, drmglue, `i915_ddi.c`, `i915_drm_glue.c`), against
stubs. The KPI agent's `scripts/p3-compile-check.py` measures the same with slightly different include
paths (it puts all of gpl/include on the path); the build's numbers are the ones that predict the
real build.

## 4. i915 attach glue (drivers/i915_glue)

Two halves with a plain-C interface (`i915_glue.h`), because illumos DDI headers and Linux DRM headers
cannot share a translation unit cleanly (platform.h ordering, `mutex_init` and friends are renamed):

- `i915_ddi.c` (illumos headers first, then `<linuxkpi/ddi.h>` and drmglue's illumos header):
  `LKPI_PCI_DRIVER_MODULE(i915, ..., psi915_i915_init, psi915_i915_exit)`; `psi915_i915_init()` sets
  `devo_cb_ops` (a copy of `drmglue_cb_ops` with open/close/ioctl/devmap wrapped for the console
  minor), `devo_getinfo = psi915_getinfo` (drmglue's minor layout is `clone << 8 | node`, which
  `lkpi_ddi_getinfo` would decode as an instance), `devo_attach/detach` (wrappers around
  `lkpi_pci_attach/detach`), before `mod_install`. The macro has no parameters for these, hence the
  assignment (R-K3).
- `i915_drm_glue.c` (compiled like an i915 file): DRM minors of the probed device, the ioctl-size
  callback, the parameter table, nomodeset, console hooks.

attach(DDI_ATTACH):
1. One instance only (drmglue hosts anonymous files on the first DRM dip; KBL-R has one GPU).
2. Console: `gfxp_fb_softc_alloc` + `gfxp_fb_attach` (bitmap framebuffer from the boot loader on UEFI,
   VGA text otherwise; sets `primary-controller`), then `ddi_create_minor_node("text-0", minor
   0xfe, DDI_NT_DISPLAY)`, the first minor of the dip (consconfig, section 2).
3. `lkpi_module_params_apply(dip, psi915_i915_params())`: i915.conf properties named like Linux
   parameters (23 of them, `i915_drm_glue.c`).
4. `lkpi_pci_attach(dip, DDI_ATTACH)`: pci_dev wrapper, `i915_pci_probe` -> `drm_dev_register`.
5. `psi915_drm_nodes()` reads `dev->primary` and `dev->render` of the drm_device stored as PCI
   drvdata (`pci_set_drvdata(pdev, &i915->drm)`) and calls `drmglue_attach_node(dip, index,
   "card0"/"renderD128", dev->driver->fops, psi915_drm_kcmd)` (DDI_NT_DISPLAY_DRM). The driver fops are
   used directly: drm_open finds the minor from `iminor()`, which drmglue sets, so `drm_stub_fops`
   (static) is not needed.
6. `gfxp_bm_register_fbops(&psi915_blt_ops)` (KDSETMODE hook), `callb_add(CB_CL_PANIC)`.
7. If 4 or 5 fail and the console attached: stay attached, console only (`psi915_console_fallback`).

ioctl-size callback (`psi915_drm_kcmd`, docs/10 2.4): core numbers 0x00-0x3f and 0xa0-0xff from a
table built from every `DRM_IOCTL_*` definition of uapi drm.h (`drm_ioctls[]` is static in
drm_ioctl.c); driver numbers 0x40-0x9f from `drm_file->minor->dev->driver->ioctls[nr - 0x40].cmd`.

Device nodes and links: minors `text-0` (console, `ddi_display`), `card0`, `renderD128`
(`ddi_display:drm`). `/dev/dri/*` come from the devlink.tab line in
`userland/config/devlink.tab.psi915` (or gfx-drm's link module if installed). Permissions:
`userland/config/minor_perm.psi915` plus `text-* 0600 root sys`, passed to add_drv by the printed
command.

detach(DDI_DETACH): refused while the console minor or a DRM node has open files (the framework
already refuses while any minor is open); then drmglue nodes, `lkpi_pci_detach` (remove, devres),
panic callback, console minor, gfxp state. Untested and of limited value: tem keeps the console open,
so a console GPU never detaches.

Power management: DDI_SUSPEND -> `lkpi_pci_detach(DDI_SUSPEND)` = dev_pm_ops prepare, suspend,
suspend_late, suspend_noirq (i915_pm_ops), then `gfxp_fb_detach(DDI_SUSPEND)`; on gfxp failure the
Linux side is resumed again. DDI_RESUME -> `lkpi_pci_attach(DDI_RESUME)` = resume_noirq,
resume_early, resume, complete, then `gfxp_fb_attach(DDI_RESUME)` (console redraw). Suspend is P7
work; illumos S3 on this laptop class is itself unverified. quiesce(9E) is
`ddi_quiesce_not_supported` (from the macro), so fast reboot is disabled while i915 is loaded.

Tunables: `psi915_nomodeset`, `psi915_console_fallback`, `psi915_enable_console` (i915 module,
/etc/system), `drmglue_ioctl_abi` (linuxkpi_gpl_g1), i915.conf properties; all listed in
`drivers/i915_glue/i915.conf`.

## 5. Console handoff design

Precedents read: illumos-gate f71cb08 `uts/intel/io/vgatext/vgatext.c` (a thin wrapper over
`gfxp_fb_*` since the framebuffer console work), `uts/i86pc/io/gfx_private/gfxp_fb.c`,
`gfxp_bitmap.c`, `uts/i86pc/io/consplat.c`, `uts/common/io/consconfig_dacf.c`, `uts/common/io/tem.c`,
`uts/common/os/panic.c`; gfx-drm `drm_sunmod.c`, `drm_fb_helper.c`, i915 `i915_gem.c` (`fbcon_obj`).

5.1 How the console reaches the driver today. consconfig opens `plat_fbpath()` (first display dip;
without minor name the first minor) and tem issues `VIS_DEVINIT` through LDI; `gfxp_fb_ioctl` maps the
boot framebuffer (`fb_info.paddr`, from the loader; on KBL-R UEFI this is the GOP framebuffer in the
GMADR aperture, BAR2) and returns polled-I/O display/copy/cursor routines. Xorg/VT switching sends
`KDSETMODE` (KD_GRAPHICS / KD_TEXT) to wscons, which forwards it to the fb device;
`bitmap_kdsetmode()` calls the driver's `blt_ops.setmode` hook (registered by
`gfxp_bm_register_fbops`) and stops/resumes tem drawing.

5.2 Takeover (implemented in the glue). i915 attaches in place of vgatext (alias beats the class
alias) and serves the console minor through the same gfx_private calls, so tem, `/dev/fb` and
wscons see no difference until KMS changes the display. i915's own initial-plane takeover
(`intel_plane_initial.c`: the BIOS framebuffer is wrapped as a stolen-memory GEM object and pinned at
its original GGTT offset) keeps the boot framebuffer scanned out at the same aperture address, so tem
keeps drawing through its existing mapping. No fbdev emulation is built (CONFIG_DRM_FBDEV_EMULATION
off, set "fbdev" not imported), so no in-kernel DRM client competes with tem.

5.3 The three problems and the design to fix them (not implemented; patch requests R-P1..R-P3):

- R-P1 Keep the console framebuffer alive. After the first modeset that replaces the initial
  framebuffer, i915 frees the initial-plane object and its GGTT range, and tem would write through
  BAR2 into whatever is bound there next (GPU-visible corruption). Patch: on `CONFIG_PSI915`, keep the
  initial-plane vma pinned and its GGTT node reserved for the life of the device (a
  `drm_mm_reserve_node` like `ggtt->uc_fw`, at `plane_config->base`, size of the boot framebuffer),
  and keep a `drm_framebuffer` referencing it. gfx-drm did the equivalent by allocating `fbcon_obj`
  at GGTT offset 0 and copying the old GTT contents. If the firmware gave no framebuffer
  (`fb_info` type EGA text), there is nothing to keep and the console stays VGA text until the first
  modeset, after which it is invisible (same as gfx-drm).
- R-P2 Restore on KD_TEXT (`psi915_drm_console_setmode`, called from `blt_ops.setmode` in process
  context). Record at takeover the CRTC, connector, mode and the R-P1 framebuffer; on KD_TEXT, if no
  DRM master holds the device (`drm_master_internal_acquire`), build an atomic state that scans out
  the console framebuffer on that CRTC with that mode, as `drm_client_modeset_commit_locked` does for
  fbdev, and commit it; then return 0 so gfxp redraws tem's text. On KD_GRAPHICS do nothing (the master
  owns the display). This is i915-independent DRM code and can live in i915_drm_glue.c once the KPI and
  drm_client pieces compile; the only i915 part is R-P1.
- R-P3 Panic (`psi915_drm_console_panic`, from the CB_CL_PANIC callback, which `panic()` runs after
  stopping the other CPUs and before printing the banner with polled I/O): no locks, no sleeping, no
  allocation. Write the Gen9 primary plane registers of the pipe recorded in R-P2 directly
  (PLANE_CTL format/tiling linear, PLANE_STRIDE, PLANE_SURF = GGTT offset of the console framebuffer,
  then PLANE_SURF again to latch), skipping the atomic machinery, and force DC states off
  (DC_STATE_EN = 0) so the plane registers are powered. Problem outside the driver: tem only draws while
  its `tvs_fbmode` is KD_TEXT (`tem.c`), and nothing on the panic path resets it, so with Xorg running a
  panic stays invisible even with the plane restored. Needs an illumos change (tem drawing in panic
  regardless of fbmode, or a panic hook that sets KD_TEXT); until then panics under X are diagnosed from
  the serial console and the dump, which is how illumos behaves with vgatext today.

kmdb: on i86pc no driver hook runs on debugger entry (`CB_CL_ENTER_DEBUGGER` is executed only on
sun4); kmdb draws with tem's polled I/O and therefore only in KD_TEXT. Same limitation as vgatext:
use a serial console for kmdb while a DRM master is active. Reboot (CB_CL_MDBOOT) could restore the
boot framebuffer for the loader; not needed on UEFI, where firmware reinitialises GOP.

## 6. Installation and firmware

```
scripts/fetch-i915-firmware.sh             # -> build/i915/obj64/firmware (sha256-pinned)
beadm create psi915 && beadm mount psi915 /mnt
scripts/install-all.sh -R /mnt             # modules, i915.conf, firmware; prints the next steps
scripts/install-all.sh -d -R /mnt          # print the registration steps again
scripts/install-all.sh -f ips | -f svr4    # package layout (IPS actions / SVR4 prototype)
```

Layout: `/usr/kernel/misc/amd64/{linuxkpi,linuxkpi_gpl_g1,linuxkpi_drm,linuxkpi_gpl_g2}`,
`/usr/kernel/drv/amd64/i915`, `/usr/kernel/drv/i915.conf`,
`/usr/lib/firmware/i915/{kbl_dmc_ver1_04.bin,kbl_guc_70.1.1.bin,kbl_huc_4.0.0.bin,WHENCE}`.
Firmware names come from `intel_dmc.c` (`KBL_DMC_PATH`) and `intel_uc_fw.c` (KABYLAKE GuC 70.1.1,
HuC 4.0.0). Only DMC is used with the Gen9 default `enable_guc=0`. Source: the official linux-firmware
repository on gitlab.com (reachable from the dev container; git.kernel.org is tried second; a GitHub
mirror can be given with `-u`). LICENSE.i915 was not found at the expected raw path; WHENCE carries the
i915 licence line ("Redistributable").

The installer never runs add_drv, rem_drv, modload or devfsadm and never edits files in /etc. It
refuses to install on `/` when another i915 driver exists (gfx-drm's `/kernel/drv/amd64/i915`, which
would shadow `/usr/kernel` and owns the name). The printed sequence is:
1. remove gfx-drm's i915 if present (`pkg uninstall driver/graphics/drm` or `rem_drv i915`);
2. `add_drv -b /mnt -n -v -m 'card* 0600 root sys,renderD* 0666 root sys,text-* 0600 root sys'
   -i '"pci8086,5917"' i915` (`-A` on the installer lists every Gen9 alias of
   userland/config/driver_aliases.gen9);
3. add `type=ddi_display:drm<TAB>dri/\M0` to /etc/devlink.tab unless gfx-drm's link module exists;
4. optional /etc/system: `kmem_flags=0xf`, `linuxkpi:linuxkpi_bug_panic=1`;
5. reboot into the boot environment with a serial console; check `modinfo`, `/dev/dri`, `prtconf -D`.

## 7. Requests to other owners

KPI (include/, src/, build/linuxkpi):
- R-K1 `postcore_initcall` (drm_mipi_dsi.c) and `__exitcall` (dma-buf.c) in `<linux/module.h>`,
  defined like `module_init`/`module_exit` (the module linkage files call
  `lkpi_initcall_mipi_dsi_bus_init` and `lkpi_exitcall_dma_buf_deinit`). Also `arch_initcall`,
  `fs_initcall`, `core_initcall` for completeness.
- R-K2 Link drmglue into linuxkpi (add drmglue/src/*.c to LINUXKPI_OBJS with
  `-I drmglue/include`, call `drmglue_init/fini` from linuxkpi `_init/_fini`), then build with
  `DRMGLUE_IN=none`. This removes BSD infrastructure from the GPL module and matches docs/10 R1/R3
  (Linux file layer belongs to the KPI).
- R-K3 `LKPI_PCI_DRIVER_MODULE_OPS(name, desc, cbops, getinfo, attach, detach, init, exit)`: the
  current macro hardcodes getinfo/attach/detach, and its `(_cbops) != NULL ? (_cbops) : ...` is not a
  constant initializer for gcc when `_cbops` is an address, so only NULL can be passed. The glue works
  around both by assigning dev_ops fields in its init function.
- R-K4 Module-qualified parameter descriptor names (e.g. `__lkpi_param_<KBUILD_MODNAME>_<name>`)
  or a per-module table, to rule out cross-module collisions (section 1).
- R-K5 `BUILD_BUG_ON` as `_Static_assert` (include/linux/build_bug.h) rejects conditions that Linux
  only needs to be constant after optimisation, e.g. `BUILD_BUG_ON(__builtin_constant_p(mem_type) &&
  mem_type >= TTM_NUM_MEM_TYPES)` in the inline `ttm_manager_type()` (drm/ttm/ttm_device.h:279),
  included by almost every i915 file: 241 of the 271 failing i915 units and 10 drm units stop there.
  Linux uses `compiletime_assert()`: a call to an undefined `extern void f(void)
  __attribute__((error(msg)))` under `if (!(cond))`, which the optimiser removes when the condition
  is provably true. Suggest that form for BUILD_BUG_ON/BUILD_BUG_ON_MSG/BUILD_BUG, keeping
  `_Static_assert` for `static_assert`.
- R-K6a Missing in the syntax-check: `uapi/linux/dma-buf.h` (G1 dma-buf.c), `video/cmdline.h`,
  `video/mipi_display.h`, `video/of_videomode.h` (include/video is outside every manifest scope),
  `tasklet_unlock_spin_wait` and a complete `struct tasklet_struct` where i915_tasklet.h expects it
  (16 i915 units), `__read_mostly`.
- R-K6b A declaration of `video_bsd_set_firmware_drivers_only()` / `video_bsd_set_option()` in a header
  (`<video/nomodeset.h>` / `<video/cmdline.h>`); the glue declares the first itself for now.

GPL policy / imports (third_party/):
- R-G1 `allow-header include/drm/drm_gem_atomic_helper.h` includes `drm/drm_format_helper.h`, a GPL
  header that is not on the allow list. The build's allow-header include directory exposes the gap
  (drm_atomic_helper.c, i915 intel_atomic_plane.c fail on it). Either add it as a debt or replace
  drm_gem_atomic_helper.h with a BSD header in drm_bsd/.
  Done 2026-11-26: drm_gem_atomic_helper.h is replaced by a BSD header plus drm_gem_plane_helper_prepare_fb()
  in drm_bsd/, and the GPL pair is set "replaced" (docs/04).
- R-P1..R-P3 i915 console patches (section 5.3).

Build (build/Makefile, top-level Makefile):
- R-B1 Add `linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915` to `SUBDIRS` in build/Makefile (in that order,
  after linuxkpi), and let `make install` call `scripts/install-all.sh`. Until then use
  `scripts/build-gfx.sh`. Also list docs/11 in the README docs table.

Userland (userland/config):
- R-U1 Add `i915:text-* 0600 root sys` to minor_perm.psi915 (the installer adds it to the printed
  add_drv command).

## 8. Known blockers

1. Most imported translation units do not compile yet (section 3 snapshot); P3 KPI work is in
   progress. No kmod has been linked, so link-time gaps (undefined symbols that the KPI does not
   provide) are unknown; they appear only at modload on illumos, since kmods link with undefined
   symbols. A host-side `nm -u` allowlist check like drmglue's is the next step once objects compile.
2. R-K1 (initcall macros) is needed for linuxkpi_drm and linuxkpi_gpl_g1 to load at all.
3. Licence question docs/08 5.4 item 4 (CDDL kernel loading GPL-2.0 modules that exchange symbols with
   MIT/BSD modules) is open; the module split follows the owner's decision but is not legal advice.
4. Console: R-P1 is needed before any userspace modeset is safe with the console active (tem writes
   into GGTT-recycled memory otherwise). Until then bring-up should run with
   `psi915_enable_console=0` and a serial console, or not start a DRM master.
5. gfx-drm conflict: both are called i915; only one can be installed.
6. One device per system (drmglue host dip, single softc).

## 9. Not verified (all of it on illumos)

- Every Makefile on illumos: gmake, `/usr/bin/ld -ztype=kmod`, `-N` dependencies, ctfconvert/ctfmerge
  on imported DWARF, module sizes. Only `gmake -n` and HOST_STUBS syntax checks ran on Linux.
- All C in drivers/i915_glue and the module linkage files: compiled against stub headers only
  (`drivers/i915_glue/stubs/illumos`, drmglue and tests stubs), which are not illumos headers.
- The first-minor rule for the console path, `gfxp_fb_attach` on a non-vgatext driver, KDSETMODE
  reaching the hook, CB_CL_PANIC timing: read from illumos-gate source, not exercised.
- drmglue under a real driver (docs/10 V1-V9), devlink.tab escape `\M0`, minor_perm via add_drv -m.
- install-all.sh: ran on Linux (dash) into a temporary root for the firmware module and in dry-run mode
  for the rest; IPS/SVR4 output not consumed by pkg tooling. fetch-i915-firmware.sh: ran on Linux
  against gitlab.com; `digest -a sha256` path (illumos) not exercised.
- Boot-time attach from /usr/kernel during consconfig, firmware reads at that point, suspend/resume.

## 10. Module-load risk review (KPI side, 2026-11-26)

Checks a syntax check cannot do, run on the Linux host over the module set's objects compiled with
HOST_STUBS=1 (`scripts/kpi-linkcheck.sh`, then `scripts/kpi-modcheck.py`; reports in
`tests/out/linkcheck/` and `tests/out/modcheck/`). Host gcc/binutils, stub illumos headers: these
are indications, not an illumos link.

| Check | Result | Notes |
|-------|--------|-------|
| Unresolved symbols (`nm -u` vs module set + linuxkpi + illumos allowlists) | 0 (374 objects, after the import fixes of 3c9ee63) | last KPI additions: `intel_graphics_stolen_res` (filled from the GPU config space before probe), `intel_gmch_*` (Gen2-5 GTT: probe fails), `register/unregister_vmap_purge_notifier` (never called); `intel_pxp_gsccs_is_ready_for_sessions` resolved by the module build |
| Load order linuxkpi <- G1 <- drm <- G2 <- i915 | no reference to a symbol defined only by a later module | krtld resolves against MODDEPS only |
| Globals defined by more than one module | 0 (besides `_init/_info/_fini`) | krtld would bind the first one found |
| Weak symbols | 0 | an unresolved weak reference stays 0 at modload |
| Our globals that illumos also defines | 0 real (one false positive: `up` matched a comment in sys/protosw.h) | heuristic over usr/src/uts (C definitions, `extern` declarations, assembly ENTRY points); the reference tree has no usr/src/common (avl, list, nvpair, string, acpica, smbios), so those are not covered. Platform renames (mutex, page, file, module, cred, pid), lkpi_ string helpers and the module-qualified parameter names keep Linux names off illumos ones |
| Allowlisted illumos symbols not found in the tree | 21, all explained | ACPI CA and nvlist (declared without `extern` / in usr/src/common), `__x86_indirect_thunk_*` (generated by RETPOLINE_MKTHUNK), `CRED` (macro), `vn_hold` (drmglue stub artifact: illumos VN_HOLD is a macro; real headers resolve it) |
| Relocation types in loadable sections | only R_X86_64_64, PC32, PLT32, 32S | krtld has no GOT and no TLS; R_X86_64_32 would truncate kernel addresses silently. Debug sections carry R_X86_64_32 offsets, which is fine. Recheck with illumos gcc/ld, which may differ |
| Size (host objects, text bytes) | linuxkpi 144 KB, G1 33 KB, drm 550 KB, G2 7 KB, i915 2.5 MB (285 objects) | well under MODSIZE_WARN_KB; bss of linuxkpi is 68 KB (lock/hash tables) |
| Namespace of linuxkpi exports | every global is a Linux API name (v6.14 include/, arch/x86/include, or the imported drm/i915/gpl headers) or lkpi_/linuxkpi_/drmglue_ prefixed | renamed: `__devres_alloc` -> `lkpi_devres_alloc`, `devres_release_all` -> `lkpi_devres_release_all`, `no_llseek` -> `lkpi_no_llseek` (macro keeps the name); remaining non-header names `__kthread_should_park`, `pci_platform_rom` are Linux kernel exports |

### 10.1 IOMMU (illumos immu) and the GPU (R-K7)

`device_iommu_mapped()` now reports whether the GPU's DMA is translated
(src/linux_dma.c, `lkpi_plat_dev_iommu_mapped()` in src/plat_illumos_dev.c), and i915's
`intel_vtd_active()` follows it, so the Gen9 VT-d workarounds (256 KiB scanout alignment, guard
pages, no GGTT updates while scanning out...) are applied exactly when the DMAR unit translates.

What illumos does (usr/src/uts/i86pc/io/immu*.c, part of drv/rootnex): with `immu_enable` and
`immu_dvma_enable` (default on where the firmware has a DMAR table), every display device gets a
unity (1:1) DVMA domain (`check_gfx()`: IMMU_FLAGS_UNITY). DMA addresses then equal physical
addresses, but the hardware still walks the unity page tables, which is the case the i915
workarounds are for. The graphics-only DMAR unit is started only if `immu_gfxdvma_enable` is true
(`immu_dvma_startup()`).

Detection: translated = `immu_running && immu_dvma_enable && immu_gfxdvma_enable` (globals of
drv/rootnex; linuxkpi now has `MODDEPS ... drv/rootnex`). Override: `set
linuxkpi:linuxkpi_iommu_mapped = 0` (or `1`) in /etc/system; `-1` (default) is automatic. Not
detected: a GPU behind a shared (non graphics-only) DMAR unit with `immu_gfxdvma_enable` off is
still translated but reported as not; set the override to 1 on such a system.

Running the GPU outside IOMMU translation (any of, in order of preference):
- boot property / loader: `immu-gfxdvma-enable=false` (stops the graphics-only DMAR unit; Kaby
  Lake laptops have one for 00:02.0); in /etc/system: `set rootnex:immu_gfxdvma_enable = 0`;
- `immu-dvma-enable=false` or `immu-enable=false` (all devices; loses DMA protection everywhere).

Recommendation for first boot on the Latitude 5420 Rugged: run with `immu-gfxdvma-enable=false`,
so the GPU is not translated, `device_iommu_mapped()` is false and the simpler non-VT-d paths are
exercised first; enable it afterwards (and expect the workarounds to be applied automatically).
All of this is read from illumos-gate source; none of it has been run.
