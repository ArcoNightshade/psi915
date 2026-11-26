#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Fetch the Kaby Lake firmware i915 (Linux 6.14) asks for, from linux-firmware,
# into a staging directory (never into the system), and verify SHA-256 sums
# pinned below. scripts/install-all.sh then installs it under
# /usr/lib/firmware/i915 (LKPI_FIRMWARE_DIR, include/linux/firmware.h).
#
#   i915/kbl_dmc_ver1_04.bin   display microcontroller (DC states). Needed
#                              for display power saving; i915 runs without it
#                              (warns, no DC5/DC6).
#   i915/kbl_guc_70.1.1.bin    GuC; loaded only with enable_guc > 0 (Gen9
#                              default 0)
#   i915/kbl_huc_4.0.0.bin     HuC (media); needs enable_guc=2
#   WHENCE                     linux-firmware licence index (i915 entries:
#                              "Redistributable. See LICENSE.i915")
# Names from i915/drivers/gpu/drm/i915/display/intel_dmc.c (KBL_DMC_PATH) and
# gt/uc/intel_uc_fw.c (KABYLAKE guc_mmp/huc_mmp).
#
# Usage: scripts/fetch-i915-firmware.sh [-o dir] [-u base-url] [-n]
#   -o dir   staging directory (default build/i915/obj64/firmware, ignored
#            by git); files land in dir/i915/ and dir/WHENCE
#   -u url   base URL of a linux-firmware tree (raw files). Default: the
#            official GitLab repository; tried in order: $FW_BASE_URL (if
#            set), gitlab.com, git.kernel.org. A GitHub mirror works too,
#            e.g. -u https://raw.githubusercontent.com/<owner>/linux-firmware/main
#   -n       print what would be fetched
# Needs curl or wget, and sha256sum, shasum or illumos digest(1).
#
# The pinned sums were taken from gitlab.com/kernel-firmware/linux-firmware
# main on 2026-11-26. A changed sum aborts; check the file and update the pin.
# Firmware is redistributable but not modifiable; it is not part of psi915.
#
set -eu

top=$(cd "$(dirname "$0")/.." && pwd)
out=$top/build/i915/obj64/firmware
base=
dry=0
while getopts "o:u:nh" o; do
	case $o in
	o) out=$OPTARG ;;
	u) base=$OPTARG ;;
	n) dry=1 ;;
	*) sed -n '2,/^set -eu/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 2 ;;
	esac
done

FILES='
i915/kbl_dmc_ver1_04.bin 2cde41c3e5ad181423bcc3e98ff9c49f743c88f18646af4d0b3c3a9664b831a1
i915/kbl_guc_70.1.1.bin 497bb8b2b90086aa681f90c917e89d7c6299fc096b1d89369d49d24d467a1112
i915/kbl_huc_4.0.0.bin 8bc643aa06a173c2969ac5946e9183544521048985d53e7148b85a898e85d10f
WHENCE -
'
BASES="${base:-${FW_BASE_URL:-}} https://gitlab.com/kernel-firmware/linux-firmware/-/raw/main https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"

if command -v curl >/dev/null 2>&1; then
	fetch() { curl -fsSL --max-time 120 -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
	fetch() { wget -q -T 120 -O "$2" "$1"; }
else
	echo "fetch-i915-firmware: need curl or wget" >&2; exit 1
fi
sha256() {
	if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
	elif command -v digest >/dev/null 2>&1; then digest -a sha256 "$1"
	else shasum -a 256 "$1" | awk '{print $1}'; fi
}

rc=0
echo "$FILES" | while read -r f sum; do
	[ -n "$f" ] || continue
	dst=$out/$f
	if [ $dry = 1 ]; then
		echo "fetch $f -> $dst (sha256 $sum)"
		continue
	fi
	mkdir -p "$(dirname "$dst")"
	got=0
	for b in $BASES; do
		[ -n "$b" ] || continue
		if fetch "$b/$f" "$dst.part" 2>/dev/null; then got=1; break; fi
	done
	if [ $got = 0 ]; then
		rm -f "$dst.part"
		echo "fetch-i915-firmware: could not fetch $f (tried: $BASES)" >&2
		echo "  copy it by hand from a linux-firmware checkout to $dst" >&2
		exit 1
	fi
	if [ "$sum" != "-" ]; then
		s=$(sha256 "$dst.part")
		if [ "$s" != "$sum" ]; then
			rm -f "$dst.part"
			echo "fetch-i915-firmware: $f: sha256 $s, expected $sum" >&2
			exit 1
		fi
	fi
	mv -f "$dst.part" "$dst"
	echo "fetch-i915-firmware: $f OK"
done || rc=$?
[ $dry = 1 ] || [ $rc != 0 ] || echo "fetch-i915-firmware: staged in $out; install with scripts/install-all.sh"
exit $rc
