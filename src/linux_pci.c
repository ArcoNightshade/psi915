/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * PCI device wrapper and driver binding (see <linux/pci.h>). Clean-room.
 *
 * A struct pci_dev is created per dev_info_t (lkpi_pci_dev_alloc()):
 * the attach(9E) glue makes the one it binds; enumeration
 * (pci_get_device() etc.) makes refcounted wrappers for other functions,
 * kept on lkpi_pci_devs so a function has at most one wrapper. The
 * wrapper holds the dev_info_t (ndi_hold_devi) and a config handle until
 * its last reference goes.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linuxkpi/intel_gfx.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/bug.h>
#include <linuxkpi/plat_dev.h>

static LIST_HEAD(lkpi_pci_devs);		/* pci_dev.bus_list */
static LIST_HEAD(lkpi_pci_drivers);		/* pci_driver.lkpi_entry */
static DEFINE_MUTEX(lkpi_pci_lock);

const struct bus_type pci_bus_type = {
	.name = "pci",
};

struct resource iomem_resource = { .name = "PCI mem", .start = 0,
	.end = ~(resource_size_t)0, .flags = IORESOURCE_MEM };
struct resource ioport_resource = { .name = "PCI IO", .start = 0,
	.end = 0xffff, .flags = IORESOURCE_IO };

struct lkpi_pci_iomap {
	struct list_head	entry;
	void __iomem		*addr;
	void			*handle;
	int			bar;
	bool			managed;
};

static void
lkpi_pci_release(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	mutex_lock(&lkpi_pci_lock);
	list_del(&pdev->bus_list);
	mutex_unlock(&lkpi_pci_lock);
	WARN_ON(!list_empty(&pdev->lkpi_iomaps));
	if (pdev->lkpi_cfg != NULL)
		lkpi_plat_pci_cfg_close(pdev->lkpi_cfg);
	lkpi_plat_dev_rele(pdev->dev.lkpi_dip);
	kfree(pdev);
}

static u8 lkpi_pci_find_cap_from(struct pci_dev *pdev, u8 pos, int cap);

/* Caller holds lkpi_pci_lock. */
static struct pci_dev *
lkpi_pci_lookup_locked(void *dip)
{
	struct pci_dev *pdev;

	list_for_each_entry(pdev, &lkpi_pci_devs, bus_list)
		if (pdev->dev.lkpi_dip == dip)
			return (pdev);
	return (NULL);
}

static struct pci_dev *
lkpi_pci_dev_create(void *dip)
{
	struct lkpi_plat_pci_info info;
	struct pci_dev *pdev;
	u16 status;
	int i;

	if (lkpi_plat_pci_info(dip, &info) != 0)
		return (NULL);
	pdev = kzalloc(sizeof (*pdev), GFP_KERNEL);
	if (pdev == NULL)
		return (NULL);
	if (lkpi_plat_pci_cfg_open(dip, &pdev->lkpi_cfg) != 0) {
		kfree(pdev);
		return (NULL);
	}
	lkpi_plat_dev_hold(dip);
	device_initialize(&pdev->dev);
	pdev->dev.lkpi_dip = dip;
	pdev->dev.bus = &pci_bus_type;
	pdev->dev.release = lkpi_pci_release;
	pdev->dev.dma_mask = &pdev->dma_mask;
	pdev->dma_mask = DMA_BIT_MASK(32);
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	INIT_LIST_HEAD(&pdev->lkpi_iomaps);
	pdev->vendor = info.vendor;
	pdev->device = info.device;
	pdev->subsystem_vendor = info.subvendor;
	pdev->subsystem_device = info.subdevice;
	pdev->class = info.class;
	pdev->revision = info.revision;
	pdev->devfn = info.devfn;
	pdev->lkpi_bus.number = info.bus;
	pdev->lkpi_bus.domain_nr = info.domain;
	INIT_LIST_HEAD(&pdev->lkpi_bus.devices);
	pdev->bus = &pdev->lkpi_bus;
	pdev->current_state = PCI_D0;
	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		struct resource *r = &pdev->resource[i];

		pdev->lkpi_rnumber[i] = info.bar[i].rnumber;
		if (info.bar[i].len == 0)
			continue;
		r->start = info.bar[i].start;
		r->end = info.bar[i].start + info.bar[i].len - 1;
		r->flags = (info.bar[i].flags & LKPI_BAR_IO) ? IORESOURCE_IO :
		    IORESOURCE_MEM;
		if (info.bar[i].flags & LKPI_BAR_64)
			r->flags |= IORESOURCE_MEM_64;
		if (info.bar[i].flags & LKPI_BAR_PREFETCH)
			r->flags |= IORESOURCE_PREFETCH;
		if (info.bar[i].start == 0)
			r->flags |= IORESOURCE_UNSET;
	}
	if (info.rom_len != 0) {
		pdev->resource[PCI_ROM_RESOURCE].start = info.rom_start;
		pdev->resource[PCI_ROM_RESOURCE].end = info.rom_start +
		    info.rom_len - 1;
		pdev->resource[PCI_ROM_RESOURCE].flags = IORESOURCE_MEM |
		    IORESOURCE_READONLY;
	}
	(void) dev_set_name(&pdev->dev, "%04x:%02x:%02x.%u", info.domain,
	    info.bus, PCI_SLOT(info.devfn), PCI_FUNC(info.devfn));
	pdev->hdr_type = (u8)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg,
	    PCI_HEADER_TYPE, 1) & PCI_HEADER_TYPE_MASK;
	pdev->pin = (u8)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg,
	    PCI_INTERRUPT_PIN, 1);
	status = (u16)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg, PCI_STATUS, 2);
	if (status & PCI_STATUS_CAP_LIST) {
		pdev->pcie_cap = lkpi_pci_find_cap_from(pdev,
		    PCI_CAPABILITY_LIST, PCI_CAP_ID_EXP);
		pdev->msi_cap = lkpi_pci_find_cap_from(pdev,
		    PCI_CAPABILITY_LIST, PCI_CAP_ID_MSI);
		pdev->msix_cap = lkpi_pci_find_cap_from(pdev,
		    PCI_CAPABILITY_LIST, PCI_CAP_ID_MSIX);
		pdev->pm_cap = lkpi_pci_find_cap_from(pdev,
		    PCI_CAPABILITY_LIST, PCI_CAP_ID_PM);
	}
	if (pdev->pcie_cap != 0)
		pdev->pcie_flags_reg = (u16)lkpi_plat_pci_cfg_read(
		    pdev->lkpi_cfg, pdev->pcie_cap + PCI_EXP_FLAGS, 2);
	pdev->cfg_size = pdev->pcie_cap != 0 ? PCI_CFG_SPACE_EXP_SIZE :
	    PCI_CFG_SPACE_SIZE;
	return (pdev);
}

/*
 * The wrapper for dip, with a new reference (created on first use).
 */
struct pci_dev *
lkpi_pci_dev_alloc(void *dip)
{
	struct pci_dev *pdev, *other;

	mutex_lock(&lkpi_pci_lock);
	pdev = lkpi_pci_lookup_locked(dip);
	if (pdev != NULL && kobject_get_unless_zero(&pdev->dev.kobj) != NULL) {
		mutex_unlock(&lkpi_pci_lock);
		return (pdev);
	}
	mutex_unlock(&lkpi_pci_lock);
	pdev = lkpi_pci_dev_create(dip);
	if (pdev == NULL)
		return (NULL);
	mutex_lock(&lkpi_pci_lock);
	other = lkpi_pci_lookup_locked(dip);
	if (other != NULL && kobject_get_unless_zero(&other->dev.kobj) != NULL) {
		mutex_unlock(&lkpi_pci_lock);
		INIT_LIST_HEAD(&pdev->bus_list);
		put_device(&pdev->dev);
		return (other);
	}
	list_add_tail(&pdev->bus_list, &lkpi_pci_devs);
	mutex_unlock(&lkpi_pci_lock);
	return (pdev);
}

struct pci_dev *
pci_dev_get(struct pci_dev *pdev)
{
	if (pdev != NULL)
		(void) get_device(&pdev->dev);
	return (pdev);
}

void
pci_dev_put(struct pci_dev *pdev)
{
	if (pdev != NULL)
		put_device(&pdev->dev);
}

/* Config space. */

static int
lkpi_cfg_check(const struct pci_dev *pdev, int where, int size)
{
	if (pdev == NULL || pdev->lkpi_cfg == NULL)
		return (PCIBIOS_DEVICE_NOT_FOUND);
	if (where < 0 || (where & (size - 1)) != 0 ||
	    where + size > pdev->cfg_size)
		return (PCIBIOS_BAD_REGISTER_NUMBER);
	return (PCIBIOS_SUCCESSFUL);
}

#define	LKPI_CFG_RD(name, type, size)					\
int									\
name(const struct pci_dev *pdev, int where, type *val)			\
{									\
	int r = lkpi_cfg_check(pdev, where, size);			\
									\
	if (r != 0) {							\
		*val = (type)~0U;					\
		return (r);						\
	}								\
	*val = (type)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg, where, size); \
	return (0);							\
}
#define	LKPI_CFG_WR(name, type, size)					\
int									\
name(const struct pci_dev *pdev, int where, type val)			\
{									\
	int r = lkpi_cfg_check(pdev, where, size);			\
									\
	if (r != 0)							\
		return (r);						\
	lkpi_plat_pci_cfg_write(pdev->lkpi_cfg, where, size, val);	\
	return (0);							\
}

LKPI_CFG_RD(pci_read_config_byte, u8, 1)
LKPI_CFG_RD(pci_read_config_word, u16, 2)
LKPI_CFG_RD(pci_read_config_dword, u32, 4)
LKPI_CFG_WR(pci_write_config_byte, u8, 1)
LKPI_CFG_WR(pci_write_config_word, u16, 2)
LKPI_CFG_WR(pci_write_config_dword, u32, 4)

/* Walk the capability list from the pointer at pos (max 48 entries). */
static u8
lkpi_pci_find_cap_from(struct pci_dev *pdev, u8 pos, int cap)
{
	int ttl = 48;
	u8 p;

	p = (u8)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg, pos, 1) & ~3;
	while (p >= 0x40 && ttl-- > 0) {
		if ((int)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg,
		    p + PCI_CAP_LIST_ID, 1) == cap)
			return (p);
		p = (u8)lkpi_plat_pci_cfg_read(pdev->lkpi_cfg,
		    p + PCI_CAP_LIST_NEXT, 1) & ~3;
	}
	return (0);
}

u8
pci_find_capability(struct pci_dev *pdev, int cap)
{
	u16 status;

	if (pci_read_config_word(pdev, PCI_STATUS, &status) != 0 ||
	    !(status & PCI_STATUS_CAP_LIST))
		return (0);
	return (lkpi_pci_find_cap_from(pdev, PCI_CAPABILITY_LIST, cap));
}

u8
pci_find_next_capability(struct pci_dev *pdev, u8 pos, int cap)
{
	return (lkpi_pci_find_cap_from(pdev, pos + PCI_CAP_LIST_NEXT, cap));
}

u16
pci_find_next_ext_capability(struct pci_dev *pdev, u16 start, int cap)
{
	int ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;
	u16 pos = PCI_CFG_SPACE_SIZE;
	u32 h;

	if (pdev->cfg_size <= PCI_CFG_SPACE_SIZE)
		return (0);
	if (start != 0) {
		if (pci_read_config_dword(pdev, start, &h) != 0)
			return (0);
		pos = (u16)PCI_EXT_CAP_NEXT(h);
	}
	while (pos >= PCI_CFG_SPACE_SIZE && ttl-- > 0) {
		if (pci_read_config_dword(pdev, pos, &h) != 0 || h == 0 ||
		    h == ~0U)
			return (0);
		if ((int)PCI_EXT_CAP_ID(h) == cap && pos != start)
			return (pos);
		pos = (u16)PCI_EXT_CAP_NEXT(h);
	}
	return (0);
}

u16
pci_find_ext_capability(struct pci_dev *pdev, int cap)
{
	return (pci_find_next_ext_capability(pdev, 0, cap));
}

int
pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val)
{
	*val = 0;
	if (pdev->pcie_cap == 0)
		return (-EINVAL);
	return (pci_read_config_word(pdev, pdev->pcie_cap + pos, val) ?
	    -EIO : 0);
}

int
pcie_capability_read_dword(struct pci_dev *pdev, int pos, u32 *val)
{
	*val = 0;
	if (pdev->pcie_cap == 0)
		return (-EINVAL);
	return (pci_read_config_dword(pdev, pdev->pcie_cap + pos, val) ?
	    -EIO : 0);
}

int
pcie_capability_write_word(struct pci_dev *pdev, int pos, u16 val)
{
	if (pdev->pcie_cap == 0)
		return (-EINVAL);
	return (pci_write_config_word(pdev, pdev->pcie_cap + pos, val) ?
	    -EIO : 0);
}

int
pcie_capability_write_dword(struct pci_dev *pdev, int pos, u32 val)
{
	if (pdev->pcie_cap == 0)
		return (-EINVAL);
	return (pci_write_config_dword(pdev, pdev->pcie_cap + pos, val) ?
	    -EIO : 0);
}

int
pcie_capability_clear_and_set_word(struct pci_dev *pdev, int pos, u16 clear,
    u16 set)
{
	u16 v;
	int r = pcie_capability_read_word(pdev, pos, &v);

	if (r != 0)
		return (r);
	return (pcie_capability_write_word(pdev, pos, (v & ~clear) | set));
}

enum pci_bus_speed
pcie_get_speed_cap(struct pci_dev *pdev)
{
	static const enum pci_bus_speed speeds[] = {
		PCI_SPEED_UNKNOWN, PCIE_SPEED_2_5GT, PCIE_SPEED_5_0GT,
		PCIE_SPEED_8_0GT, PCIE_SPEED_16_0GT, PCIE_SPEED_32_0GT
	};
	u32 cap;

	if (pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &cap) != 0)
		return (PCI_SPEED_UNKNOWN);
	cap &= PCI_EXP_LNKCAP_SLS;
	return (cap < ARRAY_SIZE(speeds) ? speeds[cap] : PCI_SPEED_UNKNOWN);
}

enum pcie_link_width
pcie_get_width_cap(struct pci_dev *pdev)
{
	u32 cap;

	if (pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &cap) != 0)
		return (PCIE_LNK_WIDTH_UNKNOWN);
	return ((enum pcie_link_width)((cap & PCI_EXP_LNKCAP_MLW) >> 4));
}

/* Enable / bus mastering. */

static void
lkpi_pci_cmd_set(struct pci_dev *pdev, u16 clear, u16 set)
{
	u16 cmd;

	if (pci_read_config_word(pdev, PCI_COMMAND, &cmd) != 0)
		return;
	if (((cmd & ~clear) | set) != cmd)
		(void) pci_write_config_word(pdev, PCI_COMMAND,
		    (cmd & ~clear) | set);
}

static int
lkpi_pci_enable(struct pci_dev *pdev, u16 bits)
{
	if (pdev->lkpi_enable_cnt++ == 0)
		lkpi_pci_cmd_set(pdev, 0, bits);
	return (0);
}

int
pci_enable_device(struct pci_dev *pdev)
{
	u16 bits = 0;
	int i;

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		if (pdev->resource[i].flags & IORESOURCE_MEM)
			bits |= PCI_COMMAND_MEMORY;
		if (pdev->resource[i].flags & IORESOURCE_IO)
			bits |= PCI_COMMAND_IO;
	}
	return (lkpi_pci_enable(pdev, bits));
}

int
pci_enable_device_mem(struct pci_dev *pdev)
{
	return (lkpi_pci_enable(pdev, PCI_COMMAND_MEMORY));
}

static void
lkpi_pcim_disable(void *arg)
{
	pci_disable_device(arg);
}

int
pcim_enable_device(struct pci_dev *pdev)
{
	int r = pci_enable_device(pdev);

	if (r != 0)
		return (r);
	pdev->lkpi_managed = true;
	return (devm_add_action_or_reset(&pdev->dev, lkpi_pcim_disable, pdev));
}

/*
 * Disabling clears bus mastering only: memory decoding stays on, because
 * illumos (and the boot console) may still be using the device.
 */
void
pci_disable_device(struct pci_dev *pdev)
{
	if (pdev->lkpi_enable_cnt <= 0) {
		WARN_ONCE(1, "linuxkpi: %s disabled too often\n",
		    pci_name(pdev));
		return;
	}
	if (--pdev->lkpi_enable_cnt == 0)
		pci_clear_master(pdev);
}

void
pci_set_master(struct pci_dev *pdev)
{
	lkpi_pci_cmd_set(pdev, 0, PCI_COMMAND_MASTER);
	pdev->is_busmaster = 1;
}

void
pci_clear_master(struct pci_dev *pdev)
{
	lkpi_pci_cmd_set(pdev, PCI_COMMAND_MASTER, 0);
	pdev->is_busmaster = 0;
}

/* Power states are left to illumos; record what the driver asked for. */
int
pci_set_power_state(struct pci_dev *pdev, pci_power_t state)
{
	pdev->current_state = state;
	return (0);
}

int
pci_save_state(struct pci_dev *pdev)
{
	(void) pdev;
	return (0);
}

void
pci_restore_state(struct pci_dev *pdev)
{
	(void) pdev;
}

int
pci_request_regions(struct pci_dev *pdev, const char *name)
{
	(void) pdev;
	(void) name;
	return (0);
}

void
pci_release_regions(struct pci_dev *pdev)
{
	(void) pdev;
}

int
pci_request_region(struct pci_dev *pdev, int bar, const char *name)
{
	(void) name;
	return (bar >= 0 && bar < PCI_STD_NUM_BARS &&
	    pci_resource_len(pdev, bar) != 0 ? 0 : -EINVAL);
}

void
pci_release_region(struct pci_dev *pdev, int bar)
{
	(void) pdev;
	(void) bar;
}

/* BAR mappings. */

static void __iomem *
lkpi_pci_iomap_common(struct pci_dev *pdev, int bar, unsigned long off,
    unsigned long maxlen, int cache, bool managed)
{
	struct lkpi_pci_iomap *m;
	resource_size_t len;
	void *addr, *handle;

	if (bar < 0 || bar >= PCI_STD_NUM_BARS ||
	    pdev->lkpi_rnumber[bar] < 0 ||
	    !(pdev->resource[bar].flags & IORESOURCE_MEM))
		return (NULL);
	len = pci_resource_len(pdev, bar);
	if (off >= len)
		return (NULL);
	len -= off;
	if (maxlen != 0 && maxlen < len)
		len = maxlen;
	addr = lkpi_plat_map_regs(pdev->dev.lkpi_dip, pdev->lkpi_rnumber[bar],
	    off, len, cache, &handle);
	if (addr == NULL)
		return (NULL);
	m = kzalloc(sizeof (*m), GFP_KERNEL);
	m->addr = addr;
	m->handle = handle;
	m->bar = bar;
	m->managed = managed;
	mutex_lock(&lkpi_pci_lock);
	list_add_tail(&m->entry, &pdev->lkpi_iomaps);
	mutex_unlock(&lkpi_pci_lock);
	return (addr);
}

void __iomem *
pci_iomap_range(struct pci_dev *pdev, int bar, unsigned long offset,
    unsigned long maxlen)
{
	return (lkpi_pci_iomap_common(pdev, bar, offset, maxlen,
	    LKPI_CACHE_ATTR_UC, false));
}

void __iomem *
pci_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen)
{
	return (pci_iomap_range(pdev, bar, 0, maxlen));
}

void __iomem *
pci_iomap_wc_range(struct pci_dev *pdev, int bar, unsigned long offset,
    unsigned long maxlen)
{
	return (lkpi_pci_iomap_common(pdev, bar, offset, maxlen,
	    LKPI_CACHE_ATTR_WC, false));
}

void __iomem *
pci_iomap_wc(struct pci_dev *pdev, int bar, unsigned long maxlen)
{
	return (pci_iomap_wc_range(pdev, bar, 0, maxlen));
}

static struct lkpi_pci_iomap *
lkpi_pci_iomap_take(struct pci_dev *pdev, void __iomem *addr)
{
	struct lkpi_pci_iomap *m;

	mutex_lock(&lkpi_pci_lock);
	list_for_each_entry(m, &pdev->lkpi_iomaps, entry) {
		if (m->addr == addr) {
			list_del(&m->entry);
			mutex_unlock(&lkpi_pci_lock);
			return (m);
		}
	}
	mutex_unlock(&lkpi_pci_lock);
	return (NULL);
}

void
pci_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	struct lkpi_pci_iomap *m;

	if (addr == NULL)
		return;
	m = lkpi_pci_iomap_take(pdev, addr);
	if (m == NULL) {
		WARN_ONCE(1, "linuxkpi: pci_iounmap of unknown %p\n", addr);
		return;
	}
	lkpi_plat_unmap_regs(&m->handle);
	kfree(m);
}

struct lkpi_pcim_table {
	struct pci_dev		*pdev;
	void __iomem		*table[PCI_STD_NUM_BARS];
};

static void
lkpi_pcim_table_release(struct device *dev, void *res)
{
	struct lkpi_pcim_table *t = res;
	int i;

	(void) dev;
	for (i = 0; i < PCI_STD_NUM_BARS; i++)
		if (t->table[i] != NULL)
			pci_iounmap(t->pdev, t->table[i]);
}

static struct lkpi_pcim_table *
lkpi_pcim_table(struct pci_dev *pdev)
{
	struct lkpi_pcim_table *t;

	t = devres_find(&pdev->dev, lkpi_pcim_table_release, NULL, NULL);
	if (t != NULL)
		return (t);
	t = devres_alloc(lkpi_pcim_table_release, sizeof (*t), GFP_KERNEL);
	if (t == NULL)
		return (NULL);
	t->pdev = pdev;
	devres_add(&pdev->dev, t);
	return (t);
}

void __iomem *
pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen)
{
	struct lkpi_pcim_table *t = lkpi_pcim_table(pdev);

	if (t == NULL || bar < 0 || bar >= PCI_STD_NUM_BARS)
		return (NULL);
	if (t->table[bar] == NULL)
		t->table[bar] = pci_iomap(pdev, bar, maxlen);
	return (t->table[bar]);
}

void
pcim_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	struct lkpi_pcim_table *t = lkpi_pcim_table(pdev);
	int i;

	for (i = 0; t != NULL && i < PCI_STD_NUM_BARS; i++) {
		if (t->table[i] == addr) {
			t->table[i] = NULL;
			pci_iounmap(pdev, addr);
		}
	}
}

int
pcim_iomap_regions(struct pci_dev *pdev, int mask, const char *name)
{
	int i;

	(void) name;
	for (i = 0; i < PCI_STD_NUM_BARS; i++)
		if ((mask & (1 << i)) && pcim_iomap(pdev, i, 0) == NULL)
			return (-ENOMEM);
	return (0);
}

void __iomem *const *
pcim_iomap_table(struct pci_dev *pdev)
{
	struct lkpi_pcim_table *t = lkpi_pcim_table(pdev);

	return (t != NULL ? t->table : NULL);
}

/* Expansion ROM: mapped through its physical address (gfx_private). */
void __iomem *
pci_map_rom(struct pci_dev *pdev, size_t *size)
{
	struct resource *r = &pdev->resource[PCI_ROM_RESOURCE];
	u32 rom;
	void __iomem *va;

	if (r->start == 0 || resource_size(r) == 0)
		return (NULL);
	(void) pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &rom);
	(void) pci_write_config_dword(pdev, PCI_ROM_ADDRESS, rom | 1);
	va = ioremap(r->start, resource_size(r));
	if (va == NULL)
		return (NULL);
	*size = resource_size(r);
	return (va);
}

void
pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom)
{
	u32 v;

	iounmap(rom);
	(void) pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &v);
	(void) pci_write_config_dword(pdev, PCI_ROM_ADDRESS, v & ~1U);
}

void __iomem *
pci_platform_rom(struct pci_dev *pdev, size_t *size)
{
	(void) pdev;
	*size = 0;
	return (NULL);
}

/* Enumeration. */

struct lkpi_pci_match {
	unsigned int	vendor, device, ssvendor, ssdevice;
	unsigned int	class, class_mask;
	int		domain, bus, devfn;	/* -1: any */
	void		*after;		/* resume after this dip */
	bool		passed;
	void		*found;
};

static int
lkpi_pci_match_cb(void *dip, void *arg)
{
	struct lkpi_pci_match *m = arg;
	struct lkpi_plat_pci_info info;

	if (m->after != NULL && !m->passed) {
		if (dip == m->after)
			m->passed = true;
		return (0);
	}
	if (lkpi_plat_pci_info(dip, &info) != 0)
		return (0);
	if ((m->vendor != PCI_ANY_ID && m->vendor != info.vendor) ||
	    (m->device != PCI_ANY_ID && m->device != info.device) ||
	    (m->ssvendor != PCI_ANY_ID && m->ssvendor != info.subvendor) ||
	    (m->ssdevice != PCI_ANY_ID && m->ssdevice != info.subdevice) ||
	    ((info.class ^ m->class) & m->class_mask) != 0 ||
	    (m->domain >= 0 && m->domain != info.domain) ||
	    (m->bus >= 0 && m->bus != info.bus) ||
	    (m->devfn >= 0 && m->devfn != info.devfn))
		return (0);
	m->found = dip;
	lkpi_plat_dev_hold(dip);
	return (1);
}

static struct pci_dev *
lkpi_pci_search(struct lkpi_pci_match *m, struct pci_dev *from)
{
	struct pci_dev *pdev = NULL;

	m->after = from != NULL ? from->dev.lkpi_dip : NULL;
	m->passed = false;
	m->found = NULL;
	lkpi_plat_pci_walk(lkpi_pci_match_cb, m);
	if (m->found != NULL) {
		pdev = lkpi_pci_dev_alloc(m->found);
		lkpi_plat_dev_rele(m->found);
	}
	pci_dev_put(from);
	return (pdev);
}

struct pci_dev *
pci_get_subsys(unsigned int vendor, unsigned int device,
    unsigned int ss_vendor, unsigned int ss_device, struct pci_dev *from)
{
	struct lkpi_pci_match m = { vendor, device, ss_vendor, ss_device,
	    0, 0, -1, -1, -1, NULL, false, NULL };

	return (lkpi_pci_search(&m, from));
}

struct pci_dev *
pci_get_device(unsigned int vendor, unsigned int device,
    struct pci_dev *from)
{
	return (pci_get_subsys(vendor, device, PCI_ANY_ID, PCI_ANY_ID, from));
}

struct pci_dev *
pci_get_class(unsigned int class, struct pci_dev *from)
{
	struct lkpi_pci_match m = { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	    PCI_ANY_ID, class, 0xffffff, -1, -1, -1, NULL, false, NULL };

	return (lkpi_pci_search(&m, from));
}

struct pci_dev *
pci_get_base_class(unsigned int class, struct pci_dev *from)
{
	struct lkpi_pci_match m = { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	    PCI_ANY_ID, class << 16, 0xff0000, -1, -1, -1, NULL, false, NULL };

	return (lkpi_pci_search(&m, from));
}

struct pci_dev *
pci_get_domain_bus_and_slot(int domain, unsigned int bus, unsigned int devfn)
{
	struct lkpi_pci_match m = { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	    PCI_ANY_ID, 0, 0, domain, (int)bus, (int)devfn, NULL, false,
	    NULL };

	return (lkpi_pci_search(&m, NULL));
}

bool
lkpi_pci_dev_present(const struct pci_device_id *ids)
{
	struct pci_dev *pdev;

	for (; ids->vendor != 0 || ids->subvendor != 0 || ids->class_mask != 0;
	    ids++) {
		pdev = pci_get_subsys(ids->vendor, ids->device, ids->subvendor,
		    ids->subdevice, NULL);
		if (pdev != NULL) {
			pci_dev_put(pdev);
			return (true);
		}
	}
	return (false);
}

/* No bridge topology is modelled: no upstream bridge is reported. */
struct pci_dev *
pci_upstream_bridge(struct pci_dev *pdev)
{
	(void) pdev;
	return (NULL);
}

static int
lkpi_bus_cfg(struct pci_bus *bus, unsigned int devfn, int where, int size,
    u32 *val, bool write)
{
	struct pci_dev *pdev;
	int r;

	pdev = pci_get_domain_bus_and_slot(bus->domain_nr, bus->number, devfn);
	if (pdev == NULL) {
		if (!write)
			*val = ~0U;
		return (PCIBIOS_DEVICE_NOT_FOUND);
	}
	r = lkpi_cfg_check(pdev, where, size);
	if (r == 0) {
		if (write)
			lkpi_plat_pci_cfg_write(pdev->lkpi_cfg, where, size,
			    *val);
		else
			*val = lkpi_plat_pci_cfg_read(pdev->lkpi_cfg, where,
			    size);
	}
	pci_dev_put(pdev);
	return (r);
}

int
pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn, int where,
    u8 *val)
{
	u32 v;
	int r = lkpi_bus_cfg(bus, devfn, where, 1, &v, false);

	*val = (u8)v;
	return (r);
}

int
pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn, int where,
    u16 *val)
{
	u32 v;
	int r = lkpi_bus_cfg(bus, devfn, where, 2, &v, false);

	*val = (u16)v;
	return (r);
}

int
pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 *val)
{
	return (lkpi_bus_cfg(bus, devfn, where, 4, val, false));
}

int
pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
    int where, u8 val)
{
	u32 v = val;

	return (lkpi_bus_cfg(bus, devfn, where, 1, &v, true));
}

int
pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn,
    int where, u16 val)
{
	u32 v = val;

	return (lkpi_bus_cfg(bus, devfn, where, 2, &v, true));
}

int
pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 val)
{
	return (lkpi_bus_cfg(bus, devfn, where, 4, &val, true));
}

/* Drivers. */

const struct pci_device_id *
pci_match_id(const struct pci_device_id *ids, struct pci_dev *pdev)
{
	if (ids == NULL)
		return (NULL);
	for (; ids->vendor != 0 || ids->subvendor != 0 || ids->class_mask != 0;
	    ids++) {
		if ((ids->vendor == PCI_ANY_ID || ids->vendor == pdev->vendor) &&
		    (ids->device == PCI_ANY_ID || ids->device == pdev->device) &&
		    (ids->subvendor == PCI_ANY_ID ||
		    ids->subvendor == pdev->subsystem_vendor) &&
		    (ids->subdevice == PCI_ANY_ID ||
		    ids->subdevice == pdev->subsystem_device) &&
		    ((ids->class ^ pdev->class) & ids->class_mask) == 0)
			return (ids);
	}
	return (NULL);
}

int
__pci_register_driver(struct pci_driver *drv, struct lkpi_module *owner,
    const char *mod_name)
{
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;
	if (drv->driver.name == NULL)
		drv->driver.name = drv->name;
	drv->driver.bus = &pci_bus_type;
	mutex_lock(&lkpi_pci_lock);
	list_add_tail(&drv->lkpi_entry, &lkpi_pci_drivers);
	mutex_unlock(&lkpi_pci_lock);
	return (0);
}

void
pci_unregister_driver(struct pci_driver *drv)
{
	mutex_lock(&lkpi_pci_lock);
	list_del_init(&drv->lkpi_entry);
	mutex_unlock(&lkpi_pci_lock);
}

struct pci_driver *
lkpi_pci_find_driver(struct pci_dev *pdev, const struct pci_device_id **idp)
{
	struct pci_driver *drv;
	const struct pci_device_id *id;

	mutex_lock(&lkpi_pci_lock);
	list_for_each_entry(drv, &lkpi_pci_drivers, lkpi_entry) {
		id = pci_match_id(drv->id_table, pdev);
		if (id != NULL) {
			mutex_unlock(&lkpi_pci_lock);
			*idp = id;
			return (drv);
		}
	}
	mutex_unlock(&lkpi_pci_lock);
	return (NULL);
}

/* Bind pdev to its registered driver. On failure, devres is unwound. */
int
lkpi_pci_probe(struct pci_dev *pdev)
{
	const struct pci_device_id *id;
	struct pci_driver *drv;
	int r;

	drv = lkpi_pci_find_driver(pdev, &id);
	if (drv == NULL)
		return (-ENODEV);
	pdev->driver = drv;
	pdev->dev.driver = &drv->driver;
	lkpi_intel_gfx_quirk(pdev);	/* early-quirk equivalent */
	r = drv->probe(pdev, id);
	if (r != 0) {
		(void) lkpi_devres_release_all(&pdev->dev);
		pdev->driver = NULL;
		pdev->dev.driver = NULL;
		pci_set_drvdata(pdev, NULL);
	}
	return (r);
}

void
lkpi_pci_remove(struct pci_dev *pdev)
{
	if (pdev->driver != NULL && pdev->driver->remove != NULL)
		pdev->driver->remove(pdev);
	(void) lkpi_devres_release_all(&pdev->dev);
	pdev->driver = NULL;
	pdev->dev.driver = NULL;
	pci_set_drvdata(pdev, NULL);
}

void
lkpi_pci_shutdown(struct pci_dev *pdev)
{
	if (pdev->driver != NULL && pdev->driver->shutdown != NULL)
		pdev->driver->shutdown(pdev);
}

static const struct dev_pm_ops *
lkpi_pci_pm(struct pci_dev *pdev)
{
	return (pdev->driver != NULL ? pdev->driver->driver.pm : NULL);
}

/* DDI_SUSPEND: prepare, suspend, suspend_late, suspend_noirq. */
int
lkpi_pci_suspend(struct pci_dev *pdev)
{
	const struct dev_pm_ops *pm = lkpi_pci_pm(pdev);
	struct device *dev = &pdev->dev;
	int r = 0;

	if (pm == NULL) {
		if (pdev->driver != NULL && pdev->driver->suspend != NULL)
			return (pdev->driver->suspend(pdev, PMSG_SUSPEND));
		return (0);
	}
	if (pm->prepare != NULL && (r = pm->prepare(dev)) != 0)
		return (r);
	if (pm->suspend != NULL && (r = pm->suspend(dev)) != 0)
		goto undo_prepare;
	if (pm->suspend_late != NULL && (r = pm->suspend_late(dev)) != 0)
		goto undo_suspend;
	if (pm->suspend_noirq != NULL && (r = pm->suspend_noirq(dev)) != 0)
		goto undo_late;
	dev->power.is_suspended = true;
	return (0);
undo_late:
	if (pm->resume_early != NULL)
		(void) pm->resume_early(dev);
undo_suspend:
	if (pm->resume != NULL)
		(void) pm->resume(dev);
undo_prepare:
	if (pm->complete != NULL)
		pm->complete(dev);
	return (r);
}

/* DDI_RESUME: resume_noirq, resume_early, resume, complete. */
int
lkpi_pci_resume(struct pci_dev *pdev)
{
	const struct dev_pm_ops *pm = lkpi_pci_pm(pdev);
	struct device *dev = &pdev->dev;
	int r = 0;

	if (pm == NULL) {
		if (pdev->driver != NULL && pdev->driver->resume != NULL)
			return (pdev->driver->resume(pdev));
		return (0);
	}
	if (pm->resume_noirq != NULL)
		r = pm->resume_noirq(dev);
	if (r == 0 && pm->resume_early != NULL)
		r = pm->resume_early(dev);
	if (r == 0 && pm->resume != NULL)
		r = pm->resume(dev);
	if (pm->complete != NULL)
		pm->complete(dev);
	dev->power.is_suspended = false;
	return (r);
}

/* Alignment callback for pci_bus_alloc_resource(), which always fails. */
resource_size_t
pcibios_align_resource(void *data, const struct resource *res,
    resource_size_t size, resource_size_t align)
{
	(void) data;
	(void) size;
	(void) align;
	return (res->start);
}
