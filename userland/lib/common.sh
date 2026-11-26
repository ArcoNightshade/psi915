# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Shared helpers for userland/build.sh. POSIX sh (illumos /bin/sh is ksh93,
# which is fine). Sourced, not executed. UNTESTED ON ILLUMOS.

msg()  { printf '%s\n' "build: $*"; }
warn() { printf '%s\n' "build: WARNING: $*" >&2; }
die()  { printf '%s\n' "build: ERROR: $*" >&2; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

# First command found in the argument list.
pick() {
	for c in "$@"; do
		if have "$c"; then echo "$c"; return 0; fi
	done
	return 1
}

detect_distro() {
	if [ -n "$PSI_DISTRO" ]; then echo "$PSI_DISTRO"; return; fi
	case "$(uname -s)" in
	SunOS)
		if [ -f /etc/zap/version.current ] || [ -d /etc/zap ]; then
			echo tribblix
		elif grep -qi openindiana /etc/release 2>/dev/null; then
			echo oi
		else
			echo illumos
		fi ;;
	Linux)	echo linux ;;
	*)	echo unknown ;;
	esac
}

sha256_of() {
	if have sha256sum; then sha256sum "$1" | awk '{print $1}'
	elif have digest; then digest -a sha256 "$1"
	elif have shasum; then shasum -a 256 "$1" | awk '{print $1}'
	else die "no sha256 tool (sha256sum/digest/shasum)"; fi
}

fetch_url() { # url dest
	if have curl; then curl -fL --retry 3 -o "$2" "$1"
	elif have wget; then wget -O "$2" "$1"
	else die "need curl or wget"; fi
}

# Read one field of a versions.conf entry: conf_get name field(2..5)
conf_get() {
	awk -v n="$1" -v f="$2" '$1 == n && $0 !~ /^#/ { print $f; exit }' \
	    "$PSI_USERLAND/versions.conf"
}

conf_names() {
	awk 'NF >= 5 && $0 !~ /^#/ { print $1 }' "$PSI_USERLAND/versions.conf"
}

# Fetch and unpack a component; prints the source directory.
fetch_component() { # name
	name=$1
	ver=$(conf_get "$name" 2); kind=$(conf_get "$name" 3)
	url=$(conf_get "$name" 4); sum=$(conf_get "$name" 5)
	[ -n "$url" ] || die "$name: not in versions.conf"
	mkdir -p "$WORK/dl" "$WORK/src"
	case "$kind" in
	tar)
		file="$WORK/dl/${url##*/}"
		[ -f "$file" ] || fetch_url "$url" "$file" >&2 || die "$name: download failed: $url"
		got=$(sha256_of "$file")
		if [ "$sum" = "-" ]; then
			warn "$name $ver: no pinned sha256; saw $got (recorded in $WORK/digests.txt)"
			echo "$name $ver $got" >> "$WORK/digests.txt"
		elif [ "$got" != "$sum" ]; then
			die "$name: sha256 mismatch: want $sum got $got"
		fi
		dir="$WORK/src/$name-$ver"
		if [ ! -d "$dir" ]; then
			mkdir -p "$dir.tmp"
			case "$file" in
			*.tar.xz)  xz -dc "$file" | (cd "$dir.tmp" && $TAR xf -) ;;
			*.tar.gz)  gzip -dc "$file" | (cd "$dir.tmp" && $TAR xf -) ;;
			*.tar.bz2) bzip2 -dc "$file" | (cd "$dir.tmp" && $TAR xf -) ;;
			*) die "$name: unknown archive type $file" ;;
			esac || die "$name: extract failed"
			# tarballs carry one top-level directory
			mv "$dir.tmp"/* "$dir" && rmdir "$dir.tmp"
			apply_series "$name" "$dir"
		fi ;;
	git)
		repo=${url%%#*}; ref=${url#*#}
		dir="$WORK/src/$name-$ver"
		if [ ! -d "$dir" ]; then
			git clone --depth 1 --branch "$ref" "$repo" "$dir" >&2 \
			    || die "$name: clone failed: $repo"
			( cd "$dir" && echo "$name $(git rev-parse HEAD)" ) >> "$WORK/digests.txt"
			apply_series "$name" "$dir"
		fi ;;
	*) die "$name: unknown kind $kind" ;;
	esac
	echo "$dir"
}

# Apply userland/patches/<name>/series (quilt format: one patch per line,
# '#' comments). Patches are applied once, right after unpacking.
apply_series() { # name dir
	series="$PSI_USERLAND/patches/$1/series"
	[ -f "$series" ] || return 0
	grep -v '^[[:space:]]*#' "$series" | while read -r p opt; do
		[ -n "$p" ] || continue
		pf="$PSI_USERLAND/patches/$1/$p"
		[ -f "$pf" ] || die "$1: series lists missing patch $p"
		msg "$1: applying $p" >&2
		(cd "$2" && $PATCH -p1 $opt < "$pf") >&2 || die "$1: patch $p failed"
	done
}

# meson setup + compile + install for one component.
meson_build() { # name srcdir [extra meson args...]
	name=$1; src=$2; shift 2
	bld="$WORK/build/$name"
	if [ ! -f "$bld/build.ninja" ]; then
		meson setup "$bld" "$src" $MESON_MACHINE_ARGS \
		    --prefix="$PREFIX" --libdir="$PSI_LIBDIR" \
		    --buildtype=debugoptimized "$@" || die "$name: meson setup failed"
	fi
	ninja -C "$bld" $NINJA_JOBS || die "$name: build failed"
	if [ -n "$DESTDIR" ]; then
		DESTDIR="$DESTDIR" meson install -C "$bld" --no-rebuild || die "$name: install failed"
	else
		meson install -C "$bld" --no-rebuild || die "$name: install failed"
	fi
}

# Does the source tree's meson option file define this option?
meson_has_opt() { # srcdir option
	for f in "$1/meson_options.txt" "$1/meson.options"; do
		[ -f "$f" ] && grep -q "option('$2'" "$f" && return 0
	done
	return 1
}

# Echo "-Dopt=val" only if the tree has that option (versions differ).
mopt() { # srcdir opt val
	if meson_has_opt "$1" "$2"; then echo "-D$2=$3"; else warn "option $2 not in $(basename "$1"), skipped"; fi
}
