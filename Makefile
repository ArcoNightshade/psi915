#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# psi915 top-level Makefile (GNU make).
#
#   make test        userland unit tests for the pure-logic KPI headers
#   make check-kmod  compile kernel sources against stub illumos headers
#                    (host-side sanity check, not a real kernel build)
#   make check       test + check-kmod
#   make kmod        build kernel modules (illumos only: gmake kmod [DEBUG=1])
#   make install     build, then install kernel modules (DESTDIR=..., DEBUG=1)
#   make clean
#

UNAME_S		:= $(shell uname -s)
DEBUG		= 0
JOBS		?= $(shell nproc 2>/dev/null || echo 2)
OBJS_DIR	= $(if $(filter 0,$(DEBUG)),obj64,debug64)

.PHONY: all help test check check-kmod kmod install clean

all: help

help:
	@sed -n '7,14p' Makefile | sed 's/^# \{0,1\}//'

test:
	$(MAKE) -C tests -j$(JOBS) run

check-kmod:
	$(MAKE) -C tests check-kmod

check: test check-kmod

kmod:
ifeq ($(UNAME_S),SunOS)
	$(MAKE) -C build all DEBUG=$(DEBUG)
else
	@echo "make kmod: kernel modules build only on illumos (uname -s = SunOS)."
	@echo "On this host use 'make check-kmod' for a stub-header compile check."
	@exit 1
endif

install: kmod
	scripts/install.sh -R "$(or $(DESTDIR),/)" -o $(OBJS_DIR)

clean:
	$(MAKE) -C tests clean
ifeq ($(UNAME_S),SunOS)
	$(MAKE) -C build clobber
endif
