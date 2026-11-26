#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Installer for the full psi915 module set: linuxkpi, linuxkpi_gpl_g1,
# linuxkpi_drm, linuxkpi_gpl_g2 (misc, /usr/kernel/misc/amd64), the i915
# driver (/usr/kernel/drv/amd64/i915 + /usr/kernel/drv/i915.conf) and the
# Kaby Lake firmware (/usr/lib/firmware/i915), per build/drm/install-gfx.manifest.
#
#   install-all.sh [-R root] [-o obj64|debug64] [-F fwdir] [-m module] [-n]
#   install-all.sh -f list|ips|svr4 [-m module]
#   install-all.sh -d [-R root] [-A]      print the driver registration steps
#
#   -R root   install under this root: / , a DESTDIR / package proto area, or
#             a mounted boot environment (beadm mount <be> /mnt; -R /mnt)
#   -o dir    object directory (default obj64)
#   -F dir    firmware staging dir (default build/i915/obj64/firmware, from
#             scripts/fetch-i915-firmware.sh); missing firmware is skipped
#   -m mod    only this manifest module tag (linuxkpi, linuxkpi_gpl_g1,
#             linuxkpi_drm, linuxkpi_gpl_g2, i915, firmware; "all"-tagged
#             directories always come along)
#   -n        dry run: print the actions
#   -f fmt    print the layout (list | IPS p5m actions | SVR4 prototype)
#   -d        print (never run) the add_drv / devlink / minor_perm steps
#   -A        with -d: alias every Gen9 device of
#             userland/config/driver_aliases.gen9, not only pci8086,5917
#
# Non-destructive by design: it copies files (via name.new + mv) and creates
# directories. It never runs add_drv, rem_drv, update_drv, modload or
# devfsadm, never edits /etc/name_to_major, /etc/driver_aliases,
# /etc/minor_perm or /etc/devlink.tab, and refuses -R / when a different i915
# driver (gfx-drm's driver/graphics/drm) is installed. The registration
# commands are printed for the operator (-d, and after an install).
#
# Portable to illumos /bin/sh (ksh93) and dash. UNTESTED ON ILLUMOS
# (exercised on Linux with -R into a temporary directory).
#
set -eu

prog=$(basename "$0")
srcroot=$(cd "$(dirname "$0")/.." && pwd)
manifest=$srcroot/build/drm/install-gfx.manifest
aliases=$srcroot/userland/config/driver_aliases.gen9
minorperm=$srcroot/userland/config/minor_perm.psi915
devlink=$srcroot/userland/config/devlink.tab.psi915

root=/
objdir=obj64
fwdir=$srcroot/build/i915/obj64/firmware
module=
dryrun=0
format=
drvsteps=0
allaliases=0

usage() {
	sed -n '2,/^set -eu/p' "$0" | sed '$d; s/^# \{0,1\}//' >&2
	exit 2
}

while getopts "R:o:F:m:nf:dAh" opt; do
	case $opt in
	R) root=$OPTARG ;;
	o) objdir=$OPTARG ;;
	F) fwdir=$OPTARG ;;
	m) module=$OPTARG ;;
	n) dryrun=1 ;;
	f) format=$OPTARG ;;
	d) drvsteps=1 ;;
	A) allaliases=1 ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 0 ] || usage
case $objdir in obj64|debug64) ;; *) echo "$prog: -o must be obj64 or debug64" >&2; exit 2 ;; esac
case $format in ''|list|ips|svr4) ;; *) echo "$prog: unknown format '$format'" >&2; exit 2 ;; esac
[ -n "$root" ] || root=/
[ -r "$manifest" ] || { echo "$prog: missing $manifest" >&2; exit 1; }

run() {
	if [ $dryrun -eq 1 ]; then echo "$*"; else "$@"; fi
}

is_root=0
[ "$(id -u 2>/dev/null || echo 1)" = 0 ] && is_root=1

# Another driver called i915 (gfx-drm) would shadow ours: /kernel is searched
# before /usr/kernel, and the name is already bound in name_to_major.
other_i915() {
	r=${root%/}
	if [ -f "$r/kernel/drv/amd64/i915" ] || [ -f "$r/platform/i86pc/kernel/drv/amd64/i915" ]; then
		echo "$r/kernel/drv/amd64/i915"
		return 0
	fi
	return 1
}

aliases_arg() {
	if [ $allaliases -eq 1 ]; then
		awk '$1 == "i915" {printf "%s%s", sep, $2; sep=" "}' "$aliases"
	else
		printf '"pci8086,5917"'
	fi
}

minorperm_arg() {
	# add_drv -m takes "minor mode owner group" entries, comma separated.
	{ awk '!/^#/ && NF == 4 {split($1, a, ":"); print a[2], $2, $3, $4}' "$minorperm"
	  echo "text-* 0600 root sys"; } | awk '{printf "%s%s", sep, $0; sep=","}'
}

print_driver_steps() {
	r=${root%/}
	b=
	[ "$r" = "" ] || b="-b $r "
	cat <<EOF

== Driver registration (not done by $prog; run as root, preferably in a new
   boot environment: beadm create psi915 && beadm mount psi915 /mnt, then
   use -R /mnt above and the "-b /mnt" form below)

1. If gfx-drm (driver/graphics/drm) is installed, its i915 must go first:
     pkg uninstall driver/graphics/drm     (OpenIndiana; Tribblix: zap uninstall)
   or, keeping the package, rem_drv ${b}i915 (both claim the name i915).

2. Register the driver without attaching it now (-n): the boot console GPU
   is held by vgatext until the next boot, which then binds i915 by alias.
     add_drv ${b}-n -v \\
       -m '$(minorperm_arg)' \\
       -i '$(aliases_arg)' \\
       i915
   (-A on $prog lists every Gen9 alias of userland/config/driver_aliases.gen9.)

3. /dev/dri links: add this line (one TAB) to $r/etc/devlink.tab unless
   gfx-drm's devfsadm link module is installed (it already links
   ddi_display:drm nodes):
$(grep -v '^#' "$devlink" | sed 's/^/     /')

4. Optional /etc/system settings for bring-up (see drivers/i915_glue/i915.conf):
     set kmem_flags = 0xf
     set linuxkpi:linuxkpi_bug_panic = 1
   Keep a serial console or kmdb (-k) available: tem output is invisible
   while a DRM master owns the display and console restore is not
   implemented yet (docs/11 section 5).

5. Reboot into the boot environment. Check:
     modinfo | egrep 'linuxkpi|i915'
     ls -l /dev/dri/          (card0, renderD128)
     prtconf -D | grep i915
   Load order is automatic: i915 -> linuxkpi_gpl_g2 -> linuxkpi_drm ->
   linuxkpi_gpl_g1 -> linuxkpi (DT_NEEDED). Without the GPU bound, the
   misc modules can be tried alone in that reverse order with modload.

Undo: rem_drv ${b}i915; remove the devlink.tab line; delete the files listed
by "$prog -f list" under $r/.
EOF
	if f=$(other_i915); then
		echo
		echo "NOTE: $f exists (another i915 driver); step 1 is required."
	fi
}

if [ $drvsteps -eq 1 ]; then
	print_driver_steps
	exit 0
fi

if [ -z "$format" ] && [ "${root%/}" = "" ] && [ $dryrun -eq 0 ] && f=$(other_i915); then
	echo "$prog: $f exists: another i915 driver (gfx-drm) is installed on /." >&2
	echo "  Install into a new boot environment instead (-R /mnt, see -d)." >&2
	exit 1
fi

status=0
grep -v '^[[:space:]]*#' "$manifest" | grep -v '^[[:space:]]*$' |
while read -r type mode owner group path source mod; do
	if [ -n "$module" ] && [ "$mod" != "$module" ] && [ "$mod" != all ]; then
		continue
	fi
	source=$(echo "$source" | sed "s|@OBJDIR@|$objdir|g; s|@FWDIR@|$fwdir|g")
	case $format in
	list)
		printf '%s\t%s\t%s\t%s\t%s\n' "$type" "$mode" "$owner" "$group" "$path"
		continue ;;
	ips)
		t=$type; [ "$t" = ofile ] && t=file
		printf '%s path=%s owner=%s group=%s mode=%s\n' "$t" "$path" "$owner" "$group" "$mode"
		continue ;;
	svr4)
		case $type in dir) t=d ;; file|ofile) t=f ;; *) echo "$prog: bad type $type" >&2; exit 1 ;; esac
		printf '%s none %s %s %s %s\n' "$t" "$path" "$mode" "$owner" "$group"
		continue ;;
	esac

	dest="${root%/}/$path"
	case $type in
	dir)
		run mkdir -p "$dest"
		run chmod "$mode" "$dest"
		;;
	file|ofile)
		case $source in /*) src=$source ;; *) src=$srcroot/$source ;; esac
		if [ ! -f "$src" ]; then
			if [ "$type" = ofile ]; then
				echo "$prog: skipping $path ($src not present)" >&2
				continue
			fi
			if [ $dryrun -eq 0 ]; then
				echo "$prog: $src not built (gmake -C build/<module>)" >&2
				exit 1
			fi
		fi
		run mkdir -p "$(dirname "$dest")"
		run cp "$src" "$dest.new"
		run chmod "$mode" "$dest.new"
		run mv -f "$dest.new" "$dest"
		;;
	*)
		echo "$prog: unknown manifest type '$type'" >&2
		exit 1 ;;
	esac
	if [ $is_root -eq 1 ]; then
		run chown "$owner:$group" "$dest"
	fi
done || status=$?

if [ -z "$format" ] && [ $status -eq 0 ] && { [ -z "$module" ] || [ "$module" = i915 ]; }; then
	print_driver_steps
fi
exit $status
