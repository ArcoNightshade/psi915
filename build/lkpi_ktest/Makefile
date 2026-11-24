#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# This makefile drives the production of the lkpi_ktest misc module: the
# LinuxKPI unit tests (tests/test_*.c, built with -DLKPI_KTEST) run at
# _init. Development only: not installed by build/install.manifest. Load
# it from the build directory after installing linuxkpi:
#   modload build/lkpi_ktest/obj64/lkpi_ktest
#

BUILDBASE	= ..

MODULE		= lkpi_ktest
MODDEPS		= misc/linuxkpi
EXTRA_CPPFLAGS	= -DLKPI_KTEST -I$(SRCBASE)/tests
NO_INSTALL	= 1

include $(BUILDBASE)/Makefile.master
include $(BUILDBASE)/Makefile.files

OBJECTS		= $(LKPI_KTEST_OBJS:%=$(OBJS_DIR)/%)

include $(BUILDBASE)/Makefile.rules
include $(BUILDBASE)/Makefile.targ
