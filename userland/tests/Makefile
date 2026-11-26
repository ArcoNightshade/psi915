#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# uAPI conformance check (GNU make; gmake on illumos).
#
#   make check     build kernel-side and user-side uapi_dump, run both, compare
#   make check32   also compare LP64 vs ILP32 layouts (needs a working -m32)
#   make check-sim-illumos  Linux host: user side built with illumos <sys/ioccom.h>
#   make regen     regenerate uapi_list.h from the current header sets
#   make clean
#
# Header sets (override on the command line):
#   KHDR_DRM     kernel-side uapi/drm dir. Default: the project's drm/ import if
#                present, else $(REF)/drm-kmod/include/uapi/drm.
#   KHDR_SYNC    dir containing linux/sync_file.h (kernel side)
#   KHDR_DMABUF  dir containing linux/dma-buf.h (kernel side)
#   UHDR_DRM     user-side dir containing drm.h and i915_drm.h as libdrm installs
#                them. Linux: /usr/include/drm (kernel uapi, which libdrm copies)
#                or <prefix>/include/libdrm. illumos: /opt/psi915/include/libdrm.
#   UHDR_EXTRA   extra user-side include dir that has linux/sync_file.h and
#                linux/dma-buf.h, e.g. a Mesa tree's include/drm-uapi (illumos).
#
# Status: passes on Linux (gcc, glibc). Untested on illumos.

UNAME_S		:= $(shell uname -s)
CC		?= gcc
CFLAGS		?= -O1 -g
WARN		= -Wall -Wextra -std=gnu11
OUT		= out

REF		?= $(HOME)/ref
TOP		= ../..
KHDR_DRM	?= $(if $(wildcard $(TOP)/drm/include/uapi/drm/drm.h),$(TOP)/drm/include/uapi/drm,$(REF)/drm-kmod/include/uapi/drm)
KHDR_SYNC	?= $(REF)/drm-kmod/linuxkpi/gplv2/include/uapi
KHDR_DMABUF	?= $(REF)/drm-kmod/linuxkpi/bsd/include/uapi

ifeq ($(UNAME_S),SunOS)
UHDR_DRM	?= /opt/psi915/include/libdrm
UHDR_EXTRA	?=
USER_COMPAT	= -I../compat/include
else
UHDR_DRM	?= /usr/include/drm
UHDR_EXTRA	?=
USER_COMPAT	=
endif

K_HAVE		= $(if $(wildcard $(KHDR_SYNC)/linux/sync_file.h),-DUAPI_HAVE_SYNC_FILE) \
		  $(if $(wildcard $(KHDR_DMABUF)/linux/dma-buf.h),-DUAPI_HAVE_DMA_BUF)
K_CPPFLAGS	= -DUAPI_SIDE_KERNEL -D__KERNEL__ -Icompat/kpi -I$(KHDR_DRM) \
		  -I$(KHDR_SYNC) -I$(KHDR_DMABUF) -I. $(K_HAVE)

# User side: sync_file.h/dma-buf.h from UHDR_EXTRA, else the host's.
U_SYNC		= $(firstword $(wildcard $(UHDR_EXTRA)/linux/sync_file.h /usr/include/linux/sync_file.h))
U_DMABUF	= $(firstword $(wildcard $(UHDR_EXTRA)/linux/dma-buf.h /usr/include/linux/dma-buf.h))
U_HAVE		= $(if $(U_SYNC),-DUAPI_HAVE_SYNC_FILE) $(if $(U_DMABUF),-DUAPI_HAVE_DMA_BUF)
U_CPPFLAGS	= -DUAPI_SIDE_USER -I$(UHDR_DRM) $(if $(UHDR_EXTRA),-I$(UHDR_EXTRA)) \
		  $(USER_COMPAT) -I. $(U_HAVE)

SRC		= uapi_dump.c uapi_list.h

.PHONY: all check check32 check-sim-illumos regen clean

all: check

$(OUT):
	mkdir -p $(OUT)

$(OUT)/dump-kernel: $(SRC) | $(OUT)
	@test -r $(KHDR_DRM)/i915_drm.h || { echo "missing $(KHDR_DRM)/i915_drm.h: set KHDR_DRM= or REF="; exit 2; }
	$(CC) $(WARN) $(CFLAGS) $(K_CPPFLAGS) uapi_dump.c -o $@

$(OUT)/dump-user: $(SRC) | $(OUT)
	@test -r $(UHDR_DRM)/i915_drm.h || { echo "missing $(UHDR_DRM)/i915_drm.h: set UHDR_DRM="; exit 2; }
	$(CC) $(WARN) $(CFLAGS) $(U_CPPFLAGS) uapi_dump.c -o $@

check: $(OUT)/dump-kernel $(OUT)/dump-user
	./$(OUT)/dump-kernel > $(OUT)/kernel.txt
	./$(OUT)/dump-user > $(OUT)/user.txt
	-awk -f uapi_cmp.awk $(OUT)/kernel.txt $(OUT)/user.txt > $(OUT)/compare.txt
	@grep -v '^INFO' $(OUT)/compare.txt || true
	@grep -q '^# result: 0 FAIL' $(OUT)/compare.txt

# Linux host only: build the user side as if on illumos (native <sys/ioccom.h>
# from the illumos-gate reference tree, __sun defined, compat <linux/*.h>) to
# exercise the illumos encoding path before real hardware exists. The
# reference ioccom.h is copied into $(OUT) at build time, never committed.
IL_IOCCOM	?= $(REF)/illumos/usr/src/uts/common/sys/ioccom.h
check-sim-illumos: $(OUT)/dump-kernel
	@test -r $(IL_IOCCOM) || { echo "missing $(IL_IOCCOM): set IL_IOCCOM= or REF="; exit 2; }
	mkdir -p $(OUT)/simil/sys $(OUT)/simil/uapi/linux
	cp $(IL_IOCCOM) $(OUT)/simil/sys/ioccom.h
	$(if $(U_SYNC),cp $(U_SYNC) $(OUT)/simil/uapi/linux/)
	$(if $(U_DMABUF),cp $(U_DMABUF) $(OUT)/simil/uapi/linux/)
	$(CC) $(WARN) -Wno-parentheses $(CFLAGS) -U__linux__ -D__sun -DUAPI_SIDE_USER \
	    -I$(OUT)/simil -I$(UHDR_DRM) -I$(OUT)/simil/uapi -I../compat/include -I. \
	    $(U_HAVE) uapi_dump.c -o $(OUT)/dump-simil
	./$(OUT)/dump-kernel > $(OUT)/kernel.txt
	./$(OUT)/dump-simil > $(OUT)/simil.txt
	-awk -f uapi_cmp.awk $(OUT)/kernel.txt $(OUT)/simil.txt > $(OUT)/compare-simil.txt
	@grep -v '^INFO' $(OUT)/compare-simil.txt || true
	@awk -F'\t' '$$1=="E" && $$7!=$$8 {n++} $$1=="E" {t++} END {printf("# %d of %d ioctl numbers differ between Linux and illumos encodings\n", n, t)}' $(OUT)/simil.txt
	@grep -q '^# result: 0 FAIL' $(OUT)/compare-simil.txt

# ILP32: 32-bit processes on the amd64 kernel. Skipped if -m32 cannot link.
check32: check
	@if printf 'int main(void){return 0;}\n' | $(CC) -m32 -x c - -o $(OUT)/m32probe 2>/dev/null; then \
	    $(CC) -m32 $(WARN) $(CFLAGS) $(K_CPPFLAGS) uapi_dump.c -o $(OUT)/dump-kernel32 && \
	    ./$(OUT)/dump-kernel32 > $(OUT)/kernel32.txt && \
	    awk -v mode=abi -f uapi_cmp.awk $(OUT)/kernel.txt $(OUT)/kernel32.txt \
	        > $(OUT)/compare32.txt; \
	    grep -v '^INFO' $(OUT)/compare32.txt; \
	    echo "(ILP32 differences are expected: each FAIL line is a compat conversion the kernel needs)"; \
	else echo "check32: skipped, $(CC) -m32 cannot build executables on this host"; fi

# Regenerate the entry list from the union of the kernel and user header sets.
regen:
	sh gen-uapi-list.sh \
	    $(KHDR_DRM)/drm.h $(KHDR_DRM)/drm_mode.h $(KHDR_DRM)/i915_drm.h \
	    $(wildcard $(UHDR_DRM)/drm.h $(UHDR_DRM)/drm_mode.h $(UHDR_DRM)/i915_drm.h) \
	    $(wildcard $(KHDR_SYNC)/linux/sync_file.h $(KHDR_DMABUF)/linux/dma-buf.h) \
	    $(U_SYNC) $(U_DMABUF) > uapi_list.h.new
	mv uapi_list.h.new uapi_list.h

clean:
	rm -rf $(OUT)
