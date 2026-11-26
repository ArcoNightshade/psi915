# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Linux host: only for cross-compiling to illumos with an illumos sysroot
# (meson/cross-x86_64-illumos.ini) or for a dry run of the scripts.

if [ -n "$PSI_CROSS_FILE" ]; then
	: "${PSI_SYSROOT:?set PSI_SYSROOT to an illumos sysroot (see meson/cross-x86_64-illumos.ini)}"
	PKG_CONFIG_SYSROOT_DIR="$PSI_SYSROOT"
	PKG_CONFIG_LIBDIR="$PSI_SYSROOT$PREFIX/$PSI_LIBDIR/pkgconfig:$PSI_SYSROOT/usr/lib/amd64/pkgconfig:$PSI_SYSROOT/usr/share/pkgconfig"
	export PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR
else
	PSI_NATIVE_FILE=""
fi
TAR=$(pick tar gtar)
PATCH=$(pick patch gpatch)
PSI_DEPS="meson ninja-build pkg-config python3-mako python3-yaml glslang-tools"
PSI_DEPS_CMD="(install with your distribution's package manager)"
