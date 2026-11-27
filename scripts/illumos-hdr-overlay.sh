#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Build an include directory laid out like an installed illumos
# /usr/include from an illumos-gate tree (default ../ref/illumos), for
# compiling the kernel modules on a Linux host against the real kernel
# headers (scripts/illumos-hdr-check.sh). Approximation of what the
# system/header package installs:
#   sys/  uts/common/sys + uts/intel/sys (not uts/intel/sys/acpi, which
#         OmniOS does not install; not uts/i86pc/sys, which goes to
#         /usr/platform/i86pc/include and is not on our include path)
#   vm/   uts/common/vm
#   rpc/, net/, netinet/, inet/ from uts/common (some kernel headers
#         include them)
# Userland-only headers (usr/src/head) are not needed with -D_KERNEL.
#
#   scripts/illumos-hdr-overlay.sh [illumos-tree] [out-dir]
#
set -eu
top=$(cd "$(dirname "$0")/.." && pwd)
src=${1:-$top/../ref/illumos}
out=${2:-$top/tests/out/illumos-inc}
uts=$src/usr/src/uts
[ -d "$uts/common/sys" ] || { echo "no illumos tree at $src" >&2; exit 1; }
rm -rf "$out"
mkdir -p "$out"
link_tree() {	# from to [exclude-subdir]
	(cd "$1" && find . -name '*.h' ! -path "./$3/*" -print) | while read -r f; do
		mkdir -p "$2/$(dirname "$f")"
		ln -sf "$1/${f#./}" "$2/${f#./}"
	done
}
link_tree "$uts/common/sys" "$out/sys" __none__
link_tree "$uts/intel/sys" "$out/sys" acpi
for d in vm rpc net netinet inet; do
	[ -d "$uts/common/$d" ] && link_tree "$uts/common/$d" "$out/$d" __none__
done
# ISA headers some sys/ headers include as <ia32/sys/...>/<amd64/sys/...>
for d in ia32 amd64; do
	[ -d "$uts/intel/$d" ] && link_tree "$uts/intel/$d" "$out/$d" __none__
done
echo "$out"
