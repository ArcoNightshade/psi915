#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Neutral installer for psi915 kernel artefacts.
#
# Reads build/install.manifest and either installs the listed files under a
# root directory, or prints the layout in a form that package tooling can use:
#
#   install.sh [-R root] [-o obj64|debug64] [-n]    install (default root /)
#   install.sh -f list|ips|svr4 [-o obj64|debug64]  print layout only
#
#   -R root   install under this root (a DESTDIR or a package staging dir)
#   -o dir    object directory to take modules from (default obj64)
#   -n        dry run: print the actions without doing them
#   -f fmt    list: plain table; ips: p5m actions; svr4: prototype(5) lines
#
# Wrappers: an IPS build stages with "-R $PROTO" and appends "-f ips" output
# to its p5m manifest; an SVR4 (Tribblix) build stages with "-R $PROTO" and
# uses "-f svr4" output as the prototype file body.
#
# Kernel modules are not loaded or registered here. The linuxkpi module is a
# misc module and needs no add_drv; load it with
#   modload /usr/kernel/misc/amd64/linuxkpi
#
# Portable to illumos /bin/sh (ksh93) and Linux /bin/sh (dash).
#
# UNTESTED ON ILLUMOS as of P0 (exercised on Linux with -R into a temp dir).
#

set -eu

prog=$(basename "$0")
srcroot=$(cd "$(dirname "$0")/.." && pwd)
manifest="$srcroot/build/install.manifest"

root=/
objdir=obj64
dryrun=0
format=

usage() {
	echo "usage: $prog [-R root] [-o obj64|debug64] [-n] [-f list|ips|svr4]" >&2
	exit 2
}

while getopts "R:o:nf:h" opt; do
	case "$opt" in
	R) root=$OPTARG ;;
	o) objdir=$OPTARG ;;
	n) dryrun=1 ;;
	f) format=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 0 ] || usage

case "$objdir" in
obj64|debug64) ;;
*) echo "$prog: -o must be obj64 or debug64" >&2; exit 2 ;;
esac
case "$format" in
''|list|ips|svr4) ;;
*) echo "$prog: unknown format '$format'" >&2; exit 2 ;;
esac
[ -n "$root" ] || root=/

[ -r "$manifest" ] || { echo "$prog: missing $manifest" >&2; exit 1; }

run() {
	if [ "$dryrun" -eq 1 ]; then
		echo "$*"
	else
		"$@"
	fi
}

# Only change ownership when running as root (package builds usually are not;
# the packaging system records owner/group itself).
is_root=0
[ "$(id -u 2>/dev/null || echo 1)" = 0 ] && is_root=1

status=0
grep -v '^[[:space:]]*#' "$manifest" | grep -v '^[[:space:]]*$' |
while read -r type mode owner group path source; do
	source=$(echo "${source:-}" | sed "s/@OBJDIR@/$objdir/g")
	case "$format" in
	list)
		printf '%s\t%s\t%s\t%s\t%s\n' "$type" "$mode" "$owner" "$group" "$path"
		continue ;;
	ips)
		printf '%s path=%s owner=%s group=%s mode=%s\n' \
		    "$type" "$path" "$owner" "$group" "$mode"
		continue ;;
	svr4)
		case "$type" in
		dir) t=d ;;
		file) t=f ;;
		*) echo "$prog: unknown type $type" >&2; exit 1 ;;
		esac
		printf '%s none %s %s %s %s\n' "$t" "$path" "$mode" "$owner" "$group"
		continue ;;
	esac

	dest="${root%/}/$path"
	case "$type" in
	dir)
		run mkdir -p "$dest"
		run chmod "$mode" "$dest"
		;;
	file)
		src="$srcroot/$source"
		if [ ! -f "$src" ] && [ "$dryrun" -eq 0 ]; then
			echo "$prog: $src not built (run gmake -C build first)" >&2
			exit 1
		fi
		run mkdir -p "$(dirname "$dest")"
		run cp "$src" "$dest.new"
		run chmod "$mode" "$dest.new"
		run mv -f "$dest.new" "$dest"
		;;
	*)
		echo "$prog: unknown manifest type '$type'" >&2
		exit 1
		;;
	esac
	if [ "$is_root" -eq 1 ]; then
		run chown "$owner:$group" "$dest"
	fi
done || status=$?

exit $status
