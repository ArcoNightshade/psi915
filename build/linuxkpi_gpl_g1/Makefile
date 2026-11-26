#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# linuxkpi_gpl_g1 misc module (GPL-2.0 combined work): upstream dma-buf.c,
# dma-fence.c, dma-fence-array.c, dma-fence-chain.c, dma-fence-unwrap.c,
# sync_file.c (gpl/), the MIT dma-resv.c (drm/) and, with DRMGLUE_IN=g1
# (default, see build/drm/Makefile.lkpi), drmglue/. Depends only on
# misc/linuxkpi (gfx_private and acpica come in through linuxkpi).
# Installed as /usr/kernel/misc/amd64/linuxkpi_gpl_g1. UNTESTED ON ILLUMOS.
#

BUILDBASE	= ..

MODULE		= linuxkpi_gpl_g1
MODDEPS		= misc/linuxkpi $(if $(filter drm,$(DRMGLUE_IN)),misc/linuxkpi_drm)
LINUX_MODNAME	= dma_buf
LKPI_LICENCE	= kpi

include Objects.mk
include ../drm/Objects.mk

LINUX_SRCS	= $(GPL_G1_SRCS) $(GPL_G1_MIT_SRCS)
GLUE_SRCS	= build/linuxkpi_gpl_g1/gpl_g1_mod.c \
		  $(if $(filter g1,$(DRMGLUE_IN)),$(DRMGLUE_SRCS))
MODULE_DEFS	= $(if $(filter g1,$(DRMGLUE_IN)),-DPSI915_DRMGLUE_HERE)

include ../drm/Makefile.lkpi
