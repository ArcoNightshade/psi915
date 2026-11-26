/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for the P2 layers that can run without hardware: the device
 * model and devres, PCI wrappers over a fake device (config space,
 * capabilities, enumeration, BAR mapping), interrupts and tasklets, the
 * DMA API over the fake IOMMU, scatterlists, DMA pools, struct page,
 * ioremap/io_mapping, firmware loading, DMI matching, I2C bit-banging
 * against a simulated EEPROM, seq_file and CPU feature detection.
 * Userland only (fake devices); the kernel suite is empty.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/io-mapping.h>
#include <linux/firmware.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <asm/set_memory.h>
#include <asm/cpu_device_id.h>
#include <linuxkpi/subsys.h>
#include <linux/shmem_fs.h>

#include "harness.h"

#ifndef LKPI_KTEST

#include <linuxkpi/fakedev.h>
#include <sys/stat.h>

static int released;

static void
dev_release(struct device *dev)
{
	(void) dev;
	released++;
}

static int actions[4], action_order[4], norder;

static void
act(void *p)
{
	int i = (int)(intptr_t)p;

	actions[i]++;
	action_order[norder++] = i;
}

TEST(device_devres)
{
	struct device parent = { 0 }, dev = { 0 };
	void *p, *g;
	char *s;

	released = 0;
	device_initialize(&parent);
	parent.release = dev_release;
	CHECK_EQ(dev_set_name(&parent, "parent%d", 0), 0);
	CHECK_EQ(device_add(&parent), 0);
	device_initialize(&dev);
	dev.parent = &parent;
	dev.release = dev_release;
	dev.init_name = "child";
	CHECK_EQ(device_add(&dev), 0);
	CHECK_EQ(strcmp(dev_name(&dev), "child"), 0);
	CHECK_EQ(strcmp(lkpi_fake_uevent_action, "add"), 0);
	CHECK_EQ(strcmp(lkpi_fake_uevent_path, "/parent0/child"), 0);
	{
		char *env[] = { "HOTPLUG=1", NULL };

		CHECK_EQ(kobject_uevent_env(&dev.kobj, KOBJ_CHANGE, env), 0);
		CHECK_EQ(strcmp(lkpi_fake_uevent_action, "change"), 0);
		CHECK_EQ(strcmp(lkpi_fake_uevent_env0, "HOTPLUG=1"), 0);
	}

	/* devres: reverse order release, kfree, remove_action, groups */
	memset(actions, 0, sizeof (actions));
	norder = 0;
	p = devm_kzalloc(&dev, 100, GFP_KERNEL);
	CHECK(p != NULL && ((char *)p)[99] == 0);
	s = devm_kasprintf(&dev, GFP_KERNEL, "x%d", 42);
	CHECK(s != NULL && strcmp(s, "x42") == 0);
	devm_kfree(&dev, s);
	CHECK_EQ(devm_add_action(&dev, act, (void *)0), 0);
	CHECK_EQ(devm_add_action(&dev, act, (void *)1), 0);
	CHECK_EQ(devm_add_action_or_reset(&dev, act, (void *)2), 0);
	devm_remove_action(&dev, act, (void *)1);
	g = devres_open_group(&dev, NULL, GFP_KERNEL);
	CHECK(g != NULL);
	CHECK_EQ(devm_add_action(&dev, act, (void *)3), 0);
	devres_close_group(&dev, g);
	CHECK_EQ(devres_release_group(&dev, g), 1);
	CHECK_EQ(actions[3], 1);
	CHECK_EQ(lkpi_devres_release_all(&dev), 3);	/* kzalloc, act0, act2 */
	CHECK_EQ(actions[0], 1);
	CHECK_EQ(actions[1], 0);
	CHECK_EQ(actions[2], 1);
	CHECK_EQ(action_order[1], 2);		/* newest first */
	CHECK_EQ(action_order[2], 0);

	CHECK(get_device(&dev) == &dev);
	put_device(&dev);
	device_del(&dev);
	put_device(&dev);
	CHECK_EQ(released, 1);
	device_unregister(&parent);
	CHECK_EQ(released, 2);

	{
		struct class *cls = class_create("drm");
		struct device *cd;

		CHECK(!IS_ERR(cls));
		cd = device_create(cls, NULL, 7, &parent, "card%d", 0);
		CHECK(!IS_ERR(cd));
		CHECK_EQ(strcmp(dev_name(cd), "card0"), 0);
		CHECK(dev_get_drvdata(cd) == &parent);
		device_destroy(cls, 7);
		class_destroy(cls);
	}
	CHECK_EQ(pm_runtime_get_sync(&dev), 1);
	(void) pm_runtime_put(&dev);
	CHECK(IS_ERR(debugfs_create_dir("x", NULL)));
}

/* A fake PCI function with MSI, PCIe and an extended capability. */
static struct lkpi_fake_pci fdev;
static u8 fbar0[8192] __attribute__((__aligned__(4096)));
static u8 fbar2[16384] __attribute__((__aligned__(4096)));

static void
fdev_setup(void)
{
	memset(&fdev, 0, sizeof (fdev));
	fdev.info.vendor = 0x8086;
	fdev.info.device = 0x5917;
	fdev.info.subvendor = 0x1028;
	fdev.info.subdevice = 0x0001;
	fdev.info.class = 0x030000;
	fdev.info.revision = 7;
	fdev.info.bus = 0;
	fdev.info.devfn = PCI_DEVFN(2, 0);
	fdev.info.bar[0].start = 0xe0000000;
	fdev.info.bar[0].len = sizeof (fbar0);
	fdev.info.bar[0].flags = LKPI_BAR_MEM | LKPI_BAR_64;
	fdev.info.bar[0].rnumber = 1;
	fdev.info.bar[2].start = 0xc0000000;
	fdev.info.bar[2].len = sizeof (fbar2);
	fdev.info.bar[2].flags = LKPI_BAR_MEM | LKPI_BAR_PREFETCH;
	fdev.info.bar[2].rnumber = 3;
	fdev.info.bar[1].rnumber = -1;
	fdev.info.bar[3].rnumber = -1;
	fdev.info.bar[4].rnumber = -1;
	fdev.info.bar[5].rnumber = -1;
	fdev.bar_mem[0] = fbar0;
	fdev.bar_mem[2] = fbar2;
	fdev.intr_types = LKPI_INTR_MSIX | LKPI_INTR_MSI;
	fdev.max_vec = 2;
	fdev.name = "gpu";
	memcpy(&fdev.cfg[0], &fdev.info.vendor, 2);
	memcpy(&fdev.cfg[2], &fdev.info.device, 2);
	fdev.cfg[6] = 0x10;		/* capability list */
	fdev.cfg[0x34] = 0x40;
	fdev.cfg[0x40] = PCI_CAP_ID_PM;
	fdev.cfg[0x41] = 0x50;
	fdev.cfg[0x50] = PCI_CAP_ID_MSI;
	fdev.cfg[0x51] = 0x60;
	fdev.cfg[0x60] = PCI_CAP_ID_EXP;
	fdev.cfg[0x61] = 0x00;
	fdev.cfg[0x62] = 0x92;		/* flags: RC endpoint, version 2 */
	fdev.cfg[0x6c] = 0x23;		/* LNKCAP: 8 GT/s, x2 */
	fdev.cfg[0x6d] = 0x00;
	/* Extended caps: AER at 0x100 -> vendor at 0x140 -> end. */
	{
		u32 aer = PCI_EXT_CAP_ID_ERR | (1 << 16) | (0x140 << 20);
		u32 vndr = PCI_EXT_CAP_ID_VNDR | (1 << 16);

		memcpy(&fdev.cfg[0x100], &aer, 4);
		memcpy(&fdev.cfg[0x140], &vndr, 4);
	}
	lkpi_fake_pci_add(&fdev);
}

static atomic_t irq_hits;
static atomic_t thread_hits;

static irqreturn_t
hw_irq(int irq, void *dev_id)
{
	(void) irq;
	atomic_inc(&irq_hits);
	return (dev_id == (void *)1 ? IRQ_WAKE_THREAD : IRQ_HANDLED);
}

static irqreturn_t
hw_thread(int irq, void *dev_id)
{
	(void) irq;
	(void) dev_id;
	atomic_inc(&thread_hits);
	return (IRQ_HANDLED);
}

static atomic_t tl_runs;

static void
tl_fn(struct tasklet_struct *t)
{
	(void) t;
	atomic_inc(&tl_runs);
}

static atomic_t iw_runs;

static void
iw_fn(struct irq_work *w)
{
	(void) w;
	atomic_inc(&iw_runs);
}

TEST(pci_wrapper)
{
	struct pci_dev *pdev, *again, *found;
	void __iomem *m, *wc;
	u16 w;
	u32 d;
	int n, irq;

	fdev_setup();
	pdev = lkpi_pci_dev_alloc(&fdev);
	CHECK(pdev != NULL);
	if (pdev == NULL)
		return;
	again = lkpi_pci_dev_alloc(&fdev);
	CHECK(again == pdev);	/* one wrapper per function */
	pci_dev_put(again);
	CHECK_EQ(pdev->vendor, 0x8086);
	CHECK_EQ(pdev->device, 0x5917);
	CHECK_EQ(pdev->subsystem_vendor, 0x1028);
	CHECK_EQ(pdev->class, 0x030000);
	CHECK_EQ(pdev->revision, 7);
	CHECK_EQ(PCI_SLOT(pdev->devfn), 2);
	CHECK_EQ(strcmp(pci_name(pdev), "0000:00:02.0"), 0);
	CHECK_EQ(pci_resource_start(pdev, 0), 0xe0000000);
	CHECK_EQ(pci_resource_len(pdev, 0), sizeof (fbar0));
	CHECK(pci_resource_flags(pdev, 0) & IORESOURCE_MEM_64);
	CHECK(pci_resource_flags(pdev, 2) & IORESOURCE_PREFETCH);
	CHECK_EQ(pci_resource_len(pdev, 1), 0);

	CHECK_EQ(pci_read_config_word(pdev, PCI_VENDOR_ID, &w), 0);
	CHECK_EQ(w, 0x8086);
	CHECK_EQ(pci_read_config_dword(pdev, 1, &d), PCIBIOS_BAD_REGISTER_NUMBER);
	CHECK_EQ(pci_find_capability(pdev, PCI_CAP_ID_MSI), 0x50);
	CHECK_EQ(pci_find_capability(pdev, PCI_CAP_ID_MSIX), 0);
	CHECK_EQ(pdev->pcie_cap, 0x60);
	CHECK_EQ(pdev->pm_cap, 0x40);
	CHECK(pci_is_pcie(pdev));
	CHECK_EQ(pci_pcie_type(pdev), PCI_EXP_TYPE_RC_END);
	CHECK_EQ(pcie_get_speed_cap(pdev), PCIE_SPEED_8_0GT);
	CHECK_EQ(pcie_get_width_cap(pdev), 2);
	CHECK_EQ(pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR), 0x100);
	CHECK_EQ(pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_VNDR), 0x140);
	CHECK_EQ(pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ACS), 0);
	CHECK_EQ(pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, 0x10), 0);
	CHECK_EQ(pcie_capability_read_word(pdev, PCI_EXP_DEVCTL, &w), 0);
	CHECK_EQ(w, 0x10);

	CHECK_EQ(pci_enable_device(pdev), 0);
	pci_set_master(pdev);
	CHECK_EQ(pci_read_config_word(pdev, PCI_COMMAND, &w), 0);
	CHECK_EQ(w, PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* Enumeration */
	found = pci_get_device(0x8086, 0x5917, NULL);
	CHECK(found == pdev);
	found = pci_get_device(0x8086, 0x5917, found);
	CHECK(found == NULL);
	found = pci_get_class(0x030000, NULL);
	CHECK(found == pdev);
	pci_dev_put(found);
	found = pci_get_domain_bus_and_slot(0, 0, PCI_DEVFN(2, 0));
	CHECK(found == pdev);
	pci_dev_put(found);
	CHECK(pci_get_domain_bus_and_slot(0, 0, PCI_DEVFN(0, 0)) == NULL);
	{
		static const struct pci_device_id ids[] = {
			{ PCI_DEVICE(0x8086, 0x5917), .driver_data = 9 },
			{ 0 }
		};

		CHECK(pci_match_id(ids, pdev) == &ids[0]);
		CHECK(pci_dev_present(ids));
	}

	/* BAR mappings */
	m = pci_iomap(pdev, 0, 0);
	CHECK(m == (void *)fbar0);
	writel(0xdeadbeef, (char *)m + 8);
	CHECK_EQ(readl((char *)m + 8), 0xdeadbeef);
	wc = pci_iomap_wc_range(pdev, 2, 4096, 4096);
	CHECK(wc == (void *)(fbar2 + 4096));
	CHECK(pci_iomap(pdev, 1, 0) == NULL);
	CHECK(pci_iomap_range(pdev, 2, sizeof (fbar2), 0) == NULL);
	pci_iounmap(pdev, wc);
	pci_iounmap(pdev, m);

	/* Interrupts: MSI-X preferred, two vectors */
	n = pci_alloc_irq_vectors(pdev, 1, 4, PCI_IRQ_MSIX | PCI_IRQ_MSI);
	CHECK_EQ(n, 2);
	CHECK(pdev->msix_enabled);
	irq = pci_irq_vector(pdev, 1);
	CHECK(irq > 0 && irq != pdev->irq);
	CHECK_EQ(pci_irq_vector(pdev, 2), -EINVAL);
	atomic_set(&irq_hits, 0);
	atomic_set(&thread_hits, 0);
	CHECK_EQ(request_irq(pdev->irq, hw_irq, 0, "v0", (void *)2), 0);
	CHECK_EQ(request_irq(pdev->irq, hw_irq, 0, "dup", (void *)3), -EBUSY);
	CHECK_EQ(request_threaded_irq(irq, hw_irq, hw_thread, 0, "v1",
	    (void *)1), 0);
	CHECK(lkpi_fake_raise_irq(&fdev, 0));
	CHECK(lkpi_fake_raise_irq(&fdev, 1));
	synchronize_irq(irq);
	CHECK_EQ(atomic_read(&irq_hits), 2);
	CHECK_EQ(atomic_read(&thread_hits), 1);
	disable_irq(pdev->irq);
	CHECK(!lkpi_fake_raise_irq(&fdev, 0));	/* masked */
	enable_irq(pdev->irq);
	CHECK(lkpi_fake_raise_irq(&fdev, 0));
	CHECK(free_irq(pdev->irq, (void *)2) != NULL);
	CHECK(!lkpi_fake_raise_irq(&fdev, 0));
	(void) free_irq(irq, (void *)1);
	pci_free_irq_vectors(pdev);
	CHECK_EQ(fdev.vec_alloc, 0);
	CHECK_EQ(pdev->irq, 0);

	/* Tasklets and irq_work */
	{
		struct tasklet_struct t;
		struct irq_work iw;

		tasklet_setup(&t, tl_fn);
		atomic_set(&tl_runs, 0);
		tasklet_schedule(&t);
		lkpi_plat_sleep_ns(20000000);
		CHECK_EQ(atomic_read(&tl_runs), 1);
		tasklet_disable(&t);
		tasklet_schedule(&t);
		lkpi_plat_sleep_ns(20000000);
		CHECK_EQ(atomic_read(&tl_runs), 1);	/* held while disabled */
		tasklet_enable(&t);
		lkpi_plat_sleep_ns(20000000);
		CHECK_EQ(atomic_read(&tl_runs), 2);
		CHECK(tasklet_trylock(&t));
		CHECK(!tasklet_trylock(&t));
		tasklet_unlock(&t);
		tasklet_kill(&t);
		init_irq_work(&iw, iw_fn);
		atomic_set(&iw_runs, 0);
		CHECK(irq_work_queue(&iw));
		irq_work_sync(&iw);
		CHECK_EQ(atomic_read(&iw_runs), 1);
	}

	pci_disable_device(pdev);
	CHECK_EQ(pci_read_config_word(pdev, PCI_COMMAND, &w), 0);
	CHECK(!(w & PCI_COMMAND_MASTER));
	pci_dev_put(pdev);
	lkpi_fake_pci_remove(&fdev);
}

TEST(dma_api)
{
	struct device dev = { 0 };
	u64 mask = 0;
	dma_addr_t h, a;
	void *cpu;
	u8 *buf;
	int live = lkpi_fake_dma_live();
	struct page *pages[6];
	struct sg_table sgt;
	struct scatterlist *sg;
	struct sg_page_iter it;
	struct sg_dma_page_iter dit;
	struct dma_pool *pool;
	void *blk[40];
	dma_addr_t bd[40];
	unsigned int i, cnt;

	device_initialize(&dev);
	dev.release = dev_release;
	dev.dma_mask = &mask;
	CHECK_EQ(dma_set_mask_and_coherent(&dev, DMA_BIT_MASK(32)), 0);
	CHECK_EQ(dma_get_mask(&dev), DMA_BIT_MASK(32));

	cpu = dma_alloc_coherent(&dev, 5000, &h, GFP_KERNEL);
	CHECK(cpu != NULL);
	CHECK(h != (dma_addr_t)(uintptr_t)cpu);	/* IOMMU-translated */
	CHECK(lkpi_fake_dma_kva(h, 5000) == cpu);
	memset(cpu, 0xab, 5000);
	dma_free_coherent(&dev, 5000, cpu, h);
	CHECK(lkpi_fake_dma_kva(h, 1) == NULL);

	buf = kmalloc(256, GFP_KERNEL);
	a = dma_map_single(&dev, buf, 256, DMA_TO_DEVICE);
	CHECK(!dma_mapping_error(&dev, a));
	CHECK(lkpi_fake_dma_kva(a + 16, 16) == buf + 16);
	dma_sync_single_for_device(&dev, a, 256, DMA_TO_DEVICE);
	dma_sync_single_for_cpu(&dev, a + 64, 32, DMA_FROM_DEVICE);
	dma_unmap_single(&dev, a, 256, DMA_TO_DEVICE);
	kfree(buf);

	/* Mask too small for any fake bus address: mapping fails. */
	CHECK_EQ(dma_set_mask(&dev, DMA_BIT_MASK(12)), 0);
	buf = kmalloc(64, GFP_KERNEL);
	CHECK(dma_mapping_error(&dev, dma_map_single(&dev, buf, 64,
	    DMA_BIDIRECTIONAL)));
	kfree(buf);
	CHECK_EQ(dma_set_mask(&dev, DMA_BIT_MASK(32)), 0);

	/* sg_table from pages: one order-1 block (contiguous) + singles */
	pages[0] = alloc_pages(GFP_KERNEL | __GFP_ZERO, 1);
	CHECK(pages[0] != NULL);
	pages[1] = pages[0] + 1;
	for (i = 2; i < 6; i++)
		pages[i] = alloc_page(GFP_KERNEL);
	CHECK_EQ(sg_alloc_table_from_pages(&sgt, pages, 6, 0, 6 * PAGE_SIZE,
	    GFP_KERNEL), 0);
	CHECK(sgt.orig_nents <= 5);
	CHECK_EQ(sgt.sgl[0].length, 2 * PAGE_SIZE);
	cnt = 0;
	for_each_sgtable_page(&sgt, &it, 0) {
		CHECK(sg_page_iter_page(&it) == pages[cnt]);
		cnt++;
	}
	CHECK_EQ(cnt, 6);
	CHECK_EQ(dma_map_sgtable(&dev, &sgt, DMA_BIDIRECTIONAL, 0), 0);
	cnt = 0;
	for_each_sgtable_dma_sg(&sgt, sg, i) {
		CHECK(lkpi_fake_dma_kva(sg_dma_address(sg), sg_dma_len(sg)) ==
		    sg_virt(sg));
		cnt += sg_dma_len(sg);
	}
	CHECK_EQ(cnt, 6 * PAGE_SIZE);
	cnt = 0;
	for_each_sgtable_dma_page(&sgt, &dit, 0) {
		CHECK(lkpi_fake_dma_kva(sg_page_iter_dma_address(&dit),
		    PAGE_SIZE) != NULL);
		cnt++;
	}
	CHECK_EQ(cnt, 6);
	dma_sync_sgtable_for_cpu(&dev, &sgt, DMA_BIDIRECTIONAL);
	dma_unmap_sgtable(&dev, &sgt, DMA_BIDIRECTIONAL, 0);
	sg_free_table(&sgt);
	/* max_segment splits the contiguous run */
	CHECK_EQ(sg_alloc_table_from_pages_segment(&sgt, pages, 2, 0,
	    2 * PAGE_SIZE, PAGE_SIZE, GFP_KERNEL), 0);
	CHECK_EQ(sgt.orig_nents, 2);
	CHECK_EQ(sg_nents(sgt.sgl), 2);
	sg_free_table(&sgt);
	__free_pages(pages[0], 1);
	for (i = 2; i < 6; i++)
		__free_page(pages[i]);

	/* dma_pool: alignment and boundary */
	pool = dma_pool_create("t", &dev, 48, 64, 1024);
	CHECK(pool != NULL);
	for (i = 0; i < 40; i++) {
		blk[i] = dma_pool_alloc(pool, GFP_KERNEL, &bd[i]);
		CHECK(blk[i] != NULL);
		CHECK_EQ(bd[i] & 63, 0);
		CHECK_EQ(bd[i] / 1024, (bd[i] + 47) / 1024);
		CHECK(lkpi_fake_dma_kva(bd[i], 48) == blk[i]);
	}
	for (i = 0; i < 40; i++)
		dma_pool_free(pool, blk[i], bd[i]);
	dma_pool_destroy(pool);
	CHECK_EQ(lkpi_fake_dma_live(), live);
	put_device(&dev);
}

TEST(pages_and_io)
{
	struct page *p, *q;
	struct page *arr[2];
	unsigned long addr;
	void *v;
	struct io_mapping iom = { 0 };
	char *map;

	p = alloc_pages(GFP_KERNEL | __GFP_ZERO, 2);
	CHECK(p != NULL);
	if (p == NULL)
		return;
	CHECK_EQ(compound_order(p), 2);
	CHECK(compound_head(p + 3) == p);
	CHECK(virt_to_page((char *)page_address(p) + 2 * PAGE_SIZE) == p + 2);
	CHECK(pfn_to_page(page_to_pfn(p + 1)) == p + 1);
	CHECK_EQ(page_count(p + 1), 1);
	get_page(p + 3);
	CHECK_EQ(page_count(p), 2);
	put_page(p);
	CHECK_EQ(((char *)kmap(p + 1))[100], 0);
	kunmap(p + 1);
	arr[0] = p;
	arr[1] = p + 1;
	v = vmap(arr, 2, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	CHECK(v == page_address(p));
	vunmap(v);
	CHECK_EQ(set_pages_array_wc(arr, 2), 0);
	CHECK_EQ(set_pages_array_wb(arr, 2), 0);
	clflush_cache_range(page_address(p), 4 * PAGE_SIZE);
	__free_pages(p, 2);

	addr = __get_free_page(GFP_KERNEL);
	CHECK(addr != 0);
	q = virt_to_page((void *)addr);
	CHECK(q != NULL && page_address(q) == (void *)addr);
	free_page(addr);
	CHECK_EQ(get_order(1), 0);
	CHECK_EQ(get_order(PAGE_SIZE + 1), 1);
	CHECK_EQ(get_order(5 * PAGE_SIZE), 3);

	/* ioremap and io_mapping of a fake BAR by physical address */
	fdev_setup();
	map = (char *)ioremap_wc(0xc0000000 + 4096, 4096);
	CHECK(map == (char *)fbar2 + 4096);
	iounmap(map);
	CHECK(io_mapping_init_wc(&iom, 0xc0000000, sizeof (fbar2)) != NULL);
	map = io_mapping_map_wc(&iom, 8192, 4096);
	CHECK(map == (char *)fbar2 + 8192);
	io_mapping_unmap(map);
	io_mapping_fini(&iom);
	lkpi_fake_pci_remove(&fdev);
	CHECK(totalram_pages() > 0);
}

TEST(firmware_dmi_acpi_cpu)
{
	const struct firmware *fw;
	char dir[] = "/tmp/lkpi_fw_XXXXXX";
	char path[256];
	FILE *f;
	static int cb_hits;
	static const struct dmi_system_id quirks[] = {
		{ .ident = "rugged", .matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
		    DMI_MATCH(DMI_PRODUCT_NAME, "5420 Rugged") } },
		{ .ident = "other", .matches = {
		    DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Dell") } },
		{ }
	};

	CHECK(mkdtemp(dir) != NULL);
	(void) snprintf(path, sizeof (path), "%s/i915", dir);
	CHECK_EQ(mkdir(path, 0700), 0);
	(void) snprintf(path, sizeof (path), "%s/i915/kbl_dmc.bin", dir);
	f = fopen(path, "wb");
	CHECK(f != NULL);
	if (f != NULL) {
		fputs("DMCFW", f);
		fclose(f);
	}
	lkpi_fake_firmware_dir = dir;
	CHECK_EQ(request_firmware(&fw, "i915/kbl_dmc.bin", NULL), 0);
	CHECK(fw != NULL && fw->size == 5 && memcmp(fw->data, "DMCFW", 5) == 0);
	release_firmware(fw);
	CHECK_EQ(firmware_request_nowarn(&fw, "i915/missing.bin", NULL),
	    -ENOENT);
	CHECK(fw == NULL);
	CHECK_EQ(request_firmware(&fw, "../etc/passwd", NULL), -EINVAL);
	{
		char small[3];

		CHECK_EQ(request_firmware_into_buf(&fw, "i915/kbl_dmc.bin", NULL,
		    small, sizeof (small)), -EFBIG);
	}
	(void) unlink(path);
	(void) snprintf(path, sizeof (path), "%s/i915", dir);
	(void) rmdir(path);
	(void) rmdir(dir);
	lkpi_fake_firmware_dir = "/nonexistent";

	lkpi_fake_dmi[DMI_SYS_VENDOR] = "Dell Inc.";
	lkpi_fake_dmi[DMI_PRODUCT_NAME] = "Latitude 5420 Rugged";
	(void) cb_hits;
	CHECK_EQ(dmi_check_system(quirks), 1);
	CHECK(dmi_first_match(quirks) == &quirks[0]);
	CHECK(dmi_match(DMI_SYS_VENDOR, "Dell Inc."));
	CHECK(!dmi_match(DMI_SYS_VENDOR, "Dell"));
	CHECK_EQ(strcmp(dmi_get_system_info(DMI_PRODUCT_NAME),
	    "Latitude 5420 Rugged"), 0);
	CHECK(dmi_get_system_info(DMI_BOARD_NAME) == NULL);
	lkpi_fake_dmi[DMI_SYS_VENDOR] = NULL;
	lkpi_fake_dmi[DMI_PRODUCT_NAME] = NULL;
	CHECK_EQ(dmi_check_system(quirks), 0);

	{
		guid_t g = GUID_INIT(0x3e5b41c6, 0xeb1d, 0x4260, 0x9d, 0x15,
		    0xc7, 0x1f, 0xba, 0xda, 0xe4, 0x14);

		CHECK_EQ(g.b[0], 0xc6);
		CHECK_EQ(g.b[4], 0x1d);
		CHECK(ACPI_HANDLE((struct device *)NULL) == NULL);
		CHECK(acpi_evaluate_dsm(NULL, &g, 1, 0, NULL) == NULL);
		CHECK(!acpi_check_dsm(NULL, &g, 1, 1));
	}

	CHECK(boot_cpu_data.x86_clflush_size >= 32);
	CHECK(boot_cpu_has(X86_FEATURE_CLFLUSH));
	CHECK(boot_cpu_data.x86_vendor_id[0] != '\0');
	{
		/* x86_match_cpu: a table built from the boot CPU must match. */
		struct x86_cpu_id ids[3];

		memset(ids, 0, sizeof (ids));
		ids[0].vendor = X86_VENDOR_ANY;
		ids[0].family = (u16)(boot_cpu_data.x86 + 1);
		ids[0].flags = X86_CPU_ID_FLAG_ENTRY_VALID;
		ids[1].vendor = boot_cpu_data.x86_vendor;
		ids[1].family = boot_cpu_data.x86;
		ids[1].model = boot_cpu_data.x86_model;
		ids[1].flags = X86_CPU_ID_FLAG_ENTRY_VALID;
		ids[1].driver_data = 7;
		if (boot_cpu_data.x86_model != 0)
			CHECK(x86_match_cpu(ids) == &ids[1]);
		ids[1].model = (u16)(boot_cpu_data.x86_model + 1);
		CHECK(x86_match_cpu(ids) == NULL);
	}
	{
		char buf[64];
		struct seq_file m;

		lkpi_seq_init(&m, buf, sizeof (buf));
		seq_printf(&m, "x=%d ", 5);
		seq_puts(&m, "ok");
		seq_putc(&m, '!');
		CHECK_EQ(strcmp(buf, "x=5 ok!"), 0);
		CHECK(!seq_has_overflowed(&m));
	}
}

/*
 * Simulated 24C02-style EEPROM at 0x50 on bit-banged lines: reacts to
 * SCL/SDA transitions made through the algo-bit callbacks.
 */
struct sim_i2c {
	int	scl, sda_m, sda_s;	/* master and slave drive (1 = release) */
	int	state;			/* 0 idle, 1 address, 2 write, 3 read */
	int	bit, byte, ack_phase;
	bool	selected, read;
	u8	ptr;
	bool	ptr_set;
	u8	mem[256];
	int	stretch;
};

static struct sim_i2c si;

static int
sim_sda(void)
{
	return (si.sda_m && si.sda_s);
}

static void
sim_scl_rise(void)
{
	int b = sim_sda();

	if (si.state == 0)
		return;
	if (si.ack_phase) {
		if (si.state == 3 && b)		/* master NAK: done */
			si.state = 0;
		return;
	}
	if (si.state == 3)
		return;			/* slave drives data */
	si.byte = (si.byte << 1) | b;
	si.bit++;
}

static void
sim_scl_fall(void)
{
	if (si.state == 0)
		return;
	if (si.ack_phase) {
		si.ack_phase = 0;
		si.sda_s = 1;
		if (si.state == 3) {		/* next byte out */
			si.byte = si.mem[si.ptr++];
			si.bit = 0;
			si.sda_s = (si.byte >> 7) & 1;
		}
		return;
	}
	if (si.state == 3) {
		si.bit++;
		if (si.bit == 8) {
			si.sda_s = 1;		/* release for master ACK */
			si.ack_phase = 1;
		} else {
			si.sda_s = (si.byte >> (7 - si.bit)) & 1;
		}
		return;
	}
	if (si.bit < 8)
		return;
	/* A whole byte was received: ACK it (or not). */
	si.bit = 0;
	if (si.state == 1) {
		si.selected = (si.byte >> 1) == 0x50;
		si.read = si.byte & 1;
		if (!si.selected) {
			si.state = 0;
			return;
		}
		si.sda_s = 0;
		si.ack_phase = 1;
		if (si.read) {
			si.state = 3;
			/* first data byte is driven after the ACK clock */
		} else {
			si.state = 2;
			si.ptr_set = false;
		}
	} else if (si.state == 2) {
		if (!si.ptr_set) {
			si.ptr = (u8)si.byte;
			si.ptr_set = true;
		} else {
			si.mem[si.ptr++] = (u8)si.byte;
		}
		si.sda_s = 0;
		si.ack_phase = 1;
	}
	si.byte = 0;
}

static void
sim_setsda(void *data, int state)
{
	int old = sim_sda();

	(void) data;
	si.sda_m = state ? 1 : 0;
	if (si.scl && old && !sim_sda()) {		/* START */
		si.state = 1;
		si.bit = 0;
		si.byte = 0;
		si.ack_phase = 0;
		si.sda_s = 1;
	} else if (si.scl && !old && sim_sda()) {	/* STOP */
		si.state = 0;
		si.sda_s = 1;
	}
}

static void
sim_setscl(void *data, int state)
{
	(void) data;
	if (state && !si.scl) {
		si.scl = 1;
		sim_scl_rise();
	} else if (!state && si.scl) {
		si.scl = 0;
		sim_scl_fall();
	}
}

static int
sim_getsda(void *data)
{
	(void) data;
	return (sim_sda());
}

static int
sim_getscl(void *data)
{
	(void) data;
	if (si.stretch > 0) {		/* clock stretching */
		si.stretch--;
		return (0);
	}
	return (si.scl);
}

TEST(i2c_bitbang)
{
	struct i2c_algo_bit_data bit = {
		.setsda = sim_setsda, .setscl = sim_setscl,
		.getsda = sim_getsda, .getscl = sim_getscl,
		.udelay = 1, .timeout = 100,
	};
	struct i2c_adapter adap;
	u8 off = 0x10, rd[8], wr[5] = { 0x20, 'E', 'D', 'I', 'D' };
	struct i2c_msg msgs[2];
	int i;

	memset(&si, 0, sizeof (si));
	si.sda_m = si.sda_s = si.scl = 1;
	for (i = 0; i < 256; i++)
		si.mem[i] = (u8)(i ^ 0x5a);
	memset(&adap, 0, sizeof (adap));
	(void) strscpy(adap.name, "sim", sizeof (adap.name));
	adap.algo_data = &bit;
	CHECK_EQ(i2c_bit_add_bus(&adap), 0);
	CHECK(i2c_check_functionality(&adap, I2C_FUNC_I2C));

	/* Random read: write offset, repeated START, read 8 bytes. */
	msgs[0] = (struct i2c_msg){ 0x50, 0, 1, &off };
	msgs[1] = (struct i2c_msg){ 0x50, I2C_M_RD, sizeof (rd), rd };
	si.stretch = 3;
	CHECK_EQ(i2c_transfer(&adap, msgs, 2), 2);
	for (i = 0; i < 8; i++)
		CHECK_EQ(rd[i], (0x10 + i) ^ 0x5a);

	/* Write then read back. */
	msgs[0] = (struct i2c_msg){ 0x50, 0, sizeof (wr), wr };
	CHECK_EQ(i2c_transfer(&adap, msgs, 1), 1);
	CHECK_EQ(memcmp(&si.mem[0x20], "EDID", 4), 0);

	/* No device at 0x51: address NAK. */
	msgs[0] = (struct i2c_msg){ 0x51, I2C_M_RD, 1, rd };
	CHECK_EQ(i2c_transfer(&adap, msgs, 1), -ENXIO);
	CHECK_EQ(si.state, 0);
	CHECK(i2c_get_adapter(adap.nr) == &adap);
	i2c_del_adapter(&adap);
	CHECK(i2c_get_adapter(adap.nr) == NULL);
}

#endif /* !LKPI_KTEST */

/* shmem page cache (src/linux_shmem.c) on a hand-made inode. */
TEST(shmem_cache)
{
	struct inode *ip = kzalloc(sizeof (*ip), GFP_KERNEL);
	struct address_space *m;
	struct page *p, *q;
	struct folio *f;
	void *fsdata;
	char *va;

	CHECK(ip != NULL);
	ip->i_mapping = &ip->i_data;
	ip->i_data.host = ip;
	ip->i_size = 3 * PAGE_SIZE - 1;		/* 3 pages */
	m = ip->i_mapping;
	lkpi_shmem_mapping_init(m);
	CHECK(find_lock_page(m, 0) == NULL);
	p = shmem_read_mapping_page(m, 1);
	CHECK(!IS_ERR(p));
	va = page_address(p);
	CHECK(va[0] == 0 && va[PAGE_SIZE - 1] == 0);	/* zero-filled */
	va[7] = 42;
	CHECK_EQ(page_count(p), 2);		/* cache + caller */
	put_page(p);
	q = shmem_read_mapping_page(m, 1);
	CHECK(q == p && ((char *)page_address(q))[7] == 42);
	put_page(q);
	CHECK(IS_ERR(shmem_read_mapping_page(m, 3)));	/* beyond i_size */
	CHECK(m->a_ops->write_begin(NULL, m, 2 * PAGE_SIZE + 5, 4, &f,
	    &fsdata) == 0);
	memcpy((char *)page_address(&f->page) + 5, "abcd", 4);
	CHECK_EQ(m->a_ops->write_end(NULL, m, 2 * PAGE_SIZE + 5, 4, 4, f,
	    fsdata), 4);
	CHECK_EQ(lkpi_shmem_nr_pages(m), 2);
	q = find_lock_page(m, 2);
	CHECK(q != NULL && memcmp((char *)page_address(q) + 5, "abcd", 4) == 0);
	put_page(q);
	shmem_truncate_range(ip, PAGE_SIZE, -1);	/* drops 1 and 2 */
	CHECK_EQ(lkpi_shmem_nr_pages(m), 0);
	f = shmem_read_folio(m, 0);
	CHECK(!IS_ERR(f));
	folio_put(f);
	shmem_truncate_range(ip, 1, PAGE_SIZE - 1);	/* partial page: kept */
	CHECK_EQ(lkpi_shmem_nr_pages(m), 1);
	m->lkpi_release(m);				/* inode release */
	kfree(ip);
}

LKPI_SUITE(hw)
{
	RUN(shmem_cache);
#ifndef LKPI_KTEST
	CHECK_EQ(lkpi_workqueue_init(), 0);
	RUN(device_devres);
	RUN(pci_wrapper);
	RUN(dma_api);
	RUN(pages_and_io);
	RUN(firmware_dmi_acpi_cpu);
	RUN(i2c_bitbang);
	lkpi_workqueue_fini();
#endif
}
