# Userland stack: libdrm, Mesa, Xorg, kmscube, igt on illumos

Status: planning plus scripts. Nothing in this document has been built or run on illumos. The uAPI
conformance test (`userland/tests`) builds and passes on Linux. Every claim is marked with how it was
checked:

- **[checked]** read in a source tree fetched on 2026-11-24/26 (commits listed in "Sources").
- **[run]** exercised on the Linux planning host.
- **[believed]** from memory of upstream code; not re-read in source. Verify before relying on it.
- **[unverified]** could not be checked offline at all (mostly freedesktop.org, x.org and mesa3d.org
  content, which the planning host could not reach).

Files: `userland/` (scripts, tests, config, packaging), this document. Kernel-side work it implies is
listed in "Requests to the kernel track" at the end.

## 1. Stack and validation ladder

```
 kmscube          glxgears / glxinfo -B    vkcube (later)       igt subset
 GBM+EGL+GLES2    Xorg modesetting+glamor   anv                  (needs shim)
        \               |  DRI3 + Present     |
         Mesa: iris (GL/GLES), llvmpipe (fallback), anv (Vulkan), GBM, EGL, GLX
                        |
                 libdrm (+ libdrm_intel for igt/older tools)
 ------------------ /dev/dri/card0  /dev/dri/renderD128 ----------------------
       i915 (Linux v6.14 import) + DRM core + linuxkpi (kernel track)
```

Ladder (docs/07): 1 `modetest` (libdrm test program, KMS only, no GL) then kmscube; 2 Xorg
modesetting + glamor + DRI3, `glxinfo -B` reports iris; 3 igt subset; 4 Wayland (separate track).
`modetest` is added as step 0: it needs only the KMS and dumb-buffer ioctls, no GEM execbuffer.

Install policy: everything goes to a private prefix `/opt/psi915` with RUNPATH, never over the
distribution's `libdrm`/`mesa`/`xorg-server`. Both target distributions ship old versions that other
packages depend on (section 4), and a broken experimental Mesa must not take down the desktop.

## 2. Kernel uAPI each component needs

Ioctl names are the `DRM_IOCTL_*` macros. "Gen9 path" means what runs on Kaby Lake-R; Mesa probes
newer features and falls back when the kernel reports them absent, so the lists are "used when
present", with the hard minimum marked **must**.

### 2.1 DRM core, every client
| Ioctl / mechanism | Used by | Notes |
|---|---|---|
| VERSION | libdrm `drmGetVersion`, Mesa loader (driver name "i915" picks iris/anv) | **must**. Struct has pointers and `__kernel_size_t`: 32-bit compat case |
| GET_CAP | all | **must**: DUMB_BUFFER, PRIME, SYNCOBJ, SYNCOBJ_TIMELINE, TIMESTAMP_MONOTONIC, ADDFB2_MODIFIERS, CRTC_IN_VBLANK_EVENT, CURSOR_WIDTH/HEIGHT, ASYNC_PAGE_FLIP |
| SET_CLIENT_CAP | KMS clients | UNIVERSAL_PLANES, ATOMIC (kmscube -A, modesetting atomic opt-in) |
| GET_UNIQUE, SET_VERSION | libdrm `drmOpenByBusid`, Xorg modesetting PCI probe | **must for Xorg on a non-udev host** [believed]: modesetting opens by bus id "pci:0000:00:02.0" |
| GEM_CLOSE | all | **must** |
| GEM_FLINK, GEM_OPEN | DRI2 (Xorg), old clients | legacy; DRI3 path does not need them |
| GET_MAGIC, AUTH_MAGIC | DRI2 | legacy |
| SET_MASTER, DROP_MASTER | Xorg (VT switch), kmscube (implicit master on first open) | **must** for KMS |
| PRIME_HANDLE_TO_FD, PRIME_FD_TO_HANDLE | DRI3, GBM import/export, Mesa, kmscube (gbm) | **must** for DRI3; flags carry `DRM_CLOEXEC`/`DRM_RDWR` (section 5.2) |
| read(2) on the fd | libdrm `drmHandleEvent` | **must**: `struct drm_event_vblank` (FLIP_COMPLETE, VBLANK), `drm_event_crtc_sequence` |
| poll(2) | libdrm, Xorg, kmscube | **must**: POLLIN when events are queued |
| mmap(2) at fake offsets | dumb buffers, i915 GEM (MMAP_OFFSET) | **must**; devmap path (docs/02 item 10) |

### 2.2 KMS (modetest, kmscube, Xorg modesetting, Present)
MODE_GETRESOURCES, GETCONNECTOR, GETENCODER, GETCRTC, SETCRTC, GETPLANERESOURCES, GETPLANE, SETPLANE,
GETPROPERTY, SETPROPERTY, GETPROPBLOB, CREATEPROPBLOB, DESTROYPROPBLOB, OBJ_GETPROPERTIES,
OBJ_SETPROPERTY, ADDFB, ADDFB2 (with modifiers: I915_FORMAT_MOD_X/Y_TILED, Y_TILED_CCS on Gen9),
RMFB, GETFB, GETFB2, DIRTYFB, PAGE_FLIP (with DRM_MODE_PAGE_FLIP_EVENT), CURSOR, CURSOR2, GETGAMMA,
SETGAMMA, ATOMIC, CREATE_DUMB, MAP_DUMB, DESTROY_DUMB, WAIT_VBLANK, CRTC_GET_SEQUENCE,
CRTC_QUEUE_SEQUENCE. [believed: union of libdrm xf86drmMode.c wrappers, modetest, kmscube legacy and
atomic paths, and modesetting's present/vblank code]

Present (Xorg) needs page-flip completion events and vblank sequence/timestamps on the right CRTC;
glamor needs GBM BOs with modifiers and PRIME export of those BOs to DRI3 clients.

### 2.3 i915, Gen9 path (iris, anv, libdrm_intel, igt)
| Ioctl | iris | anv | Notes |
|---|---|---|---|
| I915_GETPARAM | yes | yes | **must**: CHIPSET_ID, REVISION, HAS_EXEC_SOFTPIN, HAS_EXEC_NO_RELOC, HAS_EXEC_FENCE, HAS_EXEC_FENCE_ARRAY, HAS_EXEC_TIMELINE_FENCES, HAS_CONTEXT_ISOLATION, CS_TIMESTAMP_FREQUENCY, MMAP_GTT_VERSION, MMAP_VERSION, SUBSLICE_TOTAL, EU_TOTAL, HAS_SCHEDULER [believed]. `drm_i915_getparam.value` is a user pointer: 32-bit compat case |
| I915_QUERY | yes | yes | TOPOLOGY_INFO, ENGINE_INFO (MEMORY_REGIONS, HWCONFIG_BLOB, GUC_SUBMISSION_VERSION ignored or optional on Gen9) |
| I915_GEM_CREATE / GEM_CREATE_EXT | yes | yes | |
| I915_GEM_MMAP_OFFSET | yes | yes | Linux 5.6+ [believed]; Mesa falls back to GEM_MMAP (v2, WC flag) and GEM_MMAP_GTT. All three use the devmap path |
| I915_GEM_SET_DOMAIN, SET_CACHING/GET_CACHING, SET_TILING/GET_TILING | yes | yes | tiling ioctls for importing legacy-tiled BOs |
| I915_GEM_MADVISE, BUSY, WAIT | yes | yes | WAIT returns `ETIME` on timeout (errno, section 5.3) |
| I915_GEM_EXECBUFFER2 and _WR | yes | yes | **must**. Softpin + NO_RELOC + HANDLE_LUT + BATCH_FIRST on Gen9; out-fence via _WR; in/out syncobj via I915_EXEC_FENCE_ARRAY or the timeline-fences extension (I915_EXEC_USE_EXTENSIONS) |
| I915_GEM_CONTEXT_CREATE_EXT, DESTROY, GETPARAM, SETPARAM | yes | yes | RECOVERABLE=false (iris), PRIORITY, VM, ENGINES (anv), GTT_SIZE, BANNABLE |
| I915_GEM_VM_CREATE / VM_DESTROY | | yes | per-context ppGTT (full 48-bit ppGTT on Gen9) |
| I915_GET_RESET_STATS | yes | yes | robustness, hang detection reporting (docs/07 makes hang reset mandatory) |
| I915_REG_READ | yes | yes | TIMESTAMP register (GL timer queries, VK timestamps) |
| I915_GEM_USERPTR | some | yes | anv external host memory, igt; needs pinning of user pages (hard on illumos: `as_pagelock`) |
| I915_PERF_*, GEM_SW_FINISH, GEM_PREAD/PWRITE, GEM_THROTTLE, GEM_GET_APERTURE | | | libdrm_intel, igt, perf tools; PREAD/PWRITE also by older Mesa paths |

anv additionally needs timeline syncobjs (Vulkan 1.2 timeline semaphores) [believed: anv requires
`DRM_CAP_SYNCOBJ_TIMELINE` or I915_PARAM_HAS_EXEC_TIMELINE_FENCES in current Mesa; check the minimum
in the pinned Mesa's `anv_physical_device` init].

### 2.4 Sync: syncobj, sync_file, dma-buf
| Ioctl | Users |
|---|---|
| SYNCOBJ_CREATE, DESTROY, HANDLE_TO_FD, FD_TO_HANDLE (incl. sync_file import/export flags) | iris, anv, Xorg (DRI3 1.3+ explicit sync with syncobj, believed xserver 21.1.x has it only partially) |
| SYNCOBJ_WAIT, RESET, SIGNAL | iris, anv |
| SYNCOBJ_TIMELINE_WAIT, QUERY, TRANSFER, TIMELINE_SIGNAL, EVENTFD | anv (EVENTFD: newer Mesa/compositors; optional) |
| SYNC_IOC_MERGE, SYNC_IOC_FILE_INFO ('>' 3, 4) on sync_file fds; poll(2) | Mesa util/libsync.h, EGL_ANDROID_native_fence_sync, anv sync_fd semaphores |
| DMA_BUF_IOCTL_SYNC ('b' 0) | CPU access to imported BOs (Mesa, gbm_bo_map on imports) |
| DMA_BUF_IOCTL_EXPORT_SYNC_FILE / IMPORT_SYNC_FILE ('b' 2, 3) | Mesa implicit-sync bridging for Vulkan WSI (optional; Linux 6.0+; Mesa probes, believed) |
| lseek(fd, 0, SEEK_END) on dma-buf fds | buffer size discovery (Mesa, GBM, Xorg glamor) |
| mmap(2) of dma-buf fds | software paths, gbm_bo_map |
| fd passing over AF_UNIX (SCM_RIGHTS) | DRI3 (X server <-> client), works for any vnode on illumos [believed] |

### 2.5 Non-ioctl expectations
- `fstat(fd).st_rdev` major/minor identify the node type and index (libdrm, Mesa loader, compositors).
- `/dev/dri/cardN` and `/dev/dri/renderD(128+N)` names (libdrm scans these paths).
- Linux discovers PCI identity via `/sys/dev/char/M:m/device`; non-Linux libdrm needs its own path.
- Errno values are compared symbolically (`ETIME`, `EIO`, `ENOSPC`, `EINTR`, `EAGAIN`, `ENODEV`).

## 3. Kernel-side source of truth for the uAPI

The kernel track imports upstream Linux v6.14 (`drm/include/uapi/drm/{drm,drm_mode,drm_fourcc,
drm_sarea,i915_drm}.h` [checked]). These headers are unmodified upstream: their non-Linux branch
includes `<sys/ioccom.h>` like libdrm's copy. The import does not include
`include/uapi/linux/sync_file.h` or `include/uapi/linux/dma-buf.h`; the conformance test borrows them
from the drm-kmod reference tree until the kernel track imports them (they are
GPL-2.0 WITH Linux-syscall-note, header-only uAPI).

## 4. Package versions on the targets

| Component | OpenIndiana (oi-userland e6b900913634, 2026-11-24) | Tribblix (tribblix/build 40b0bbecfe7e, 2026-11-24) | psi915 pin (`userland/versions.conf`) |
|---|---|---|---|
| libdrm | 2.4.75 via `components/openindiana/gfx-drm` (builds illumos/gfx-drm; LIBDRM_VERS=2.4.75 in its `common/libdrm/Makefile`) [checked] | not in tribblix/build; `TRIBsys-header-drm` referenced by the Mesa recipe, so it comes from a gfx-drm build too [checked reference, origin unverified] | 2.4.124 [unverified that it exists] |
| Mesa | 21.3.9, meson, `-Dgallium-drivers=swrast,i915,r600 -Ddri-drivers=i965 -Dllvm=false -Dplatforms=x11 -Dvulkan-drivers=''` [checked] | 13.0.6, autotools, i915/i965/swrast DRI, `--disable-dri3`, no gallium-llvm [checked] | 24.0.9 [unverified] |
| xorg-server | 1.19.7, `--enable-dri3 --disable-glamor`, 40 Solaris patches [checked] | 1.16.4, `--disable-present` [checked] | 21.1.13 [unverified] |
| xf86-video-modesetting | inside xorg-server (since 1.17); not separately packaged | not available (1.16.4 predates it in-tree) | inside xorg-server |
| xf86-video-intel | 2.99.917, DRI3 disabled [checked] | 2.99.917 patch set present [checked] | not used |
| kmscube | not packaged [checked: no component] | not packaged [checked] | git master |
| igt-gpu-tools | `intel-gpu-tools` 1.3 (2012) [checked] | not packaged | 1.29, design only |
| LLVM | clang/LLVM 16, 18, 19, 20, 21, 23 components; default 21.1.8 in `/usr/clang/21` [checked] | TRIBllvm 22.1.7 in `/usr/versions/llvm` [checked] | system |
| meson | 1.7.2 [checked] | `TRIBmeson-python-313` (version not read) | system |
| libepoxy | 1.5.10 [checked] | 1.5.10 [checked] | system |
| libglvnd | 1.4.0 [checked] | not checked | not used (`-Dglvnd=disabled`) |
| libxshmfence | 1.3.3 [checked] | not checked | system |
| glslang, spirv-tools/headers | components exist [checked] | not checked | system (anv build) |
| wayland | no component [checked] | not checked | out of scope |

What actually ships in the OpenIndiana 2026.10 and Tribblix 0m41.1 repositories was not checked:
these are recipe trees, not package catalogs [unverified].

Conclusions: neither distribution has a Mesa with iris or anv, an Xorg with glamor, or a libdrm newer
than 2017. The whole stack has to be built (hence `/opt/psi915`). LLVM is new enough on both.

## 5. illumos gap list

### 5.1 ioctl encoding
- Linux (x86, asm-generic): `dir<<30 | size<<16 (14 bits) | type<<8 | nr`, dir 1 = write
  (user to kernel), 2 = read.
- illumos `<sys/ioccom.h>` [checked]: `IOC_VOID 0x20000000`, `IOC_OUT 0x40000000` (kernel to user),
  `IOC_IN 0x80000000` (user to kernel), size masked with `IOCPARM_MASK 0xff`.
- The direction bits are swapped relative to Linux, `_IO` sets a bit Linux does not, and sizes of
  256 bytes or more are truncated. `_IOWR` with a size under 256 is identical in both.
- libdrm's `drm.h` and Mesa's `include/drm-uapi/drm.h` take the non-Linux branch on illumos and use
  `<sys/ioccom.h>` [checked for libdrm 2.4.75 via gfx-drm patches; believed for current libdrm and
  Mesa]. Patching every consumer to the Linux layout is not practical (libdrm, Mesa, igt, xserver and
  any third-party client each carry copies of the headers).

**Decision proposed: userland keeps the native illumos encoding; the kernel ioctl glue decodes it.**
The contract (implemented and checked by `decode_illumos_ok()` in `userland/tests/uapi_dump.c`):
nr = `cmd & 0xff`, type = `(cmd >> 8) & 0xff`, copy-in iff `cmd & IOC_IN`, copy-out iff
`cmd & IOC_OUT`, user size = `(cmd >> 16) & 0xff`; when the kernel's own size is 256 or more, take the
table size if `(ksize & 0xff) == usize`. Linux `drm_ioctl()` already looks the ioctl up by nr and
uses `_IOC_SIZE(cmd)` only for zero-extension of older/smaller structs, so the KPI's `_IOC_SIZE`,
`_IOC_NR`, `IOC_IN`, `IOC_OUT` for decoding user commands must follow the illumos layout, while the
driver's own ioctl tables may keep Linux-layout constants as long as decoding never compares full
command words. The same applies to sync_file ('>') and dma-buf ('b') ioctls on their fds.
gfx-drm did the same (decoded by nr, size from its table) [checked: gfx-drm `drm_drv.c drm_ioctl`].

Test results [run, Linux host, v6.14 headers vs Ubuntu 24.04 kernel headers and vs a simulated
illumos user build]: 182 ioctls; 70 have different numbers under the two encodings; none of the
DRM/i915/sync_file/dma-buf argument structs is 256 bytes or larger (largest is legacy GET_STATS at
248; the largest Gen9-relevant ones are 104: MODE_GETCRTC/SETCRTC/ADDFB2/GETFB2). So truncation does
not bite today, and the test flags `TRUNC_ILLUMOS` if a future uAPI struct crosses 255 bytes.

### 5.2 open(2) flag values inside ioctls
`DRM_CLOEXEC` is defined as `O_CLOEXEC` and `DRM_RDWR` as `O_RDWR`. illumos `O_CLOEXEC` is 0x800000,
Linux x86 0x80000; `O_NONBLOCK` is 0x80 vs 0x800 [checked: illumos sys/fcntl.h]. PRIME_HANDLE_TO_FD
and SYNCOBJ_HANDLE_TO_FD flags therefore arrive with illumos values; the KPI must define these
constants with illumos values (or translate at the ioctl boundary), never hard-code Linux numbers.
`uapi_cmp.awk` reports this mismatch when run on illumos.

### 5.3 errno values
Linux and illumos agree on the low numbers (EPERM..ERANGE, so EIO, ENOMEM, EBUSY, EINVAL, ENOSPC,
EAGAIN) but not on others: EDEADLK 35/45, ECANCELED 125/47, ETIMEDOUT 110/145, EOVERFLOW 75/79,
EOPNOTSUPP 95/122, ENOTSUP (= EOPNOTSUPP on Linux) vs 48 [checked: illumos sys/errno.h; Linux values
believed]. `ETIME` (62) and `EPROTO` (71) happen to match. Kernel-internal Linux codes
(ERESTARTSYS 512, ENOTSUPP 524, EPROBE_DEFER) must never reach userland. The KPI must use illumos
`<sys/errno.h>` values symbolically; userland compares symbolically, so no userland change.

### 5.4 Per-open state, clone minors and st_rdev
illumos character drivers get `dev_t`, not a file pointer, in `cb_ioctl`/`cb_read`/`cb_chpoll`, so
per-open state (`struct drm_file`) needs clone opens: `cb_open` rewrites `*devp` to a unique minor.
gfx-drm encoded the clone id in minor bits above the node index and patched libdrm to mask them
(`d &= ~0x3fe00`) [checked]. Consequences for userland:
- `fstat(fd).st_rdev` differs from `stat("/dev/dri/card0").st_rdev`; libdrm (`drmGetMinorType`,
  `drmGetDeviceNameFromFd2`, `drmGetNodeTypeFromFd`), the Mesa loader and compositors compare these.
- Proposed layout: minor = `(clone_id << 8) | node_minor`, node_minor 0..63 card, 128..191 render
  (Linux numbering, which libdrm derives the node type from). libdrm then masks with `& 0xff`
  (patches/libdrm/series 0002). A wider clone field is possible: illumos minors are 32 bits on
  amd64 (`MAXMIN32`/`L_MAXMIN`, believed).
- Alternative without clones: none known in the DDI for character devices (no per-open cookie); a
  STREAMS or `ldi`-based scheme is heavier. Kernel track decides.

### 5.5 Device nodes, naming, links, major number
- Minor nodes: `ddi_create_minor_node(dip, "card0", S_IFCHR, 0, "ddi_display:drm", 0)` and
  `"renderD128"` with minor 128 (node type as gfx-drm) [design].
- `/dev/dri` links. Option A: one `devlink.tab` line `type=ddi_display:drm<TAB>dri/\M0`
  (`userland/config/devlink.tab.psi915`; escape semantics believed, verify). Option B: a devfsadm
  link module, like gfx-drm's `SUNW_drm_link_i386.so`, which already matches `^dri$/^([a-z]+D?)([0-9]+)$`
  [checked]. B is what packages should ship (no system file edits); A is fine for manual bring-up.
- Major number: illumos assigns per driver in `/etc/name_to_major`; libdrm's fixed `DRM_MAJOR` test
  must go (patches/libdrm/series 0001). Identify DRM nodes by minor node type through libdevinfo.
- Permissions (`userland/config/minor_perm.psi915`): `renderD*` 0666, `card*` 0600 root sys, plus
  `logindevperm` (`userland/config/logindevperm.psi915`) to hand `card*` to the console user. This is
  the illumos substitute for logind/udev seat ACLs.

### 5.6 udev replacement and device discovery
- libdrm `drmGetDevice2`/`drmGetDevices2` read sysfs on Linux and use sysctl/pciconf on BSDs. illumos
  needs a libdevinfo walk (gfx-drm's `sun_devinfo.c` is the precedent [checked]): find nodes with
  minor type `ddi_display:drm`, read `vendor-id`, `device-id`, `subsystem-*`, `revision-id`, and
  the `reg` property for bus/dev/func.
- Mesa's loader uses libdrm for this (no libudev) [believed]. Xorg on a non-udev build has no
  platform bus; modesetting falls back to libpciaccess PCI probing and `drmOpen` by bus id
  [believed]; libpciaccess works on illumos (packaged; Tribblix carries a DINFOCPYALL patch [checked]).
- Hotplug: none needed for an iGPU. Connector hotplug arrives as DRM events/uevents on Linux;
  modesetting re-probes on RandR queries anyway. igt is the only consumer that insists on udev.

### 5.7 dma-buf and sync_file file descriptors
Both are anonymous files on Linux. illumos has no anon-inode facility; the kernel needs a pseudo
filesystem or a vnode-backed driver node that provides: `close` releasing the reference, `poll`
(POLLIN on signal for sync_file), `ioctl` (sections 2.4, with the illumos encoding), `lseek` to
SEEK_END returning the size (dma-buf), `mmap` (dma-buf), and `fstat`. Passing over AF_UNIX works for
any vnode [believed]. This is the largest kernel-side item the userland depends on after GEM mmap
(docs/07 already lists it). Without it: no DRI3, no PRIME, no EGL native fences, no anv sync_fd.

### 5.8 memfd, eventfd, epoll, timerfd
illumos libc mapfile (illumos-gate master, fetched 2026-11-26) [checked] exports `eventfd`
(ILLUMOS_0.13), `epoll_create` & co., `timerfd_create`, `signalfd` (ILLUMOS_0.18), `pipe2`, `dup3`,
`mkostemp`, `getrandom`, `close_range`, but **no `memfd_create`**, and `sys/mman.h` has no
`MFD_*`. Mesa's anonymous-file helper falls back to `mkostemp` in `$XDG_RUNTIME_DIR`/tmp when
memfd is missing [believed]; verify when building anv (patches/mesa/series 0005). eventfd is only
needed for SYNCOBJ_EVENTFD (optional). Wayland (out of scope) is the heavy epoll user.

### 5.9 32-bit processes
The amd64 illumos kernel runs 32-bit processes. Structs with pointers, `long` or `size_t`
(drm_version, drm_unique, drm_i915_getparam, drm_wait_vblank, legacy map/buffer ioctls), and `__u64`
members whose i386 alignment is 4 change layout. Linux handles them in `drm_ioc32.c`/`i915_ioc32.c`;
illumos does it per ioctl with `ddi_model_convert_from()` (docs/08: COMPAT off). Proposal: refuse
32-bit callers (`EINVAL` with a log line) until there is a 32-bit Mesa, which nothing in scope
needs. `make check32` in `userland/tests` lists every layout difference when a working `-m32`
toolchain exists (it does not on the planning host; it should on illumos).

### 5.10 Console handoff and driver binding
The iGPU (class 0x030000) is bound today by `vgatext` (or the illumos framebuffer console). A
device-ID alias for i915 (`userland/config/driver_aliases.gen9`, 93 Gen9 IDs incl. pci8086,5917) is
more specific than the class match, so i915 wins at boot and vgatext no longer attaches. gfx-drm's
i915 therefore exported the vgatext/console interfaces itself [checked: `DRM_MINOR_IS_VGATEXT` in
drm_sunmod.c]. The kernel track must provide console services (tem/gfx_private) or the console goes
dark when i915 attaches. Packaging uses `add_drv -n` so binding only changes at the next boot.

### 5.11 Driver name clash with gfx-drm
OpenIndiana's `driver/graphics/drm` (gfx-drm) registers `driver name=i915` with Gen2..Gen7.5 IDs
(up to Haswell/0xa0x/0xd0x; no Gen9 IDs) and ships a `ddi_display:drm` link module [checked]. Two
drivers cannot share a name. The IPS skeleton excludes `driver/graphics/drm`; the alternative is
renaming our module (e.g. `i915lkpi`), which changes `minor_perm`, aliases and link-module names.
**Open decision for the owner.**

### 5.12 VT switching and DRM master
Xorg must drop master on VT switch away and re-acquire it on return; with no logind, Xorg running as
root does this itself (OI patch 19-vt.patch is the illumos VT code [checked: exists]). kmscube run
from a text VT becomes master implicitly; the console (tem) must not draw while a KMS client is
master.

### 5.13 Build-level items
- TLS: OI built Mesa 21.3.9 with `-Delf-tls=false` and a TLS patch [checked]; current Mesa requires
  initial-exec TLS [believed]. illumos supports `__thread`/IE TLS in shared objects loaded at startup;
  `dlopen`ed DRI drivers using IE TLS can fail if the static TLS reserve is exhausted [believed].
  Watch for this first when GLX loads `iris_dri.so`.
- `-znolazyload` was needed for OI's Mesa (init cycles) [checked: Makefile]; keep in mind if loading
  fails.
- Mesa newer than the pin may require mesa_clc (clang + libclc + SPIRV-LLVM-Translator) for Intel
  internal shaders [believed]; neither distribution is known to package libclc. Pin stays at 24.0.x
  until checked (`userland/components/mesa.sh`).
- anv needs `glslangValidator` at build time [believed]; both distributions have glslang recipes.

### 5.14 igt
igt's library hard-requires libudev, libkmod, libprocps/libproc2 and libdw, and its tests read
debugfs/sysfs and load/unload modules [believed]. Plan: a compat shim and an allowlist of tests
(`userland/patches/igt-gpu-tools/series`). The kernel's lack of debugfs/sysfs (docs/02 item 13)
means many tests skip; gem_exec_*, prime_self_import, syncobj_* and kms_flip/kms_atomic should work
with the shim. Expect real porting work; it is last on the ladder for a reason.

## 6. The uAPI conformance test (`userland/tests`)

| File | Role |
|---|---|
| `gen-uapi-list.sh` | extracts every `DRM_IOCTL_*`, `SYNC_IOC_*`, `DMA_BUF_*` ioctl (name, direction, type, nr expression, argument type) and ABI constants (caps, params, flags) from the given headers into `uapi_list.h`, each entry `#ifdef`-guarded |
| `uapi_dump.c` | built twice. Kernel side: `-D__KERNEL__` with `compat/kpi` (Linux-layout `asm/ioctl.h`) and the imported v6.14 headers. User side: the headers libdrm installs (`/usr/include/drm` on Linux, `/opt/psi915/include/libdrm` on illumos). Prints ioctl records (nr, dir, size, header value, Linux and illumos encodings), struct sizes and field offsets for ~70 key structs (execbuffer2, exec_object2, syncobj, events, KMS, sync_file, dma-buf) and constants; self-checks that header values match the host's encoding and that the illumos decode contract round-trips |
| `uapi_cmp.awk` | compares two dumps: FAIL on nr/dir/offset/constant mismatch, WARN on size differences or userland-only ioctls, INFO on kernel-only; `mode=abi` compares LP64 against ILP32 |
| `Makefile` | `make check` (kernel vs user), `make check-sim-illumos` (Linux host, user side built with illumos `<sys/ioccom.h>` from the reference tree and `__sun`), `make check32`, `make regen` |

Results on the planning host [run]: `make check` and `make check-sim-illumos` pass with 0 FAIL;
3 WARN for `DMA_BUF_SET_NAME*`, absent from the borrowed BSD dma-buf.h (goes away once the v6.14
`uapi/linux/dma-buf.h` is imported); 5 INFO for uAPI newer than Ubuntu 24.04's headers
(SET_CLIENT_NAME, CONTEXT_FREQ_HINT, LOW_LATENCY, CONTEXT_IMAGE, GUC_SUBMISSION_VERSION).
A deliberately corrupted dump (changed nr and offset) produces FAIL lines and exit status 1.

On illumos, after building libdrm into /opt/psi915:
```
cd userland/tests
gmake check REF=/path/to/ref UHDR_EXTRA=/path/to/mesa/include/drm-uapi
gmake check32 REF=/path/to/ref
```
Expect a WARN for O_CLOEXEC (section 5.2) and identical nr/dir/size/offsets everywhere else.

## 7. Build scripts (`userland/`)

```
userland/build.sh --distro oi deps          # prints the pkg install line
userland/build.sh                           # libdrm mesa kmscube into /opt/psi915
userland/build.sh all                       # + xorg-server mesa-demos
userland/build.sh igt-gpu-tools             # expected to fail until the shim exists
userland/build.sh --destdir /var/tmp/proto all   # staging for packages
```
- `versions.conf` pins (sha256 "-" = unpinned; the script records digests it sees).
- `env/{oi,tribblix,linux}.sh`: toolchain paths, PKG_CONFIG_PATH, prerequisite package lists.
- `meson/native-oi.ini`, `meson/native-tribblix.ini`: compilers, llvm-config, RUNPATH, compat include.
  `meson/cross-x86_64-illumos.ini`: Linux to illumos with a sysroot (least verified piece).
  All three parse with meson 1.12 [run]; compilers were not available here.
- `components/*.sh`: meson options per component; options a tree lacks are skipped with a warning.
- `patches/<component>/series`: quilt-format series, currently only planned entries (commented out).
- `compat/include/linux/{types,ioctl}.h`: shims for Linux uAPI header copies; `<linux/ioctl.h>` maps
  to `<sys/ioccom.h>` on purpose (section 5.1).

Status: shell syntax checked with dash and bash [run]; no component has been built anywhere.

## 8. Configuration and packaging (design)

- `userland/config/driver_aliases.gen9` + `gen-driver-aliases.sh` + `Makefile`: Gen9 IDs (SKL, BXT,
  GLK, KBL, AML, CFL, WHL, CML) from `i915/include/drm/intel/pciids.h`; formats for driver_aliases,
  `add_drv -i` and IPS `alias=`.
- `minor_perm.psi915`, `logindevperm.psi915`, `devlink.tab.psi915`, `xorg.conf.d/20-modesetting.conf`.
- `userland/pkg/ips/driver-graphics-psi915.p5m`: kernel modules in /usr/kernel (matching
  build/install.manifest), `driver` action with perms and generated aliases, link module, firmware dir
  (/usr/lib/firmware, docs/02), exclude on gfx-drm. `psi915-userland.p5m`: /opt/psi915 stack,
  files generated from a staged install.
- `userland/pkg/svr4/TRIBpsi915-drm` (pkginfo, depend, prototype, postinstall with `add_drv -n`,
  preremove with `rem_drv`) and `TRIBpsi915-userland` for Tribblix/zap.

## 9. Requests to the kernel track

1. Decode user ioctl commands with the illumos layout (section 5.1 contract); same for sync_file and
   dma-buf fds.
2. Use illumos values for O_CLOEXEC/O_RDWR/O_NONBLOCK and errno (5.2, 5.3).
3. Minor numbering: card 0..63, render 128..191 in the low 8 bits; clone id above (5.4).
4. Minor node type `ddi_display:drm`, names `card%d`, `renderD%d`; a link module or devlink.tab (5.5).
5. Anonymous-file vnodes for dma-buf and sync_file with poll/ioctl/lseek/mmap (5.7).
6. Support SET_VERSION and GET_UNIQUE ("pci:DDDD:BB:DD.F") for Xorg's bus-id open (2.1).
7. Console services when i915 takes the device from vgatext (5.10).
8. Refuse 32-bit callers initially (5.9).
9. Import `include/uapi/linux/{sync_file,dma-buf}.h` (v6.14) so the test stops borrowing them (3).
10. Decide the driver name versus gfx-drm (5.11).

## Sources
- illumos-gate reference `usr/src/uts` f71cb08e8c53 (ioccom.h, fcntl.h, errno.h, eventfd.h);
  illumos-gate master `usr/src/lib/libc/port/mapfile-vers`, `uts/common/sys/mman.h` (2026-11-26).
- illumos/gfx-drm 19482684dc82 (libdrm 2.4.75 patches, devfsadm drm_link, drm_drv.c, manifests).
- OpenIndiana/oi-userland e6b900913634 (2026-11-24): mesa, xorg-server, gfx-drm, intel-gpu-tools,
  libepoxy, libglvnd, meson, clang-21, xf86-video-intel, libxshmfence; make-rules/shared-macros.mk.
- tribblix/build 40b0bbecfe7e (2026-11-24): TRIBx11-lib-mesa, TRIBllvm, TRIBx11-server-xorg,
  TRIBx11-libepoxy, patches/.
- Linux v6.14 uAPI and pciids.h as imported in `drm/` and `i915/`; drm-kmod 98be24dc9994 for
  sync_file.h and dma-buf.h; Ubuntu 24.04 linux-libc-dev headers as the Linux userland side.
- Not reachable from the planning host: gitlab.freedesktop.org (libdrm, Mesa, kmscube, igt), x.org,
  mesa3d.org; no GitHub mirror of libdrm or Mesa was reachable anonymously.
