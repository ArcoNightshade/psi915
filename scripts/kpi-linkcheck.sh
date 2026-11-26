#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Link-time closure of the graphics module set against the KPI (docs/11
# section 8, item 1), on the Linux dev host: compile every module's objects
# with HOST_STUBS=1 (stub illumos headers; units that fail to compile are
# skipped), then list the symbols the objects reference that neither the
# module set, nor linuxkpi (tests/out/kmod_defined.txt from
# `make check-kmod`), nor the illumos allowlists define. Those would be
# unresolved at modload on illumos.
#
#   scripts/kpi-linkcheck.sh [-j N]
#
# Output in tests/out/linkcheck/: missing.tsv (symbol, number of
# referencing objects, first referencing object) and summary.txt.
# Exit status 0 even when symbols are missing: it is a measuring tool.
# This is NOT an illumos link.
#
set -eu
top=$(cd "$(dirname "$0")/.." && pwd)
jobs=8
[ "${1:-}" = -j ] && jobs=$2
MAKE=${MAKE:-make}
out=$top/tests/out/linkcheck
mkdir -p "$out"

"$MAKE" -s -C "$top" check-kmod >/dev/null
for m in linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915; do
	# The module Makefiles track no header dependencies: start clean.
	rm -rf "$top/build/$m/obj64/objs"
	"$MAKE" -s -C "$top/build/$m" HOST_STUBS=1 -k -j"$jobs" objects \
	    >"$out/build-$m.log" 2>&1 || true
done

objs=$(find "$top"/build/linuxkpi_gpl_g1/obj64/objs "$top"/build/drm/obj64/objs \
    "$top"/build/linuxkpi_gpl_g2/obj64/objs "$top"/build/i915/obj64/objs \
    -name '*.o' 2>/dev/null | sort)
nobj=$(echo "$objs" | wc -l)

# shellcheck disable=SC2086
nm -g --defined-only $objs 2>/dev/null | awk 'NF==3 {print $3}' |
    cat - "$top/tests/out/kmod_defined.txt" | sort -u >"$out/defined.txt"
cat "$top/tests/kmod_allowed_syms.txt" "$top/drmglue/kmod_allowed_syms.txt" |
    grep -v '^#' | awk 'NF > 0 {print $1}' | sort -u >"$out/allowed.txt"
# shellcheck disable=SC2086
for o in $objs; do
	nm -u "$o" 2>/dev/null | awk -v o="${o#"$top"/}" 'NF==2 {print $2 "\t" o}'
done | sort >"$out/refs.tsv"
cut -f1 "$out/refs.tsv" | sort -u | comm -23 - "$out/defined.txt" |
    comm -23 - "$out/allowed.txt" >"$out/missing.txt"
awk -F'\t' 'NR==FNR {m[$1]=1; next} ($1 in m) {n[$1]++; if (!($1 in f)) f[$1]=$2}
    END {for (s in n) printf "%s\t%d\t%s\n", s, n[s], f[s]}' \
    "$out/missing.txt" "$out/refs.tsv" | sort -t"$(printf '\t')" -k2,2nr -k1,1 \
    >"$out/missing.tsv"
{
	echo "KPI link check (HOST_STUBS objects; NOT an illumos link)"
	echo "objects: $nobj; unresolved symbols: $(wc -l <"$out/missing.txt")"
	echo
	head -60 "$out/missing.tsv"
} >"$out/summary.txt"
cat "$out/summary.txt"
