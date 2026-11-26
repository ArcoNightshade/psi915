# userland

Userspace track for psi915: build scripts for libdrm, Mesa (iris, anv, llvmpipe), kmscube, Xorg
modesetting + glamor, mesa-demos and igt into `/opt/psi915`; the uAPI conformance test; driver
configuration and package skeletons. Design, gaps and verification state: `docs/09-userland-stack.md`.

Everything here is UNTESTED ON ILLUMOS. Only `tests/` has been run (Linux host).

```
tests/      uapi_dump + uapi_cmp.awk: gmake check | check-sim-illumos | check32 | regen
build.sh    ./build.sh [--distro oi|tribblix] [component ...]; ./build.sh deps; ./build.sh list
versions.conf  pinned sources (digests not yet filled in)
env/ meson/ components/ patches/ compat/   build machinery
config/     driver_aliases (Gen9), minor_perm, devlink.tab, logindevperm, xorg.conf
pkg/        IPS (p5m) and SVR4 (Tribblix) skeletons
```
