#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# build.sh: build the psi915 userland stack (libdrm, Mesa, kmscube, Xorg,
# mesa-demos, igt) from pinned upstream tarballs into a private prefix.
#
#   ./build.sh [options] [component ...]     default: libdrm mesa kmscube
#   ./build.sh deps                          print the prerequisite install command
#   ./build.sh list                          list components and pins
#
# Options:
#   --prefix DIR     install prefix (default /opt/psi915)
#   --work DIR       downloads, sources, build trees (default ./_work)
#   --distro NAME    oi | tribblix | linux (default: detected)
#   --cross FILE     meson cross file (Linux host -> illumos; needs PSI_SYSROOT)
#   --destdir DIR    stage installs into DIR (for packaging, see pkg/)
#   -j N             ninja jobs
#
# Components: libdrm mesa kmscube xorg-server mesa-demos igt-gpu-tools, or
# "all" (= all but igt-gpu-tools).
#
# STATUS: UNTESTED ON ILLUMOS. Written against the meson options of the
# pinned versions as remembered offline; options that a given tree lacks are
# skipped with a warning (see lib/common.sh: mopt).

set -e
PSI_USERLAND=$(cd "$(dirname "$0")" && pwd)
. "$PSI_USERLAND/lib/common.sh"

PREFIX=/opt/psi915
WORK="$PSI_USERLAND/_work"
PSI_DISTRO=
PSI_CROSS_FILE=
DESTDIR=
NINJA_JOBS=
PSI_LIBDIR=lib		# 64-bit only stack; no amd64 subdir inside $PREFIX

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix) PREFIX=$2; shift 2 ;;
	--work) WORK=$2; shift 2 ;;
	--distro) PSI_DISTRO=$2; shift 2 ;;
	--cross) PSI_CROSS_FILE=$2; shift 2 ;;
	--destdir) DESTDIR=$2; shift 2 ;;
	-j) NINJA_JOBS="-j $2"; shift 2 ;;
	-h|--help) sed -n '6,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	--) shift; break ;;
	-*) die "unknown option $1" ;;
	*) break ;;
	esac
done

DISTRO=$(detect_distro)
case "$DISTRO" in
oi|tribblix|linux) . "$PSI_USERLAND/env/$DISTRO.sh" ;;
*) die "unsupported host/distro '$DISTRO' (use --distro oi|tribblix|linux)" ;;
esac

case "${1:-}" in
deps)
	echo "$PSI_DEPS_CMD $(echo $PSI_DEPS)"
	exit 0 ;;
list)
	grep -v '^#' "$PSI_USERLAND/versions.conf" | awk 'NF >= 5 { printf("%-14s %-9s %s\n", $1, $2, ($5 == "-") ? "UNPINNED" : "pinned") }'
	exit 0 ;;
esac

if [ -n "$PSI_CROSS_FILE" ]; then
	MESON_MACHINE_ARGS="--cross-file $PSI_CROSS_FILE"
elif [ -n "$PSI_NATIVE_FILE" ]; then
	MESON_MACHINE_ARGS="--native-file $PSI_NATIVE_FILE"
else
	MESON_MACHINE_ARGS=
fi
export PREFIX WORK DESTDIR PSI_LIBDIR

for t in meson ninja pkg-config xz; do
	have "$t" || die "missing $t; run: ./build.sh deps"
done
[ -n "$TAR" ] || die "missing tar"
[ -n "$PATCH" ] || die "missing patch"
mkdir -p "$WORK"

for f in "$PSI_USERLAND"/components/*.sh; do . "$f"; done

[ $# -gt 0 ] || set -- libdrm mesa kmscube
[ "$1" = all ] && set -- libdrm mesa kmscube xorg-server mesa-demos

msg "distro=$DISTRO prefix=$PREFIX work=$WORK ${DESTDIR:+destdir=$DESTDIR}"

# <linux/types.h>, <linux/ioctl.h> shims for uAPI header copies (Mesa, igt).
# The native files expect them in /opt/psi915/include/psi915-compat.
if [ "$(uname -s)" = SunOS ] || [ -n "$PSI_CROSS_FILE" ]; then
	cdir="${PSI_SYSROOT:-}$PREFIX/include/psi915-compat/linux"
	mkdir -p "$cdir" || die "cannot create $cdir"
	cp "$PSI_USERLAND"/compat/include/linux/*.h "$cdir/"
	[ "$PREFIX" = /opt/psi915 ] || warn "prefix is not /opt/psi915: edit the -I and -R paths in $PSI_NATIVE_FILE$PSI_CROSS_FILE"
fi
for c in "$@"; do
	fn=build_$(echo "$c" | tr '-' '_')
	command -v "$fn" >/dev/null 2>&1 || die "unknown component $c"
	msg "=== $c ==="
	"$fn"
done
msg "done. Runtime: binaries carry RUNPATH $PREFIX/$PSI_LIBDIR (see meson/*.ini)."
[ -f "$WORK/digests.txt" ] && msg "unpinned digests seen: $WORK/digests.txt"
exit 0
