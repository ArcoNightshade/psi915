/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * P2 exit test: the toy_edu driver (drivers/toy/toy_edu.c), unmodified,
 * probes a simulated QEMU edu device through the KPI's PCI, IRQ and DMA
 * layers in userland. The simulator thread watches the BAR registers,
 * performs DMA through the fake IOMMU (bus addresses only) and raises
 * MSI interrupts. Userland only: in the kernel the driver runs against
 * the real (QEMU) device as the lkpi_toy kmod.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linuxkpi/subsys.h>

#include "harness.h"

#ifndef LKPI_KTEST

#include <linuxkpi/fakedev.h>
#include "../drivers/toy/toy_edu.h"

#define	BAR0_SIZE	(1U << 20)

static struct lkpi_fake_pci edu;
static u8 edu_bar[BAR0_SIZE] __attribute__((__aligned__(4096)));
static u8 edu_mem[4096];
static volatile int sim_stop;

static u32
rd32(int off)
{
	return (__atomic_load_n((u32 *)__builtin_assume_aligned(&edu_bar[off], 4), __ATOMIC_SEQ_CST));
}

static void
wr32(int off, u32 v)
{
	__atomic_store_n((u32 *)__builtin_assume_aligned(&edu_bar[off], 4), v, __ATOMIC_SEQ_CST);
}

static u64
rd64(int off)
{
	return (__atomic_load_n((u64 *)__builtin_assume_aligned(&edu_bar[off], 8), __ATOMIC_SEQ_CST));
}

static void
wr64(int off, u64 v)
{
	__atomic_store_n((u64 *)__builtin_assume_aligned(&edu_bar[off], 8), v, __ATOMIC_SEQ_CST);
}

static u32 irq_status;

static void
sim_irq(u32 bits)
{
	__atomic_fetch_or(&irq_status, bits, __ATOMIC_SEQ_CST);
	wr32(0x24, __atomic_load_n(&irq_status, __ATOMIC_SEQ_CST));
	(void) lkpi_fake_raise_irq(&edu, 0);
}

static void
edu_sim(void *arg)
{
	u32 live_seen = 0, fact_seen = 0, v;

	(void) arg;
	while (!sim_stop) {
		v = rd32(0x04);
		if (v != live_seen && v != ~live_seen) {
			live_seen = ~v;
			wr32(0x04, ~v);
		}
		v = rd32(0x08);
		if (v != fact_seen) {
			u32 n = v, f = 1;

			while (n > 1)
				f *= n--;
			fact_seen = f;
			wr32(0x08, f);
			if (rd32(0x20) & 0x80)
				sim_irq(0x1);
		}
		v = rd32(0x64);		/* ack */
		if (v != 0) {
			__atomic_fetch_and(&irq_status, ~v, __ATOMIC_SEQ_CST);
			wr32(0x24, __atomic_load_n(&irq_status,
			    __ATOMIC_SEQ_CST));
			wr32(0x64, 0);
		}
		v = rd32(0x60);		/* raise */
		if (v != 0) {
			wr32(0x60, 0);
			sim_irq(v);
		}
		if (rd64(0x98) & 1) {
			u64 src = rd64(0x80), dst = rd64(0x88), cnt = rd64(0x90);
			u64 cmd = rd64(0x98);
			void *ram;

			if (cmd & 2) {	/* device -> RAM */
				ram = lkpi_fake_dma_kva(dst, cnt);
				if (ram != NULL && src >= 0x40000 &&
				    src - 0x40000 + cnt <= sizeof (edu_mem))
					memcpy(ram, edu_mem + (src - 0x40000),
					    cnt);
			} else {
				ram = lkpi_fake_dma_kva(src, cnt);
				if (ram != NULL && dst >= 0x40000 &&
				    dst - 0x40000 + cnt <= sizeof (edu_mem))
					memcpy(edu_mem + (dst - 0x40000), ram,
					    cnt);
			}
			wr64(0x98, cmd & ~1ULL);
			if (ram != NULL && (cmd & 4))
				sim_irq(0x100);
		}
		lkpi_plat_sleep_ns(20000);
	}
}

static void
edu_setup(void)
{
	memset(&edu, 0, sizeof (edu));
	memset(edu_bar, 0, sizeof (edu_bar));
	edu.name = "edu";
	edu.info.vendor = TOY_EDU_VENDOR;
	edu.info.device = TOY_EDU_DEVICE;
	edu.info.subvendor = 0x1af4;
	edu.info.subdevice = 0x1100;
	edu.info.class = 0x00ff00;
	edu.info.revision = 0x10;
	edu.info.bus = 0;
	edu.info.devfn = PCI_DEVFN(4, 0);
	edu.info.bar[0].start = 0xfea00000;
	edu.info.bar[0].len = BAR0_SIZE;
	edu.info.bar[0].flags = LKPI_BAR_MEM;
	edu.info.bar[0].rnumber = 1;
	edu.bar_mem[0] = edu_bar;
	edu.intr_types = LKPI_INTR_MSI | LKPI_INTR_INTX;
	edu.max_vec = 1;
	/* Config space: ids, status with capability list, MSI cap at 0x40. */
	memcpy(&edu.cfg[0], &edu.info.vendor, 2);
	memcpy(&edu.cfg[2], &edu.info.device, 2);
	edu.cfg[6] = 0x10;
	edu.cfg[0x34] = 0x40;
	edu.cfg[0x40] = PCI_CAP_ID_MSI;
	edu.cfg[0x41] = 0;
	wr32(0x00, 0x010000ed);
	lkpi_fake_pci_add(&edu);
}

TEST(toy_probe_remove)
{
	struct lkpi_plat_thread *sim;
	struct pci_dev *pdev;
	int dma_before = lkpi_fake_dma_live();
	u16 cmd;

	edu_setup();
	sim_stop = 0;
	sim = lkpi_plat_thread_create(edu_sim, NULL, "edu_sim");
	CHECK(sim != NULL);

	CHECK_EQ(lkpi_initcall_toy_init(), 0);
	/* What attach(9E) does: wrap the node, match, probe. */
	pdev = lkpi_pci_dev_alloc(&edu);
	CHECK(pdev != NULL);
	if (pdev == NULL)
		return;
	CHECK_EQ(strcmp(pci_name(pdev), "0000:00:04.0"), 0);
	CHECK(pdev->msi_cap == 0x40);
	CHECK_EQ(lkpi_pci_probe(pdev), 0);
	CHECK(toy_last_result.probed);
	CHECK_EQ(toy_last_result.selftest, 0);
	CHECK_EQ(toy_last_result.id, 0x010000ed);
	CHECK(toy_last_result.live_ok);
	CHECK(toy_last_result.irq_ok);
	CHECK(toy_last_result.fact_ok);
	CHECK_EQ(toy_last_result.fact, 3628800);
	CHECK(toy_last_result.dma_ok);
	CHECK(toy_last_result.pool_ok);
	CHECK(toy_last_result.irqs >= 5);
	CHECK(pdev->msi_enabled);
	CHECK(pdev->irq > 0);
	/*
	 * The fake backend counts a claimed interrupt after the handler
	 * returns, which can lag the driver's own count on a loaded host.
	 */
	{
		int waited;

		for (waited = 0; READ_ONCE(edu.claimed) < 5 && waited < 2000;
		    waited++)
			msleep(1);
	}
	CHECK(edu.claimed >= 5);
	CHECK_EQ(pci_read_config_word(pdev, PCI_COMMAND, &cmd), 0);
	CHECK(cmd & PCI_COMMAND_MASTER);
	CHECK(cmd & PCI_COMMAND_MEMORY);

	lkpi_pci_remove(pdev);
	CHECK(toy_last_result.removed);
	CHECK_EQ(edu.vec_alloc, 0);		/* vectors freed */
	CHECK_EQ(lkpi_fake_dma_live(), dma_before);	/* no DMA leaks */
	CHECK_EQ(pci_read_config_word(pdev, PCI_COMMAND, &cmd), 0);
	CHECK(!(cmd & PCI_COMMAND_MASTER));	/* pcim disable ran */
	CHECK(list_empty(&pdev->lkpi_iomaps));	/* pcim iomap released */
	pci_dev_put(pdev);
	lkpi_exitcall_toy_exit();

	sim_stop = 1;
	if (sim != NULL)
		lkpi_plat_thread_join(sim);
	lkpi_fake_pci_remove(&edu);
}

#endif /* !LKPI_KTEST */

LKPI_SUITE(toy)
{
#ifndef LKPI_KTEST
	CHECK_EQ(lkpi_workqueue_init(), 0);
	RUN(toy_probe_remove);
	lkpi_workqueue_fini();
#endif
}
