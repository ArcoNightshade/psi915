#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# First real kernel tests of the module set, inside the disposable CI VM,
# after ci/illumos-build.sh built the modules. NEVER RUN ON A REAL MACHINE:
# it copies modules into /usr/kernel, runs add_drv/rem_drv and loads
# untested kernel code. Refuses to run unless CI_VM=1 and uid 0.
#
# Stages (R = required: a failure fails the job; I = informational: reported
# only, until proven; promote by changing the letter in the stage call):
#   R install       stage via scripts/install-all.sh -R, copy into /usr/kernel
#   R kpi-load      modload linuxkpi, modinfo, banner in the kernel log, modunload
#   R kpi-cycle     20 x load/unload of linuxkpi
#   I findleaks     ::findleaks after the cycle (only if kmem_flags != 0)
#   I ktest         modload lkpi_ktest (15 suites run in _init; EIO = a suite failed)
#   I gfx-misc      linuxkpi_gpl_g1, linuxkpi_drm, linuxkpi_gpl_g2 loaded in order,
#                   modinfo, unloaded in reverse
#   I i915          add_drv -n i915 (no aliases: no device binds), modload (only
#                   _init runs: i915_init + pci_register_driver), modunload, rem_drv
#   - toy           skipped: QEMU's edu device is not available in the CI VM
#
# kmem_flags=0xf needs /etc/system and a reboot, which the CI step cannot do
# cheaply; findleaks therefore runs only when the VM image already boots with
# kmem_flags set (FORCE_FINDLEAKS=1 runs it anyway; without KMF_AUDIT the
# live-kernel output has false positives and no stacks).
#
# Environment: CI_VM=1 (required), OBJDIR=obj64|debug64 (default: obj64),
#   CYCLES (default 20), FORCE_FINDLEAKS=1.
# Exit status: 1 if a required stage failed, else 0.
#
set -u

if [ "${CI_VM:-0}" != 1 ]; then
	echo "illumos-load-test: refusing to run without CI_VM=1 (CI VM only)" >&2
	exit 2
fi
if [ "$(id -u)" != 0 ]; then
	echo "illumos-load-test: needs root (CI VM)" >&2
	exit 2
fi

top=$(cd "$(dirname "$0")/.." && pwd)
cd "$top" || exit 2
OBJDIR=${OBJDIR:-obj64}
CYCLES=${CYCLES:-20}
stage_root=/var/tmp/psi915-stage
work=/var/tmp/psi915-lt
rm -rf "$work"
mkdir -p "$work"
TO=
command -v timeout >/dev/null 2>&1 && TO="timeout 300"
HAVE_MDB=0
command -v mdb >/dev/null 2>&1 && HAVE_MDB=1

results=
req_fail=0

note() { echo "   $*"; }

# Kernel message buffer (mdb ::msgbuf is the whole ring; dmesg as fallback).
klog() {
	if [ $HAVE_MDB = 1 ]; then
		echo '::msgbuf' | mdb -k 2>/dev/null
	else
		dmesg 2>/dev/null
	fi
}
logmark() { klog > "$work/log.before"; }
# Lines added to the kernel log since logmark (the ring may have wrapped:
# print what follows the last pre-mark line, or the tail if it is gone).
lognew() {
	klog > "$work/log.after"
	last=$(tail -1 "$work/log.before")
	# Everything after the last occurrence of the pre-mark last line.
	awk -v last="$last" '{l[NR] = $0} $0 == last {m = NR}
	    END {if (m == 0 && last != "") m = NR - 20; if (m < 0) m = 0; for (i = m + 1; i <= NR; i++) print l[i]}' \
	    "$work/log.after" > "$work/log.new"
	cat "$work/log.new"
}

modid() { modinfo | awk -v n="$1" '$6 == n {print $1; exit}'; }

diag() {
	echo "   -- diagnostics"
	echo "   kernel log tail:"; klog | tail -15 | sed 's/^/     /'
	echo "   modinfo -c (psi915 modules):"
	modinfo -c 2>/dev/null | egrep -i 'linuxkpi|lkpi|i915' | sed 's/^/     /'
	if [ $HAVE_MDB = 1 ]; then
		echo "   ::modinfo (psi915 modules):"
		echo '::modinfo' | mdb -k 2>/dev/null | egrep -i 'linuxkpi|lkpi|i915' | sed 's/^/     /'
	fi
}

# stage NAME R|I FUNCTION
# Output streams straight to the console (no capture) so everything printed
# before a kernel hang or panic reaches the CI log.
stage() {
	name=$1 kind=$2 fn=$3
	echo "== [$kind] $name"
	rm -f "$work/$name.rc"
	# Unbuffered indent (a read/echo loop, not sed: sed would hold lines back).
	{ $fn; echo $? > "$work/$name.rc"; } 2>&1 | while IFS= read -r l; do echo "   $l"; done
	if [ -f "$work/$name.rc" ] && [ "$(cat "$work/$name.rc")" = 0 ]; then
		res=PASS
	else
		res=FAIL
	fi
	if [ $res = FAIL ]; then
		diag
		[ "$kind" = R ] && req_fail=1
	fi
	echo "   => $name: $res"
	results="$results
$kind $res $name"
}

# Load a module file; prints the new log lines. $1 path.
load() {
	logmark
	if ! $TO modload "$1"; then
		echo "modload $1 failed"
		lognew | tail -15
		return 1
	fi
	lognew | tail -8
	return 0
}
# Unload by module name.
unload() {
	id=$(modid "$1")
	[ -n "$id" ] || { echo "$1 not loaded"; return 1; }
	logmark
	if ! $TO modunload -i "$id"; then
		echo "modunload $1 (id $id) failed"
		lognew | tail -8
		return 1
	fi
	lognew | tail -4
	[ -z "$(modid "$1")" ] || { echo "$1 still listed after modunload"; return 1; }
	return 0
}

# ---- stages ---------------------------------------------------------------

st_install() {
	rm -rf "$stage_root"
	mkdir -p "$stage_root"
	for m in linuxkpi linuxkpi_gpl_g1 linuxkpi_drm linuxkpi_gpl_g2 i915; do
		sh scripts/install-all.sh -R "$stage_root" -o "$OBJDIR" -m "$m" \
		    >/dev/null 2>"$work/inst.$m" || echo "staging $m: $(tail -1 "$work/inst.$m")"
	done
	[ -f "$stage_root/usr/kernel/misc/amd64/linuxkpi" ] || {
		echo "linuxkpi not staged (not built?)"; return 1; }
	# Copy into the live tree (CI VM only).
	(cd "$stage_root" && find usr -type f) | while read -r f; do
		mkdir -p "/$(dirname "$f")"
		cp "$stage_root/$f" "/$f"
	done
	(cd "$stage_root" && find usr -type f | sed 's/^/installed \//')
	return 0
}

st_kpi_load() {
	load /usr/kernel/misc/amd64/linuxkpi || return 1
	modinfo | awk '$6 == "linuxkpi"' | sed 's/^/modinfo: /'
	grep -q 'linuxkpi .* loaded' "$work/log.new" ||
	    { echo "load banner not found in the kernel log"; unload linuxkpi; return 1; }
	unload linuxkpi
}

st_kpi_cycle() {
	i=0 fail=0
	while [ $i -lt "$CYCLES" ]; do
		i=$((i + 1))
		if ! modload /usr/kernel/misc/amd64/linuxkpi >/dev/null 2>&1; then
			echo "cycle $i: modload failed"; fail=$((fail + 1)); continue
		fi
		id=$(modid linuxkpi)
		if [ -z "$id" ] || ! modunload -i "$id" >/dev/null 2>&1; then
			echo "cycle $i: modunload failed"; fail=$((fail + 1))
		fi
	done
	echo "$CYCLES cycles, $fail failed"
	[ $fail -eq 0 ]
}

st_findleaks() {
	[ $HAVE_MDB = 1 ] || { echo "skipped: no mdb"; return 0; }
	kf=$(echo 'kmem_flags/X' | mdb -k 2>/dev/null | awk 'NF > 1 {print $NF}' | tail -1)
	echo "kmem_flags = ${kf:-unknown}"
	if [ "${FORCE_FINDLEAKS:-0}" != 1 ] && { [ -z "$kf" ] || [ "$kf" = 0 ]; }; then
		echo "skipped: kmem_flags is 0 (setting it needs /etc/system and a reboot)"
		return 0
	fi
	echo '::findleaks' | $TO mdb -k > "$work/leaks" 2>&1
	head -30 "$work/leaks"
	if egrep -qi 'linuxkpi|lkpi' "$work/leaks"; then
		echo "leaks attributed to psi915 modules"; return 1
	fi
	return 0
}

st_ktest() {
	f=build/lkpi_ktest/$OBJDIR/lkpi_ktest
	[ -f "$f" ] || { echo "$f not built"; return 1; }
	logmark
	if ! $TO modload "$f"; then
		echo "modload lkpi_ktest failed (EIO = a suite failed)"
		lognew | egrep -i 'ktest|fail|linuxkpi' | tail -30
		[ -n "$(modid linuxkpi)" ] && unload linuxkpi >/dev/null 2>&1
		return 1
	fi
	lognew > "$work/ktest.log"
	egrep -i 'ktest|fail' "$work/ktest.log" | tail -30
	ok=0
	grep -q 'suites, 0 failed' "$work/ktest.log" || { echo "summary line missing"; ok=1; }
	unload lkpi_ktest || ok=1
	unload linuxkpi || ok=1
	return $ok
}

st_gfx_misc() {
	ok=0
	for m in linuxkpi_gpl_g1 linuxkpi_drm linuxkpi_gpl_g2; do
		p=/usr/kernel/misc/amd64/$m
		[ -f "$p" ] || { echo "$m not installed (not built?)"; ok=1; break; }
		echo "-- load $m"
		load "$p" || { ok=1; break; }
		modinfo | awk -v n="$m" '$6 == n' | sed 's/^/modinfo: /'
	done
	for m in linuxkpi_gpl_g2 linuxkpi_drm linuxkpi_gpl_g1 linuxkpi; do
		[ -n "$(modid "$m")" ] || continue
		echo "-- unload $m"
		unload "$m" || ok=1
	done
	return $ok
}

st_i915() {
	p=/usr/kernel/drv/amd64/i915
	[ -f "$p" ] || { echo "i915 not installed (not built?)"; return 1; }
	if [ -f /kernel/drv/amd64/i915 ] || grep -q '^i915 ' /etc/name_to_major 2>/dev/null; then
		echo "skipped: another i915 driver is registered in this VM (gfx-drm)"
		return 0
	fi
	add_drv -n i915 || { echo "add_drv -n i915 failed"; return 1; }
	ok=0
	if load "$p"; then
		modinfo | awk '$6 == "i915"' | sed 's/^/modinfo: /'
		unload i915 || ok=1
	else
		echo "(no device in the VM: only _init can run; a failure here is in _init)"
		ok=1
	fi
	for m in linuxkpi_gpl_g2 linuxkpi_drm linuxkpi_gpl_g1 linuxkpi; do
		[ -n "$(modid "$m")" ] && { unload "$m" >/dev/null 2>&1 || true; }
	done
	rem_drv i915 || echo "rem_drv i915 failed"
	return $ok
}

# ---- run ------------------------------------------------------------------

echo "== psi915 load test ($(uname -v), OBJDIR=$OBJDIR, mdb: $HAVE_MDB)"
stage install R st_install
if [ $req_fail = 0 ]; then
	stage kpi-load R st_kpi_load
	stage kpi-cycle R st_kpi_cycle
	stage findleaks I st_findleaks
	stage ktest I st_ktest
	stage gfx-misc I st_gfx_misc
	stage i915 I st_i915
fi
echo "== [-] toy: skipped (no QEMU edu device in the CI VM)"
echo "== load test summary (kind result stage; R = required)"
echo "$results" | sed '/^$/d; s/^/   /'
exit $req_fail
