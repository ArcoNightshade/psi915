#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Builds every kernel module. Run on illumos with GNU make:
#   gmake            non-DEBUG (obj64)
#   gmake DEBUG=1    DEBUG (debug64)
#

SUBDIRS		= linuxkpi

.PHONY: all install clean clobber $(SUBDIRS)

all install clean clobber:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@ || exit 1; \
	done
