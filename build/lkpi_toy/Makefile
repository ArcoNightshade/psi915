#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# This makefile drives the production of the lkpi_toy driver module: the
# Linux-style toy_edu PCI driver (drivers/toy) for QEMU's edu device,
# the P2 exit artifact. Development only: not installed by
# build/install.manifest. To try it in a QEMU VM started with
# "-device edu", after installing linuxkpi:
#   cp build/lkpi_toy/obj64/lkpi_toy /usr/kernel/drv/amd64/
#   add_drv -i '"pci1234,11e8"' lkpi_toy
#   (console: "0000:00:..: edu 0x10000ed: live 1 irq 1 fact 3628800 ...")
#   rem_drv lkpi_toy
#

BUILDBASE	= ..

MODULE		= lkpi_toy
MODDEPS		= misc/linuxkpi
NO_INSTALL	= 1

include $(BUILDBASE)/Makefile.master
include $(BUILDBASE)/Makefile.files

OBJECTS		= $(LKPI_TOY_OBJS:%=$(OBJS_DIR)/%)

include $(BUILDBASE)/Makefile.rules
include $(BUILDBASE)/Makefile.targ
