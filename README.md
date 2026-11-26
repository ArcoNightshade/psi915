# psi915

A LinuxKPI (Linux kernel programming interface compatibility layer) for illumos / OpenIndiana,
with the Intel i915 DRM driver as the first consumer. Licence: BSD-2-Clause (see `LICENSE`).

Status: P1 (KPI core) and P2 (hardware plumbing) are implemented and unit-tested in userland; nothing
has been built or loaded on illumos yet. The `linuxkpi` misc kmod carries atomics, locks (incl. ww_mutex),
memory, RCU/SRCU, `current`/scheduling/wait queues, workqueues/timers/kthreads, uaccess, module params,
xarray/idr, the device model and devres, the PCI/IRQ/MMIO/DMA/page layers over the illumos DDI,
firmware loading, ACPI/DMI, I2C and CPU cache helpers. `lkpi_toy` (QEMU edu device driver) is the P2 exit
artifact; `lkpi_ktest` runs the unit tests in the kernel. See "Verification status" below,
`docs/03-kpi-gap-analysis.md` (per-area status) and `docs/04-roadmap.md`.

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
include/asm/       Linux asm/ headers (barrier.h); never names illumos installs in /usr/include/asm
include/linuxkpi/  project-internal headers (platform selection and lkpi_plat_* primitives, version)
src/               KPI C sources (linux_*.c), module linkage (linuxkpi_mod.c), platform glue (plat_*.c)
build/             out-of-gate illumos kmod build (Makefile.master/.files/.rules/.targ, one dir per module)
build/install.manifest  neutral install layout used by scripts/install.sh
tests/             unit tests (userland programs and, via tests/kernel, the lkpi_ktest module);
                   stub illumos headers for host-side compile checks
scripts/           fetch-refs.sh (reference trees), install.sh (neutral installer)
third_party/       placeholder for pinned upstream imports (P3+)
```

## Quick start on any host (Linux or illumos userland)
```
make test         # 17 unit-test suites (gcc; also clang and ASan/UBSan if present)
make check-kmod   # compile src/ and the kernel test module against stub illumos headers, check symbols
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
gmake kmod                 # non-DEBUG: build/linuxkpi/obj64/linuxkpi, build/lkpi_ktest/obj64/lkpi_ktest,
                           #   build/lkpi_toy/obj64/lkpi_toy
gmake kmod DEBUG=1         # DEBUG (ASSERTs on): build/*/debug64/...
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
`linuxkpi` is a misc module, so there is no `add_drv` step and no `.conf` file. It depends on
`misc/acpica` and `misc/gfx_private` (both part of illumos). Load test (P0 exit criterion):
```
modload /usr/kernel/misc/amd64/linuxkpi   # console: "linuxkpi 0.0.2-p1 loaded (LINUXKPI_VERSION 61400)"
modinfo | grep -i linuxkpi
modunload -i <id from modinfo>            # console: "linuxkpi 0.0.2-p1 unloaded"
```
Test in a separate boot environment (`beadm create`) with serial console or kmdb (`-k`) available.

Kernel unit tests (P1 exit test; untested): on a DEBUG kernel with `set kmem_flags=0xf` in /etc/system,
after installing linuxkpi:
```
modload build/lkpi_ktest/debug64/lkpi_ktest   # log: "lkpi_ktest: 15 suites, 0 failed"; EIO if any fails
modunload -i <id>
mdb -k   # ::findleaks, ::kmem_verify
```
`lkpi_ktest` is never installed. Set `linuxkpi:linuxkpi_bug_panic = 1` in /etc/system to make Linux
`BUG()` panic (default: log and continue).

Toy PCI driver (P2 exit test; untested): in a QEMU VM started with `-device edu`, after installing linuxkpi:
```
cp build/lkpi_toy/obj64/lkpi_toy /usr/kernel/drv/amd64/
add_drv -i '"pci1234,11e8"' lkpi_toy
# console: "0000:00:..: edu 0x10000ed: live 1 irq 1 fact 3628800 (1) dma 1 pool 1, 5 interrupts"
rem_drv lkpi_toy
```

## Verification status
| Item | Linux container | illumos |
|------|-----------------|---------|
| Unit tests: list, kref/refcount, kernel helpers | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: atomics, bitops/bitmap/bitfield/log2/math64, printk/WARN/BUG/err/jiffies | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: kmalloc/kvmalloc/kmem_cache/overflow/string | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: spinlock/mutex/rwsem/rwlock/seqlock/completion/lockdep-lite (pthread shims) | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: list_sort, rbtree, RCU/rculist/kfree_rcu, xarray/idr/ida | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: sched/current/wait queues/delays, timers/workqueues/kthreads, uaccess/params/ww_mutex/semaphore/kstrto/sort/xarray marks/mempool/SRCU | passing (gcc, clang, ASan/UBSan) | not run |
| Unit tests: device model/devres, PCI over fake devices, IRQ/tasklets, DMA/sg/dma_pool over a fake IOMMU, pages/ioremap, firmware, DMI, I2C bit-bang | passing (gcc, clang, ASan/UBSan) | n/a (fake devices) |
| toy_edu driver against a simulated edu device (tests/test_toy.c) | passing (gcc, clang, ASan/UBSan) | not run (needs QEMU edu) |
| illumos hardware backend, driver glue, lkpi_toy kmod | compile against stub headers only | not built |
| Kernel sources and lkpi_ktest compile with kernel flags; undefined symbols in allowlist | against stub headers only | not built |
| lkpi_ktest in kernel under kmem_flags=0xf | n/a | not done |
| Kmod link (`ld -ztype=kmod`), CTF, modload/modunload | n/a | not done |
| install.sh layout / ips / svr4 output | exercised with `-R` staging dir | not run |
| docs/08-kpi-requirements.md | Ranked missing-API list for DRM core + Gen9 i915, GPL module boundary |
| docs/09-userland-stack.md | Userspace stack needs, distro gap list, uAPI conformance results |
| docs/10-mmap-dmabuf-design.md | devmap GEM mmap, dma-buf/PRIME fds, DRM chardev glue design |
