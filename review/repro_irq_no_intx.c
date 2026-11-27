/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reproducer for docs/13 finding P2: no legacy (INTx) interrupt when MSI
 * cannot be enabled.
 *
 * i915_driver_hw_probe() does
 *	if (pci_enable_msi(pdev) < 0) drm_dbg(...);	(not fatal)
 * and intel_irq_install() then does
 *	request_irq(to_pci_dev(dev)->irq, handler, IRQF_SHARED, "i915", i915);
 * On Linux pdev->irq always holds the legacy INTx line (set at PCI
 * enumeration), so the driver keeps working without MSI. Here pdev->irq is
 * only set by pci_alloc_irq_vectors(); after a failed pci_enable_msi() it is
 * 0 and request_irq(0) fails with -EINVAL, so the whole probe fails.
 *
 * The device below supports INTx only (as an illumos system with MSI
 * disabled, or a platform whose MSI allocation fails, would report).
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linuxkpi/fakedev.h>
#include <stdio.h>
#include <string.h>

static struct lkpi_fake_pci fdev;

static irqreturn_t
handler(int irq, void *arg)
{
	(void) irq;
	(void) arg;
	return (IRQ_HANDLED);
}

int
main(void)
{
	struct pci_dev *pdev;
	int i, r, msi;

	fdev.name = "intx-only";
	fdev.info.vendor = 0x8086;
	fdev.info.device = 0x5917;
	fdev.info.class = 0x030000;
	fdev.info.devfn = PCI_DEVFN(2, 0);
	for (i = 0; i < LKPI_PCI_NUM_BARS; i++)
		fdev.info.bar[i].rnumber = -1;
	fdev.intr_types = LKPI_INTR_INTX;	/* no MSI, no MSI-X */
	fdev.max_vec = 1;
	memcpy(&fdev.cfg[0], &fdev.info.vendor, 2);
	memcpy(&fdev.cfg[2], &fdev.info.device, 2);
	lkpi_fake_pci_add(&fdev);

	pdev = lkpi_pci_dev_alloc(&fdev);
	if (pdev == NULL) {
		printf("setup failed\n");
		return (2);
	}
	msi = pci_enable_msi(pdev);		/* i915: failure only logged */
	r = request_irq(pdev->irq, handler, IRQF_SHARED, "i915", pdev);
	printf("pci_enable_msi = %d, pdev->irq = %d, request_irq = %d\n",
	    msi, pdev->irq, r);
	if (r != 0) {
		printf("FAIL: without MSI there is no usable pdev->irq; "
		    "i915 probe fails in intel_irq_install()\n");
		return (1);
	}
	(void) free_irq(pdev->irq, pdev);
	printf("ok\n");
	return (0);
}
