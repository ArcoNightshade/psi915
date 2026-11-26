#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Run a make target in every kernel module directory of the graphics stack,
# in dependency order: linuxkpi, linuxkpi_gpl_g1, drm (linuxkpi_drm),
# linuxkpi_gpl_g2, i915. build/Makefile's SUBDIRS do not list the DRM
# modules yet (docs/11 section 7, R-B1); until they do, use this.
#
#   scripts/build-gfx.sh [target] [make variables...]
#     scripts/build-gfx.sh                       build everything (obj64)
#     scripts/build-gfx.sh all DEBUG=1           DEBUG build (debug64)
#     scripts/build-gfx.sh syntax-check -j8      per-file -fsyntax-only report
#     scripts/build-gfx.sh syntax-check HOST_STUBS=1 -j8   same on Linux
#     scripts/build-gfx.sh -n                    show the commands only
#
# For syntax-check, linuxkpi (build/linuxkpi) is skipped: it is not built
# from imported sources and has `make check-kmod` instead. A summary over
# all modules is written to build/i915/<objdir>/gfx-syntax-summary.tsv
# (git-ignored with the object directories; regenerated on every run).
#
# Uses gmake on illumos, make elsewhere ($MAKE overrides).
#
set -eu

top=$(cd "$(dirname "$0")/.." && pwd)
if [ -z "${MAKE:-}" ]; then
	if command -v gmake >/dev/null 2>&1; then MAKE=gmake; else MAKE=make; fi
fi

target=all
case ${1:-} in
''|-*|*=*) ;;
*) target=$1; shift ;;
esac

mods="linuxkpi linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915"
[ "$target" = syntax-check ] && mods="linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915"

rc=0
for m in $mods; do
	echo "== $m: $MAKE $target $*"
	$MAKE -C "$top/build/$m" "$target" "$@" || rc=1
done

if [ "$target" = syntax-check ]; then
	objdir=obj64
	for a in "$@"; do case $a in DEBUG=1) objdir=debug64 ;; esac; done
	mkdir -p "$top/build/i915/$objdir"
	sum=$top/build/i915/$objdir/gfx-syntax-summary.tsv
	: >"$sum"
	for m in $mods; do
		r=$top/build/$m/$objdir/syntax-report.tsv
		[ -f "$r" ] && awk -F'\t' -v m="$m" '{print m "\t" $0}' "$r" >>"$sum"
	done
	echo "== all modules (module, status, source, first error in $sum):"
	awk -F'\t' '{n[$1 "\t" $2]++; t[$2]++} END {
		for (k in n) print "  " k "\t" n[k]
		printf "  total\tok\t%d\n  total\tfail\t%d\n", t["ok"], t["fail"] }' "$sum" | sort
fi
exit $rc
