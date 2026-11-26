# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# libdrm with the Intel backend (libdrm_intel) and the test programs
# (modetest, proptest, vbltest), which are the first KMS smoke tests before
# kmscube. UNTESTED ON ILLUMOS.
#
# illumos notes (see docs/09-userland-stack.md):
#  - drm.h takes its non-Linux branch and includes <sys/ioccom.h>: ioctl
#    numbers use the native illumos encoding; the kernel decodes that.
#  - Device enumeration (drmGetDevice2, drmGetMinorType, drmNodeIsDRM) needs
#    the illumos patches listed in patches/libdrm/series (libdevinfo instead
#    of sysfs, clone-minor masking, no fixed DRM_MAJOR).
#  - libdrm_intel needs libpciaccess (packaged on both distributions).

build_libdrm() {
	src=$(fetch_component libdrm) || exit 1
	meson_build libdrm "$src" \
	    $(mopt "$src" intel enabled) \
	    $(mopt "$src" radeon disabled) \
	    $(mopt "$src" amdgpu disabled) \
	    $(mopt "$src" nouveau disabled) \
	    $(mopt "$src" vmwgfx disabled) \
	    $(mopt "$src" omap disabled) \
	    $(mopt "$src" exynos disabled) \
	    $(mopt "$src" freedreno disabled) \
	    $(mopt "$src" tegra disabled) \
	    $(mopt "$src" vc4 disabled) \
	    $(mopt "$src" etnaviv disabled) \
	    $(mopt "$src" cairo-tests disabled) \
	    $(mopt "$src" man-pages disabled) \
	    $(mopt "$src" valgrind disabled) \
	    $(mopt "$src" udev false) \
	    $(mopt "$src" tests true) \
	    $(mopt "$src" install-test-programs true)
}
