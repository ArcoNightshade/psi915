# Userland reproducers for docs/12-review-findings.md. Each exits 1 while
# the bug it demonstrates is present ("make -k" runs them all).
TOP	= ..
CC	= gcc
CFLAGS	= -std=gnu11 -O1 -g -I$(TOP)/include -pthread -Wall -Wno-unused-parameter
LIB	= $(wildcard $(TOP)/src/linux_*.c) $(TOP)/src/plat_userland.c $(TOP)/src/plat_userland_dev.c
OUT	= out
REPROS	= repro_dma_sg_kva repro_gfp_huge_sleep

all: $(REPROS:%=run-%)

$(OUT):
	mkdir -p $@

$(OUT)/repro_dma_sg_kva: repro_dma_sg_kva.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< $(LIB) -Wl,--wrap=lkpi_plat_virt_to_phys

$(OUT)/%: %.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

run-%: $(OUT)/%
	./$<

clean:
	rm -rf $(OUT)

.PHONY: all clean
