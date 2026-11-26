#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# linuxkpi_gpl_g2 misc module (GPL-2.0 combined work): upstream
# display/drm_hdcp_helper.c and drm_gem_framebuffer_helper.c (gpl/).
# Depends on drm (and G1, KPI); used by i915 only (gpl-policy check 5: drm/
# never calls G2). Installed as /usr/kernel/misc/amd64/linuxkpi_gpl_g2.
# UNTESTED ON ILLUMOS.
#

BUILDBASE	= ..

MODULE		= linuxkpi_gpl_g2
MODDEPS		= misc/linuxkpi misc/linuxkpi_gpl_g1 misc/linuxkpi_drm
LINUX_MODNAME	= drm_kms_helper
LKPI_LICENCE	= gpl

include Objects.mk

LINUX_SRCS	= $(GPL_G2_SRCS)
GLUE_SRCS	= build/linuxkpi_gpl_g2/gpl_g2_mod.c
MODULE_INC_LATE	= -I$(SRCBASE)/drm/drivers/gpu/drm

include ../drm/Makefile.lkpi
