#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# This makefile drives the production of the linuxkpi misc module.
# It is a misc module (modlmisc), not a driver, so it has no .conf file.
# Installed as /usr/kernel/misc/amd64/linuxkpi (see build/install.manifest).
#

BUILDBASE	= ..

MODULE		= linuxkpi
MODDEPS		= misc/acpica misc/gfx_private drv/rootnex

include $(BUILDBASE)/Makefile.master
include $(BUILDBASE)/Makefile.files

# CTF label: psi915-<git describe>, from Makefile.master (docs/13 B1,
# docs/11 R-B2).

OBJECTS		= $(LINUXKPI_OBJS:%=$(OBJS_DIR)/%)

include $(BUILDBASE)/Makefile.rules
include $(BUILDBASE)/Makefile.targ
