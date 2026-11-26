#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Builds every kernel module. Run on illumos with GNU make:
#   gmake            non-DEBUG (obj64)
#   gmake DEBUG=1    DEBUG (debug64)
#   gmake install    scripts/install-all.sh (ROOT=/ by default; see -R)
#
# Order matters (docs/11 R-B1): linuxkpi, then the graphics stack in
# dependency order: linuxkpi_gpl_g1, linuxkpi_drm (directory drm),
# linuxkpi_gpl_g2, i915. The test modules come last.
#

SUBDIRS		= linuxkpi linuxkpi_gpl_g1 drm linuxkpi_gpl_g2 i915 \
		  lkpi_ktest lkpi_toy
ROOT		?= /
OBJDIR		= $(if $(filter 1,$(DEBUG)),debug64,obj64)

.PHONY: all install clean clobber $(SUBDIRS)

all clean clobber:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@ || exit 1; \
	done

# The module set through the installer; the test modules keep their own
# install targets.
install: all
	sh ../scripts/install-all.sh -R $(ROOT) -o $(OBJDIR)
	@for d in lkpi_ktest lkpi_toy; do \
		$(MAKE) -C $$d install || exit 1; \
	done
