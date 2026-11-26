# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# kmscube: KMS + GBM + EGL/GLES2, no X. Validation ladder step 1.
# Run: kmscube -D /dev/dri/card0          (legacy SETCRTC + PAGE_FLIP)
#      kmscube -D /dev/dri/card0 -A       (atomic modesetting)
# UNTESTED ON ILLUMOS.

build_kmscube() {
	src=$(fetch_component kmscube) || exit 1
	meson_build kmscube "$src" $(mopt "$src" gstreamer disabled)
}
