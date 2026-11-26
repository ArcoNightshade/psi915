# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Xorg with the in-tree modesetting driver ("xf86-video-modesetting" has been
# part of xorg-server since 1.17), glamor, DRI2, DRI3 and Present.
# Validation ladder step 2. UNTESTED ON ILLUMOS.
#
# Installed under $PREFIX with its own module dir, so it does not disturb the
# distribution's X server (OI: 1.19.7 with glamor disabled; Tribblix: 1.16.4).
# Start it explicitly, e.g. as root on a free VT:
#   $PREFIX/bin/Xorg :1 -config $PSI_USERLAND/config/xorg.conf.d/20-modesetting.conf
#
# Non-udev host: no platform bus, so modesetting probes through libpciaccess
# and opens the device by bus id (drmOpen -> DRM_IOCTL_SET_VERSION +
# DRM_IOCTL_GET_UNIQUE == "pci:0000:00:02.0"). The kernel must support both
# ioctls (believed true of modern i915/DRM; verify in the port).
#
# The OI patch set for 1.19.7 (oi-userland components/x11/xorg-server/patches,
# 40 patches: Solaris paths, VT handling, illumosfb autoconfig, audit, ...)
# needs reviewing and forward-porting to 21.1; see patches/xorg-server/series.

build_xorg_server() {
	src=$(fetch_component xorg-server) || exit 1
	meson_build xorg-server "$src" \
	    -Dxorg=true \
	    -Dglamor=true \
	    -Ddri1=false \
	    -Ddri2=true \
	    -Ddri3=true \
	    -Dglx=true \
	    -Dxvfb=true \
	    -Dxnest=false \
	    -Dxephyr=false \
	    -Dxwin=false \
	    -Dudev=false \
	    -Dudev_kms=false \
	    -Dsystemd_logind=false \
	    -Dhal=false \
	    -Dpciaccess=true \
	    -Dsuid_wrapper=false \
	    -Dsecure-rpc=false \
	    -Dlinux_apm=false \
	    -Dlinux_acpi=false \
	    -Dint10=false \
	    -Dxkb_output_dir=/var/run/xkb \
	    -Dxkb_bin_dir=/usr/bin \
	    -Dxkb_dir=/usr/share/X11/xkb \
	    -Ddefault_font_path=catalogue:/etc/X11/fontpath.d \
	    -Dlog_dir=/var/log \
	    -Dmodule_dir="$PREFIX/$PSI_LIBDIR/xorg/modules" \
	    $(mopt "$src" sha1 libmd) \
	    $(mopt "$src" docs false) \
	    $(mopt "$src" devel-docs false)
}

# mesa-demos for glxgears / glxinfo -B (expects "iris" in the renderer).
build_mesa_demos() {
	src=$(fetch_component mesa-demos) || exit 1
	meson_build mesa-demos "$src" \
	    $(mopt "$src" gles1 disabled) \
	    $(mopt "$src" osmesa disabled) \
	    $(mopt "$src" wayland disabled) \
	    $(mopt "$src" vulkan disabled) \
	    $(mopt "$src" libdrm enabled) \
	    $(mopt "$src" x11 enabled) \
	    $(mopt "$src" egl enabled)
}
