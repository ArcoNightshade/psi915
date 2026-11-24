# psi915

A LinuxKPI (Linux kernel programming interface compatibility layer) for illumos / OpenIndiana,
with the Intel i915 DRM driver as the first consumer. Licence: BSD-2-Clause (see `LICENSE`).

Status: P0 (foundation) in progress. A skeleton `linuxkpi` misc kmod, an out-of-gate illumos build
harness, and a first slice of pure-logic KPI headers (types, list, kref) with userland unit tests.
The kernel module has NOT yet been built or loaded on illumos; see "Verification status" below and
`docs/04-roadmap.md`.

| Doc | Contents |
|-----|----------|
| docs/00-goals-and-scope.md | Goals, non-goals, target hardware, success criteria |
| docs/01-reference-sources.md | Pulled reference trees, versions, what each is used for |
| docs/02-architecture.md | Design of the illumos LinuxKPI and driver packaging |
| docs/03-kpi-gap-analysis.md | Linux API area by area: illumos mapping, difficulty, reference |
| docs/04-roadmap.md | Phased plan with exit criteria; P0 status checklist |
| docs/05-dev-environment-and-testing.md | Build, debug, test workflow |
| docs/06-risks-and-open-questions.md | Risks, licensing, decisions needed from the owner |
| docs/07-feature-scope-and-validation.md | Kernel/userspace feature list, validation ladder, work order |

## Layout
```
include/linux/     Linux KPI headers (clean-room or BSD-derived; never GPL-only Linux source)
include/linuxkpi/  project-internal headers (platform selection, LINUXKPI_VERSION)
src/               kernel C sources (src/linuxkpi_mod.c: module linkage)
build/             out-of-gate illumos kmod build (Makefile.master/.files/.rules/.targ, one dir per module)
build/install.manifest  neutral install layout used by scripts/install.sh
tests/             userland unit tests; stub illumos headers for host-side compile checks
scripts/           fetch-refs.sh (reference trees), install.sh (neutral installer)
third_party/       placeholder for pinned upstream imports (P3+)
```

## Quick start on any host (Linux or illumos userland)
```
make test         # unit tests for list/kref/kernel helpers (gcc; also clang and ASan/UBSan if present)
make check-kmod   # compile src/ against stub illumos headers and check exported symbols
```
`make check-kmod` only proves the sources type-check against hand-written stubs. It is not a kernel build.

## Building on illumos (OpenIndiana 2026.10, Tribblix 0m41.1)
Untested on illumos so far: the steps below are what the harness is designed for; please report deviations.

Prerequisites (64-bit x86 only):
- GNU make (`gmake`), gcc (the distribution's illumos-patched gcc; OpenIndiana: `developer/gcc-13` or newer,
  Tribblix: the `develop` overlay), system linker `/usr/bin/ld`, kernel headers (`system/header` on OpenIndiana;
  included in Tribblix `develop`).
- Optional: `ctfconvert`/`ctfmerge` (`developer/build/onbld` on OpenIndiana) for CTF type data, which mdb and
  DTrace use. The harness uses them automatically if found under `/opt/onbld/bin/i386` or `/usr/bin`.

Build:
```
gmake kmod                 # non-DEBUG: build/linuxkpi/obj64/linuxkpi
gmake kmod DEBUG=1         # DEBUG (ASSERTs on): build/linuxkpi/debug64/linuxkpi
```
Knobs (pass on the gmake command line, see `build/Makefile.master`):
- `CC=/usr/gcc/13/bin/gcc` to pick a compiler.
- `KERNEL_INC=/path/to/proto/root_i386/usr/include` to build against a gate proto area instead of `/usr/include`.
  Build against each target's own headers (OpenIndiana and Tribblix kernels differ).
- `ILLUMOS_GCC_FLAGS=` if the gcc lacks the illumos patches (drops `-msave-args -fno-clone-functions`).
- `LD_KMOD_TYPE=-r` for an older `ld` without `-ztype=kmod`.

Install (as root, or into a staging root for packaging):
```
gmake install                          # to /usr/kernel/misc/amd64/linuxkpi
gmake install DESTDIR=/var/tmp/proto   # staging root
scripts/install.sh -f ips              # IPS (p5m) actions for the same layout
scripts/install.sh -f svr4             # SVR4 prototype(5) lines (Tribblix)
```
`linuxkpi` is a misc module, so there is no `add_drv` step and no `.conf` file. Load test (P0 exit criterion):
```
modload /usr/kernel/misc/amd64/linuxkpi   # console: "linuxkpi 0.0.1-p0 loaded (LINUXKPI_VERSION 61400)"
modinfo | grep -i linuxkpi
modunload -i <id from modinfo>            # console: "linuxkpi 0.0.1-p0 unloaded"
```
Test in a separate boot environment (`beadm create`) with serial console or kmdb (`-k`) available.

## Verification status
| Item | Linux container | illumos |
|------|-----------------|---------|
| Unit tests (list, kref/refcount, kernel helpers) | passing (gcc, clang, ASan/UBSan) | not run |
| Kernel sources compile with kernel flags | against stub headers only | not built |
| Kmod link (`ld -ztype=kmod`), CTF, modload/modunload | n/a | not done |
| install.sh layout / ips / svr4 output | exercised with `-R` staging dir | not run |
