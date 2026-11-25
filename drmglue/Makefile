#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# drmglue: illumos DRM character device, GEM mmap (devmap) and dma-buf
# glue. See docs/10-mmap-dmabuf-design.md.
#
#   make test        userland unit tests of the portable core against the
#                    simulation backend (gcc, ASan/UBSan, clang if present)
#   make check-kmod  compile every source, including the illumos backends,
#                    with kernel flags against drmglue/stubs (NOT a kernel
#                    build), and check undefined symbols
#   make check       both
#

TOP		= ..
KPI_INC		= $(TOP)/include
KPI_SRC		= $(TOP)/src
OUT		= out

CC		= gcc
CLANG		= $(shell command -v clang 2>/dev/null)
WARN		= -Wall -Wextra -Werror -Wshadow -Wno-unused-parameter \
		  -Wno-missing-field-initializers
INCS		= -Iinclude -I$(KPI_INC)
TEST_CFLAGS	= -std=gnu11 $(WARN) -O2 -g $(INCS) -Itests -Itests/fakes \
		  -I$(TOP)/tests -pthread
SAN_FLAGS	= -fsanitize=address,undefined -fno-sanitize-recover=all -O1

CORE_SRCS	= src/xlate.c src/itree.c src/vmoff.c src/file.c src/vma.c \
		  src/anonfile.c src/cdev.c src/dma_buf.c src/sync_file.c
ILLUMOS_SRCS	= src/cdev_illumos.c src/segdev_illumos.c src/anonfs_illumos.c
KPI_LIB		= $(wildcard $(KPI_SRC)/linux_*.c) $(KPI_SRC)/plat_userland.c
TEST_LIB	= tests/sim_backend.c $(CORE_SRCS) $(KPI_LIB)
HDRS		= $(wildcard include/*/*.h include/linux/*.h tests/*.h \
		  tests/fakes/linux/*.h $(KPI_INC)/linux/*.h $(KPI_INC)/linuxkpi/*.h)

TESTS		= test_xlate test_vmoff test_vma test_anonfile test_cdev

SAN_OK		:= $(shell echo 'int main(void){return 0;}' | \
		     $(CC) -x c $(SAN_FLAGS) -o /dev/null - 2>/dev/null && echo yes)
VARIANTS	= gcc $(if $(SAN_OK),san) $(if $(CLANG),clang)
BINS		= $(foreach v,$(VARIANTS),$(TESTS:%=$(OUT)/%.$(v)))

KMOD_CFLAGS	= -std=gnu11 -fno-pic -fno-pie -m64 -mcmodel=kernel -mno-red-zone \
		  -ffreestanding -mno-mmx -mno-sse -fno-asynchronous-unwind-tables \
		  -fno-stack-protector -O2 -Wall -Wextra -Werror -Wno-unused-parameter \
		  -Wno-missing-field-initializers -Wno-sign-compare \
		  -nostdinc -D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 \
		  -D_DDI_STRICT -Dsun -D__sun -D__SVR4 \
		  $(INCS) -Itests/fakes -Istubs/illumos
KMOD_OBJS	= $(CORE_SRCS:src/%.c=$(OUT)/kmod_%.o) \
		  $(ILLUMOS_SRCS:src/%.c=$(OUT)/kmod_%.o)
ALLOWED		= kmod_allowed_syms.txt

.PHONY: all test check check-kmod clean

all: check

check: test check-kmod

$(OUT):
	mkdir -p $@

$(OUT)/%.gcc: tests/%.c $(TEST_LIB) $(HDRS) | $(OUT)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LIB)

$(OUT)/%.san: tests/%.c $(TEST_LIB) $(HDRS) | $(OUT)
	$(CC) $(TEST_CFLAGS) $(SAN_FLAGS) -o $@ $< $(TEST_LIB)

$(OUT)/%.clang: tests/%.c $(TEST_LIB) $(HDRS) | $(OUT)
	$(CLANG) $(TEST_CFLAGS) -o $@ $< $(TEST_LIB)

test: $(BINS)
	@fail=0; for t in $(BINS); do \
		echo "== $$t"; ./$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "DRMGLUE TESTS FAILED"; exit 1; fi; \
	echo "drmglue: all unit tests passed ($(VARIANTS))"

$(OUT)/kmod_%.o: src/%.c $(HDRS) $(wildcard stubs/illumos/*/*.h) | $(OUT)
	@echo "CC (stub illumos kernel headers) $<"
	@$(CC) $(KMOD_CFLAGS) -c -o $@ $<

check-kmod: $(KMOD_OBJS)
	@nm -g --defined-only $(KMOD_OBJS) | awk 'NF==3 {print $$3}' | \
		sort -u > $(OUT)/defined.txt; \
	nm -u $(KMOD_OBJS) | awk 'NF==2 {print $$2}' | sort -u | \
		comm -23 - $(OUT)/defined.txt > $(OUT)/undefined.txt; \
	grep -v '^#' $(ALLOWED) | awk '{print $$1}' | sed '/^$$/d' | sort -u \
		> $(OUT)/allowed.txt; \
	bad=$$(comm -23 $(OUT)/undefined.txt $(OUT)/allowed.txt); \
	if [ -n "$$bad" ]; then \
		echo "drmglue objects reference symbols not in $(ALLOWED):"; \
		echo "$$bad"; exit 1; \
	fi; \
	echo "drmglue undefined symbols: $$(wc -l < $(OUT)/undefined.txt) (all in $(ALLOWED))"
	@echo "check-kmod: OK (stub headers only; this is NOT an illumos build)"

clean:
	rm -rf $(OUT)
