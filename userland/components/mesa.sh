# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Mesa: iris (Gallium GL/GLES for Gen8+), llvmpipe (software fallback),
# anv (Vulkan, "intel"), GBM + EGL + GLX (DRI3). UNTESTED ON ILLUMOS.
#
# Version policy (UNVERIFIED, check before the first build):
#  - versions.conf pins a 24.0.x release. Newer Mesa (believed 24.3/25.x and
#    later) compiles Intel internal shaders with mesa_clc/intel_clc, which
#    needs clang + libclc + SPIRV-LLVM-Translator on the build host. Neither
#    OI nor Tribblix is known to package libclc or SPIRV-LLVM-Translator.
#    Moving past 24.0 means either packaging those or using
#    -Dmesa-clc=system / -Dprecomp-compiler=system with host tools built
#    elsewhere. Options below are guarded with mopt so both lines work.
#  - Gen9 is supported by iris and anv in all versions considered here.
#    Do not build i965 (removed from Mesa in 22.0; iris replaces it).
#
# Set PSI_MESA_VULKAN=0 to skip anv (first bring-up: GL only).
# Set PSI_MESA_LLVM=0 to build without LLVM (llvmpipe becomes softpipe).

build_mesa() {
	src=$(fetch_component mesa) || exit 1
	vk=intel
	[ "${PSI_MESA_VULKAN:-1}" = 0 ] && vk=
	if [ "${PSI_MESA_LLVM:-1}" = 0 ]; then
		gallium=iris,softpipe; llvm=disabled
	else
		gallium=iris,llvmpipe; llvm=enabled
	fi
	meson_build mesa "$src" \
	    -Dgallium-drivers="$gallium" \
	    -Dvulkan-drivers="$vk" \
	    -Dplatforms=x11 \
	    -Dglx=dri \
	    -Degl=enabled \
	    -Dgbm=enabled \
	    -Dgles1=disabled \
	    -Dgles2=enabled \
	    -Dopengl=true \
	    -Dllvm="$llvm" \
	    $(mopt "$src" shared-llvm enabled) \
	    $(mopt "$src" glvnd disabled) \
	    $(mopt "$src" dri3 enabled) \
	    $(mopt "$src" dri-drivers '') \
	    $(mopt "$src" gallium-va disabled) \
	    $(mopt "$src" gallium-vdpau disabled) \
	    $(mopt "$src" gallium-xa disabled) \
	    $(mopt "$src" gallium-nine false) \
	    $(mopt "$src" gallium-opencl disabled) \
	    $(mopt "$src" gallium-rusticl false) \
	    $(mopt "$src" microsoft-clc disabled) \
	    $(mopt "$src" intel-clc disabled) \
	    $(mopt "$src" video-codecs '') \
	    $(mopt "$src" valgrind disabled) \
	    $(mopt "$src" libunwind disabled) \
	    $(mopt "$src" lmsensors disabled) \
	    $(mopt "$src" build-tests false) \
	    $(mopt "$src" vulkan-layers '') \
	    $(mopt "$src" tools '') \
	    $(mopt "$src" xmlconfig enabled) \
	    $(mopt "$src" zstd enabled) \
	    $(mopt "$src" shader-cache enabled)
}
