#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# gen-driver-aliases.sh: emit driver_aliases(5) / add_drv -i / IPS "alias="
# lines for the Gen9 (graphics version 9) Intel GPUs from Linux's
# include/drm/intel/pciids.h (MIT licensed; read at build time, not copied).
#
# usage: gen-driver-aliases.sh [-f aliases|adddrv|ips] [-d driver] pciids.h
#   aliases  "i915 \"pci8086,5917\""          (driver_aliases fragment, default)
#   adddrv   '"pci8086,5917" "pci8086,..."'    (argument for add_drv -i)
#   ips      "    alias=pci8086,5917 \"         (driver action continuation)
#
# Families: SKL, BXT, GLK, KBL (incl. AML-KBL), CFL (incl. AML-CFL), WHL, CML.
# Families match the Gen9 entries of i915_pci.c pciidlist (upstream Linux
# v6.14 import in i915/, same list as drm-kmod 98be24dc9994).
# Needs a C preprocessor (cpp, gcc -E or clang -E).

set -e
fmt=aliases
drv=i915
while getopts f:d: o; do
	case $o in
	f) fmt=$OPTARG ;;
	d) drv=$OPTARG ;;
	*) echo "usage: $0 [-f aliases|adddrv|ips] [-d driver] pciids.h" >&2; exit 2 ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 1 ] && [ -r "$1" ] || { echo "usage: $0 [-f fmt] [-d driver] pciids.h" >&2; exit 2; }
hdr=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")

if command -v cpp >/dev/null 2>&1; then CPP="cpp -P"
elif command -v gcc >/dev/null 2>&1; then CPP="gcc -E -P -x c"
else CPP="clang -E -P -x c"; fi

tmp=${TMPDIR:-/tmp}/gen-aliases.$$
trap 'rm -f "$tmp.c" "$tmp.i"' EXIT
cat > "$tmp.c" <<EOF
#include "$hdr"
#define PSI_ID(id, ...) PSI_ALIAS id
INTEL_SKL_IDS(PSI_ID)
INTEL_BXT_IDS(PSI_ID)
INTEL_GLK_IDS(PSI_ID)
INTEL_KBL_IDS(PSI_ID)
INTEL_CFL_IDS(PSI_ID)
INTEL_WHL_IDS(PSI_ID)
INTEL_CML_IDS(PSI_ID)
EOF

$CPP "$tmp.c" > "$tmp.i" || { echo "$0: preprocessing $hdr failed" >&2; exit 1; }
tr ',' '\n' < "$tmp.i" | awk '$1 == "PSI_ALIAS" { print tolower($2) }' |
    sed 's/^0x0*//' | sort -u | awk -v fmt="$fmt" -v drv="$drv" '
	{ id[++n] = $1 }
	END {
		for (i = 1; i <= n; i++) {
			if (fmt == "aliases") printf("%s \"pci8086,%s\"\n", drv, id[i])
			else if (fmt == "ips") printf("    alias=pci8086,%s \\\n", id[i])
			else printf("%s\"pci8086,%s\"", (i > 1 ? " " : ""), id[i])
		}
		if (fmt == "adddrv") printf("\n")
	}'
