# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# OpenIndiana (target: 2026.10, Hipster) build environment. Sourced by build.sh.
# UNTESTED. Package FMRIs below are believed correct from oi-userland
# (snapshot e6b900913634, 2026-11-24) but were not checked against a live repo.

PSI_NATIVE_FILE="$PSI_USERLAND/meson/native-oi.ini"

# 64-bit only. OI installs 64-bit libraries in /usr/lib/amd64 and their .pc
# files in /usr/lib/amd64/pkgconfig.
PKG_CONFIG_PATH="$PREFIX/$PSI_LIBDIR/pkgconfig:$PREFIX/share/pkgconfig:/usr/lib/amd64/pkgconfig:/usr/share/pkgconfig"
export PKG_CONFIG_PATH

# oi-userland make-rules: GCC_DEFAULT = 14, CLANG_DEFAULT = 21 (LLVM in /usr/clang/21).
PATH="/usr/gcc/14/bin:/usr/clang/21/bin:$PATH"
export PATH
: "${CC:=gcc}"
: "${CXX:=g++}"
export CC CXX

TAR=$(pick gtar tar)
PATCH=$(pick gpatch patch)

# pkg install line for the build prerequisites (printed by `build.sh deps`).
# x11/library/libdrm and x11/library/mesa (old, from gfx-drm and Mesa 21.3.9)
# may stay installed; the psi915 stack lives in $PREFIX and uses RUNPATH.
PSI_DEPS="
developer/gcc-14
developer/clang-21
developer/build/meson
developer/build/ninja
developer/build/pkg-config
developer/build/gnu-make
developer/lexer/flex
developer/parser/bison
developer/versioning/git
text/gnu-patch
archiver/gnu-tar
compress/xz
library/python/mako
library/python/pyyaml
library/expat
library/zlib
library/zstd
x11/library/libpciaccess
x11/library/libxshmfence
x11/library/libx11
x11/library/libxcb
x11/library/libxext
x11/library/libxfixes
x11/library/libxrandr
x11/library/libxxf86vm
x11/library/libxdamage
x11/library/libxfont2
x11/library/libxkbfile
x11/library/libxcvt
x11/library/libepoxy
x11/library/xtrans
x11/header/x11-protocols
image/library/libpng
library/graphics/pixman
x11/glslang
x11/spirv-tools
x11/spirv-headers
"
PSI_DEPS_CMD="pfexec pkg install"
