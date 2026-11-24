#!/bin/sh
# Fetch reference sources (sparse, shallow) into ${REF:-$HOME/ref}. Reference only; not part of the build.
set -e
REF=${REF:-$HOME/ref}; mkdir -p "$REF"; cd "$REF"; export GIT_LFS_SKIP_SMUDGE=1
clone() { u=$1; d=$2; shift 2
  [ -d "$d/.git" ] || git clone --depth 1 --filter=blob:none --sparse "$u" "$d"
  git -C "$d" sparse-checkout set "$@"; }
clone https://github.com/illumos/illumos-gate  illumos   usr/src/uts usr/src/man/man4d usr/src/man/man9e usr/src/man/man9f
clone https://github.com/illumos/gfx-drm       gfx-drm
clone https://github.com/openbsd/src           openbsd   sys/dev/pci/drm sys/dev/rasops sys/dev/wscons
clone https://github.com/freebsd/freebsd-src   freebsd   sys/compat/linuxkpi
clone https://github.com/freebsd/drm-kmod      drm-kmod
clone https://github.com/NetBSD/src            netbsd    sys/external/bsd/drm2 sys/external/bsd/common
