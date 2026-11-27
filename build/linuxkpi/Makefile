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

# CTF label given with -l, not "-L VERSION" from Makefile.master, which
# needs a VERSION environment variable a plain gmake never sets (docs/13
# B1). Same as build/drm/Makefile.lkpi; lkpi_ktest and lkpi_toy still use
# Makefile.master's flags until it changes (docs/11 R-B2).
PSI915_REV	:= $(shell git -C $(SRCBASE) describe --always --dirty 2>/dev/null)
CTF_LABEL	= psi915$(if $(PSI915_REV),-$(PSI915_REV))
CTFCVTFLAGS	= -i -l $(CTF_LABEL)
CTFMRGFLAGS	= -l $(CTF_LABEL)

OBJECTS		= $(LINUXKPI_OBJS:%=$(OBJS_DIR)/%)

include $(BUILDBASE)/Makefile.rules
include $(BUILDBASE)/Makefile.targ
