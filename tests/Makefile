#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Userland test harness for the pure-logic LinuxKPI headers, plus compile
# checks of the kernel sources against stub illumos headers.
#
#   make run        build and run unit tests (gcc; also clang and
#                   ASan/UBSan variants when available)
#   make check-kmod compile src/ and the kernel config of include/linux
#                   against stubs/illumos, and check exported symbols
#
# Runs on Linux (and should run on illumos userland with gcc).
#

TOP		= ..
INCLUDE		= $(TOP)/include
OUT		= out

CC		= gcc
CLANG		= $(shell command -v clang 2>/dev/null)
CSTD		= -std=gnu11
WARN		= -Wall -Wextra -Werror -Wshadow -Wno-unused-parameter
TEST_CFLAGS	= $(CSTD) $(WARN) -O2 -g -I$(INCLUDE) -pthread
SAN_FLAGS	= -fsanitize=address,undefined -fno-sanitize-recover=all -O1
HDRS		= harness.h $(wildcard $(INCLUDE)/linux/*.h $(INCLUDE)/linuxkpi/*.h)

TESTS		= test_list test_kref test_kernel

# Kernel-mode compile check: kernel flags from build/Makefile.master minus
# -msave-args (illumos-patched gcc only).
KMOD_CFLAGS	= -std=gnu11 -fno-pic -fno-pie -m64 -mcmodel=kernel -mno-red-zone -ffreestanding \
		  -mno-mmx -mno-sse -fno-asynchronous-unwind-tables \
		  -mindirect-branch=thunk-extern -mindirect-branch-register \
		  -O2 -Wall -Wextra -Werror -Wno-unused-parameter \
		  -Wno-missing-field-initializers -Wno-sign-compare \
		  -nostdinc -D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 \
		  -D_DDI_STRICT -Dsun -D__sun -D__SVR4 \
		  -I$(INCLUDE) -Istubs/illumos
KMOD_SRCS	= $(TOP)/src/linuxkpi_mod.c kmod_headers_check.c
EXPECTED_EXPORTS = _fini _info _init linuxkpi_version

.PHONY: all run check-kmod clean

all: run

$(OUT):
	mkdir -p $@

$(OUT)/%.gcc: %.c $(HDRS) | $(OUT)
	$(CC) $(TEST_CFLAGS) -o $@ $<

$(OUT)/%.san: %.c $(HDRS) | $(OUT)
	$(CC) $(TEST_CFLAGS) $(SAN_FLAGS) -o $@ $<

$(OUT)/%.clang: %.c $(HDRS) | $(OUT)
	$(CLANG) $(TEST_CFLAGS) -o $@ $<

# ASan/UBSan need compiler runtime support; skip the variant if it cannot link.
SAN_OK		:= $(shell echo 'int main(void){return 0;}' | \
		     $(CC) -x c $(SAN_FLAGS) -o /dev/null - 2>/dev/null && echo yes)

VARIANTS	= gcc $(if $(SAN_OK),san) $(if $(CLANG),clang)
BINS		= $(foreach v,$(VARIANTS),$(TESTS:%=$(OUT)/%.$(v)))

run: $(BINS)
	@fail=0; for t in $(BINS); do \
		echo "== $$t"; ./$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "UNIT TESTS FAILED"; exit 1; fi; \
	echo "all unit tests passed ($(VARIANTS))"

check-kmod: | $(OUT)
	@for s in $(KMOD_SRCS); do \
		o=$(OUT)/kmod_$$(basename $$s .c).o; \
		echo "CC (stub illumos kernel headers) $$s"; \
		$(CC) $(KMOD_CFLAGS) $(KMOD_EXTRA) -c -o $$o $$s || exit 1; \
	done
	@got=$$(nm -g --defined-only $(OUT)/kmod_linuxkpi_mod.o | \
		awk '{print $$3}' | sort | tr '\n' ' ' | sed 's/ $$//'); \
	if [ "$$got" != "$(EXPECTED_EXPORTS)" ]; then \
		echo "linuxkpi_mod.o exports '$$got', expected '$(EXPECTED_EXPORTS)'"; \
		exit 1; \
	fi; \
	echo "linuxkpi_mod.o exports: $$got"
	@echo "check-kmod: OK (stub headers only; this is NOT an illumos build)"

clean:
	rm -f out/*
	-rmdir out
