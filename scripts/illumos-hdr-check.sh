#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Compile every kernel module of the project on the Linux host against
# the REAL illumos kernel headers (an overlay built from an illumos-gate
# tree by scripts/illumos-hdr-overlay.sh) with the real build flags of
# build/Makefile.master and build/drm/Makefile.lkpi (-Werror included),
# minus the options only the illumos-patched gcc knows. This reproduces
# the header-interaction errors of the illumos CI build (sys/list.h,
# sys/sysinfo.h, sys/ioccom.h, ...) that the stub headers hide.
# Objects only; not a kmod link. Output: tests/out/illumos-hdr/*.log and
# a summary of the most frequent errors.
#
#   scripts/illumos-hdr-check.sh [-j N] [illumos-tree]
#
set -u
top=$(cd "$(dirname "$0")/.." && pwd)
jobs=8
[ "${1:-}" = -j ] && { jobs=$2; shift 2; }
tree=${1:-$top/../ref/illumos}
out=$top/tests/out/illumos-hdr
mkdir -p "$out"
inc=$(sh "$top/scripts/illumos-hdr-overlay.sh" "$tree") || exit 1
gccinc=$(gcc -print-file-name=include)
MAKE=${MAKE:-make}
# Host gcc lacks the illumos patch that knows __format__(cmn_err): keep
# those as warnings (other format problems are listed separately below).
common="KERNEL_INC=$inc ILLUMOS_GCC_FLAGS= HOST_ILLUMOS_HDRS=1"
# gcc 14 (the illumos CI compiler) makes these errors by default; host
# gcc 13 only warns.
gcc14="-Werror=implicit-int -Werror=implicit-function-declaration \
    -Werror=int-conversion -Werror=incompatible-pointer-types"
cc="gcc -nostdinc -isystem $gccinc -Wno-error=format $gcc14"

rc=0
for m in linuxkpi lkpi_ktest lkpi_toy; do
	rm -rf "$top/build/$m/obj64"
	"$MAKE" -C "$top/build/$m" -k -j"$jobs" $common CC="$cc" \
	    objects >"$out/$m.log" 2>&1 || rc=1
done
for m in linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915; do
	rm -rf "$top/build/$m/obj64/objs"
	"$MAKE" -C "$top/build/$m" -k -j"$jobs" $common CC="$cc" \
	    objects >"$out/$m.log" 2>&1 || rc=1
done
grep -h "warning:.*format" "$out"/*.log | grep -v "cmn_err" | sort -u \
    >"$out/format-warnings.txt"
cat "$out"/*.log | grep -E "error:" | sed -e "s/^[^ ]*\.[ch]:[0-9]*:[0-9]*: //" |
    sort | uniq -c | sort -rn >"$out/summary.txt"
for f in "$out"/*.log; do
	printf '%-20s %5d errors\n' "$(basename "$f" .log)" "$(grep -c 'error:' "$f")"
done | tee "$out/counts.txt"
echo "== format warnings other than cmn_err (errors on illumos): $(wc -l <"$out/format-warnings.txt")"
echo "== top errors"
head -40 "$out/summary.txt"
exit 0
