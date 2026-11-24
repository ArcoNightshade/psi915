#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# Userland test harness for the LinuxKPI, plus compile checks of the kernel
# sources against stub illumos headers.
#
#   make run        build and run unit tests (gcc; also clang and
#                   ASan/UBSan variants when available)
#   make check-kmod compile src/, the kernel test module and the kernel
#                   config of include/linux against stubs/illumos, check
#                   exported symbols and that every undefined symbol is a
#                   known illumos kernel interface
#
# Each test program is linked with the userland build of the KPI sources
# (src/linux_*.c and src/plat_userland.c).
#
# Runs on Linux (and should run on illumos userland with gcc).
#

TOP		= ..
INCLUDE		= $(TOP)/include
SRC		= $(TOP)/src
OUT		= out

CC		= gcc
CLANG		= $(shell command -v clang 2>/dev/null)
CSTD		= -std=gnu11
WARN		= -Wall -Wextra -Werror -Wshadow -Wno-unused-parameter
TEST_CFLAGS	= $(CSTD) $(WARN) -O2 -g -I$(INCLUDE) -pthread
SAN_FLAGS	= -fsanitize=address,undefined -fno-sanitize-recover=all -O1
HDRS		= harness.h $(wildcard $(INCLUDE)/linux/*.h $(INCLUDE)/linuxkpi/*.h \
		  $(INCLUDE)/asm/*.h)

# KPI sources linked into every test program (userland configuration).
LIB_SRCS	= $(wildcard $(SRC)/linux_*.c) $(SRC)/plat_userland.c

TESTS		= test_list test_kref test_kernel test_atomic test_bitops test_printk test_slab test_locks test_list_sort test_rbtree test_rcu

# Kernel-mode compile check: kernel flags from build/Makefile.master minus
# -msave-args (illumos-patched gcc only).
KMOD_CFLAGS	= -std=gnu11 -fno-pic -fno-pie -m64 -mcmodel=kernel -mno-red-zone -ffreestanding \
		  -mno-mmx -mno-sse -fno-asynchronous-unwind-tables \
		  -mindirect-branch=thunk-extern -mindirect-branch-register \
		  -fno-stack-protector \
		  -O2 -Wall -Wextra -Werror -Wno-unused-parameter \
		  -Wno-missing-field-initializers -Wno-sign-compare \
		  -nostdinc -D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 \
		  -D_DDI_STRICT -Dsun -D__sun -D__SVR4 \
		  -I$(INCLUDE) -Istubs/illumos
# Sources of the linuxkpi module (build/Makefile.files LINUXKPI_OBJS).
KMOD_SRCS	= $(SRC)/linuxkpi_mod.c $(SRC)/plat_illumos.c $(wildcard $(SRC)/linux_*.c)
KMOD_OBJS	= $(KMOD_SRCS:$(SRC)/%.c=$(OUT)/kmod_%.o)
EXPECTED_EXPORTS = _fini _info _init linuxkpi_version
# Undefined symbols the linuxkpi objects may reference: illumos kernel
# interfaces only (no libc, no libgcc helpers).
KMOD_ALLOWED	= kmod_allowed_syms.txt

.PHONY: all run check-kmod clean

all: run

$(OUT):
	mkdir -p $@

$(OUT)/%.gcc: %.c $(HDRS) $(LIB_SRCS) | $(OUT)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(LIB_SRCS)

$(OUT)/%.san: %.c $(HDRS) $(LIB_SRCS) | $(OUT)
	$(CC) $(TEST_CFLAGS) $(SAN_FLAGS) -o $@ $< $(LIB_SRCS)

$(OUT)/%.clang: %.c $(HDRS) $(LIB_SRCS) | $(OUT)
	$(CLANG) $(TEST_CFLAGS) -o $@ $< $(LIB_SRCS)

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

$(OUT)/kmod_%.o: $(SRC)/%.c $(HDRS) FORCE | $(OUT)
	@echo "CC (stub illumos kernel headers) $<"
	@$(CC) $(KMOD_CFLAGS) $(KMOD_EXTRA) -c -o $@ $<

FORCE:

check-kmod: $(KMOD_OBJS) | $(OUT)
	@echo "CC (stub illumos kernel headers) kmod_headers_check.c"
	@$(CC) $(KMOD_CFLAGS) $(KMOD_EXTRA) -c -o $(OUT)/kmod_headers_check.o kmod_headers_check.c
	@got=$$(nm -g --defined-only $(OUT)/kmod_linuxkpi_mod.o | \
		awk '{print $$3}' | sort | tr '\n' ' ' | sed 's/ $$//'); \
	if [ "$$got" != "$(EXPECTED_EXPORTS)" ]; then \
		echo "linuxkpi_mod.o exports '$$got', expected '$(EXPECTED_EXPORTS)'"; \
		exit 1; \
	fi; \
	echo "linuxkpi_mod.o exports: $$got"
	@nm -g --defined-only $(KMOD_OBJS) $(OUT)/kmod_headers_check.o 2>/dev/null | awk 'NF==3 {print $$3}' | \
		sort -u > $(OUT)/kmod_defined.txt; \
	nm -u $(KMOD_OBJS) $(OUT)/kmod_headers_check.o 2>/dev/null | awk 'NF==2 {print $$2}' | sort -u | \
		comm -23 - $(OUT)/kmod_defined.txt > $(OUT)/kmod_undefined.txt; \
	grep -v '^#' $(KMOD_ALLOWED) | sed '/^$$/d' | sort -u > $(OUT)/kmod_allowed.txt; \
	bad=$$(comm -23 $(OUT)/kmod_undefined.txt $(OUT)/kmod_allowed.txt); \
	if [ -n "$$bad" ]; then \
		echo "linuxkpi objects reference symbols not in $(KMOD_ALLOWED):"; \
		echo "$$bad"; exit 1; \
	fi; \
	echo "linuxkpi undefined symbols: $$(wc -l < $(OUT)/kmod_undefined.txt) (all in $(KMOD_ALLOWED))"
	@echo "check-kmod: OK (stub headers only; this is NOT an illumos build)"

clean:
	rm -f out/*
	-rmdir out
