# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Tribblix (target: 0m41.1) build environment. Sourced by build.sh.
# UNTESTED. Package names are believed correct from tribblix/build
# (snapshot 40b0bbecfe7e, 2026-11-24); that repo holds build recipes, not the
# published 0m41.1 catalog, so versions on a live system may differ.

PSI_NATIVE_FILE="$PSI_USERLAND/meson/native-tribblix.ini"

# Tribblix 64-bit libraries: /usr/lib/amd64, .pc files in
# /usr/lib/amd64/pkgconfig. LLVM (TRIBllvm, 22.1.7 in the recipe) lives in
# /usr/versions/llvm.
PKG_CONFIG_PATH="$PREFIX/$PSI_LIBDIR/pkgconfig:$PREFIX/share/pkgconfig:/usr/lib/amd64/pkgconfig:/usr/share/pkgconfig"
export PKG_CONFIG_PATH
PATH="/usr/versions/llvm/bin:$PATH"
export PATH
: "${CC:=gcc}"
: "${CXX:=g++}"
export CC CXX

TAR=$(pick gtar tar)
PATCH=$(pick gpatch patch)

# Overlays first (develop brings gcc, gmake, headers), then packages.
# The psi915 stack lives in $PREFIX; Tribblix's own Mesa 13.0.6 /
# xorg-server 1.16.4 stay untouched.
PSI_DEPS="
TRIBllvm
TRIBmeson-python-313
TRIBninja
TRIBpkgconf
TRIBgnu-patch
TRIBgnu-tar
TRIBpython-313-mako
TRIBpython-313-pyyaml
TRIBx11-libepoxy
TRIBx11-lib-libpciaccess
TRIBx11-lib-libxshmfence
TRIBglslang
TRIBspirv-tools
"
PSI_DEPS_CMD="zap install-overlay develop && zap install"
