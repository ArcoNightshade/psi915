/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * toy_edu: the P2 exit artifact. A Linux-style PCI driver, written only
 * against the KPI headers, for QEMU's "edu" educational device
 * (vendor 0x1234, device 0x11e8; QEMU docs/specs/edu.rst), so it can run
 * in an OpenIndiana VM started with "-device edu". It exercises:
 *
 *   BAR      pcim_enable_device, pcim_iomap of BAR0, readl/writel
 *            (identification and liveness registers)
 *   IRQ      pci_alloc_irq_vectors (MSI, INTx fallback), request_irq,
 *            a software-raised interrupt and a factorial-done interrupt,
 *            completions
 *   DMA      dma_set_mask_and_coherent (the device decodes 28 bits),
 *            dma_alloc_coherent (RAM -> device), alloc_page +
 *            dma_map_page (device -> RAM, streaming), a dma_pool block,
 *            and a full round trip compared byte for byte
 *   devres   devm_kzalloc, pcim_* unwinding on remove or failure
 *
 * The same source is the userland unit test's driver (tests/test_toy.c,
 * against a simulated edu device) and the lkpi_toy illumos kmod
 * (drivers/toy/toy_ddi.c, build/lkpi_toy). Results are logged and kept in
 * toy_last_result for the tests.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "toy_edu.h"

#define	EDU_ID			0x00
#define	EDU_LIVE		0x04
#define	EDU_FACT		0x08
#define	EDU_STATUS		0x20
#define	 EDU_STATUS_COMPUTING	0x01
#define	 EDU_STATUS_IRQ_FACT	0x80
#define	EDU_IRQ_STATUS		0x24
#define	EDU_IRQ_RAISE		0x60
#define	EDU_IRQ_ACK		0x64
#define	EDU_DMA_SRC		0x80
#define	EDU_DMA_DST		0x88
#define	EDU_DMA_CNT		0x90
#define	EDU_DMA_CMD		0x98
#define	 EDU_DMA_START		0x01
#define	 EDU_DMA_FROM_DEV	0x02
#define	 EDU_DMA_IRQ		0x04
#define	EDU_BUF_ADDR		0x40000
#define	EDU_BUF_SIZE		4096
#define	EDU_IRQ_FACT		0x00000001
#define	EDU_IRQ_DMA		0x00000100
#define	EDU_IRQ_TEST		0x00010000

struct toy {
	struct pci_dev		*pdev;
	void __iomem		*mmio;
	int			irq;
	struct completion	done;
	u32			irq_bits;	/* last status seen */
	atomic_t		nirq;
	void			*cbuf;
	dma_addr_t		cbuf_dma;
	struct page		*page;
	struct dma_pool		*pool;
};

struct toy_result toy_last_result;

static irqreturn_t
toy_irq(int irq, void *arg)
{
	struct toy *t = arg;
	u32 st = readl(t->mmio + EDU_IRQ_STATUS);

	(void) irq;
	if (st == 0)
		return (IRQ_NONE);
	writel(st, t->mmio + EDU_IRQ_ACK);
	WRITE_ONCE(t->irq_bits, st);
	atomic_inc(&t->nirq);
	complete(&t->done);
	return (IRQ_HANDLED);
}

static int
toy_wait(struct toy *t, u32 bit)
{
	unsigned long left = msecs_to_jiffies(2000);

	while (left > 0) {
		left = wait_for_completion_timeout(&t->done, left);
		if (left == 0)
			break;
		if (READ_ONCE(t->irq_bits) & bit)
			return (0);
	}
	return (-ETIMEDOUT);
}

/* The simulated device answers asynchronously, real hardware at once. */
static bool
toy_poll(struct toy *t, int reg, u32 want)
{
	int i;

	for (i = 0; i < 2000; i++) {
		if (readl(t->mmio + reg) == want)
			return (true);
		usleep_range(100, 200);
	}
	return (false);
}

static int
toy_dma(struct toy *t, dma_addr_t src, dma_addr_t dst, u32 len, u32 dir)
{
	reinit_completion(&t->done);
	WRITE_ONCE(t->irq_bits, 0);
	writeq(src, t->mmio + EDU_DMA_SRC);
	writeq(dst, t->mmio + EDU_DMA_DST);
	writeq(len, t->mmio + EDU_DMA_CNT);
	writeq(EDU_DMA_START | EDU_DMA_IRQ | dir, t->mmio + EDU_DMA_CMD);
	return (toy_wait(t, EDU_IRQ_DMA));
}

static int
toy_selftest(struct toy *t)
{
	struct toy_result *res = &toy_last_result;
	struct device *dev = &t->pdev->dev;
	dma_addr_t page_dma, blk_dma;
	u8 *pbuf, *blk;
	u32 id, fact;
	int i, r;

	id = readl(t->mmio + EDU_ID);
	res->id = id;
	if ((id & 0xff) != 0xed) {
		dev_err(dev, "unexpected id %#x\n", id);
		return (-ENODEV);
	}
	writel(0x5a5a1234, t->mmio + EDU_LIVE);
	res->live_ok = toy_poll(t, EDU_LIVE, ~0x5a5a1234U);

	/* Software interrupt. */
	reinit_completion(&t->done);
	writel(EDU_IRQ_TEST, t->mmio + EDU_IRQ_RAISE);
	res->irq_ok = toy_wait(t, EDU_IRQ_TEST) == 0;

	/* Factorial with completion interrupt. */
	reinit_completion(&t->done);
	WRITE_ONCE(t->irq_bits, 0);
	writel(EDU_STATUS_IRQ_FACT, t->mmio + EDU_STATUS);
	writel(10, t->mmio + EDU_FACT);
	r = toy_wait(t, EDU_IRQ_FACT);
	fact = readl(t->mmio + EDU_FACT);
	res->fact = fact;
	res->fact_ok = r == 0 && fact == 3628800;

	/* RAM -> device from a coherent buffer. */
	for (i = 0; i < EDU_BUF_SIZE; i++)
		((u8 *)t->cbuf)[i] = (u8)(i * 7 + 3);
	r = toy_dma(t, t->cbuf_dma, EDU_BUF_ADDR, EDU_BUF_SIZE, 0);
	if (r != 0)
		goto out;
	/* Device -> RAM into a streaming-mapped page. */
	pbuf = page_address(t->page);
	memset(pbuf, 0, PAGE_SIZE);
	page_dma = dma_map_page(dev, t->page, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, page_dma)) {
		r = -ENOMEM;
		goto out;
	}
	r = toy_dma(t, EDU_BUF_ADDR, page_dma, EDU_BUF_SIZE,
	    EDU_DMA_FROM_DEV);
	dma_unmap_page(dev, page_dma, PAGE_SIZE, DMA_FROM_DEVICE);
	if (r != 0)
		goto out;
	res->dma_ok = memcmp(pbuf, t->cbuf, EDU_BUF_SIZE) == 0;

	/* Device -> RAM again, into a dma_pool block (first 64 bytes). */
	blk = dma_pool_zalloc(t->pool, GFP_KERNEL, &blk_dma);
	if (blk == NULL) {
		r = -ENOMEM;
		goto out;
	}
	r = toy_dma(t, EDU_BUF_ADDR, blk_dma, 64, EDU_DMA_FROM_DEV);
	res->pool_ok = r == 0 && memcmp(blk, t->cbuf, 64) == 0;
	dma_pool_free(t->pool, blk, blk_dma);
out:
	res->irqs = atomic_read(&t->nirq);
	dev_info(dev, "edu %#x: live %d irq %d fact %u (%d) dma %d pool %d, "
	    "%d interrupts\n", id, res->live_ok, res->irq_ok, fact,
	    res->fact_ok, res->dma_ok, res->pool_ok, res->irqs);
	return (r);
}

static int
toy_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct device *dev = &pdev->dev;
	struct toy *t;
	int r, nvec;

	(void) id;
	memset(&toy_last_result, 0, sizeof (toy_last_result));
	t = devm_kzalloc(dev, sizeof (*t), GFP_KERNEL);
	if (t == NULL)
		return (-ENOMEM);
	t->pdev = pdev;
	init_completion(&t->done);
	atomic_set(&t->nirq, 0);
	r = pcim_enable_device(pdev);
	if (r != 0)
		return (r);
	pci_set_master(pdev);
	t->mmio = pcim_iomap(pdev, 0, 0);
	if (t->mmio == NULL)
		return (-ENOMEM);
	r = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(28));
	if (r != 0)
		return (r);
	t->cbuf = dma_alloc_coherent(dev, EDU_BUF_SIZE, &t->cbuf_dma,
	    GFP_KERNEL);
	if (t->cbuf == NULL)
		return (-ENOMEM);
	t->page = alloc_page(GFP_KERNEL);
	t->pool = dma_pool_create("toy", dev, 64, 64, 0);
	if (t->page == NULL || t->pool == NULL) {
		r = -ENOMEM;
		goto fail_mem;
	}
	nvec = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_INTX);
	if (nvec < 0) {
		r = nvec;
		goto fail_mem;
	}
	t->irq = pci_irq_vector(pdev, 0);
	r = request_irq(t->irq, toy_irq, IRQF_SHARED, "toy_edu", t);
	if (r != 0)
		goto fail_vec;
	pci_set_drvdata(pdev, t);
	toy_last_result.probed = true;
	r = toy_selftest(t);
	toy_last_result.selftest = r;
	return (0);	/* stay attached for inspection even if a check failed */
fail_vec:
	pci_free_irq_vectors(pdev);
fail_mem:
	dma_pool_destroy(t->pool);
	if (t->page != NULL)
		__free_page(t->page);
	dma_free_coherent(dev, EDU_BUF_SIZE, t->cbuf, t->cbuf_dma);
	return (r);
}

static void
toy_remove(struct pci_dev *pdev)
{
	struct toy *t = pci_get_drvdata(pdev);

	free_irq(t->irq, t);
	pci_free_irq_vectors(pdev);
	dma_pool_destroy(t->pool);
	__free_page(t->page);
	dma_free_coherent(&pdev->dev, EDU_BUF_SIZE, t->cbuf, t->cbuf_dma);
	toy_last_result.removed = true;
	/* mmio, enable and t itself are devres-managed. */
}

static const struct pci_device_id toy_ids[] = {
	{ PCI_DEVICE(TOY_EDU_VENDOR, TOY_EDU_DEVICE) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, toy_ids);

static struct pci_driver toy_driver = {
	.name = "toy_edu",
	.id_table = toy_ids,
	.probe = toy_probe,
	.remove = toy_remove,
};

static int
toy_init(void)
{
	return (pci_register_driver(&toy_driver));
}

static void
toy_exit(void)
{
	pci_unregister_driver(&toy_driver);
}

module_init(toy_init);
module_exit(toy_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("LinuxKPI toy driver for the QEMU edu device");
