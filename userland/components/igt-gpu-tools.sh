# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# igt-gpu-tools: validation ladder step 3 (gem_exec_*, kms_*, prime_*).
# DESIGN ONLY / EXPECTED NOT TO BUILD YET. UNTESTED ON ILLUMOS.
#
# igt is Linux-specific in its library layer (believed, from igt 1.2x meson
# files): hard dependencies on libudev, libkmod, libprocps/libproc2, libdw,
# and runtime use of /sys, /proc, debugfs (/sys/kernel/debug/dri/N), sysfs
# engine/RPS controls, and kernel module load/unload. The plan
# (docs/09-userland-stack.md, "igt") is a compat shim library
# (udev/kmod/procps stubs + debugfs/sysfs path redirection) in
# patches/igt-gpu-tools, building only lib/ and an allowlist of tests.
# OI ships intel-gpu-tools 1.3 (2012) which is unrelated to modern igt.

build_igt_gpu_tools() {
	src=$(fetch_component igt-gpu-tools) || exit 1
	warn "igt-gpu-tools: expected to fail on illumos until the compat shim exists"
	meson_build igt-gpu-tools "$src" \
	    $(mopt "$src" overlay disabled) \
	    $(mopt "$src" chamelium disabled) \
	    $(mopt "$src" valgrind disabled) \
	    $(mopt "$src" man disabled) \
	    $(mopt "$src" docs disabled) \
	    $(mopt "$src" runner disabled) \
	    $(mopt "$src" libunwind disabled) \
	    $(mopt "$src" testplan disabled) \
	    $(mopt "$src" sphinx disabled) \
	    $(mopt "$src" xe_driver disabled) \
	    $(mopt "$src" tests enabled)
}
