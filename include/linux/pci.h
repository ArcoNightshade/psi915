/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux PCI driver API from its
 * documented behaviour (Documentation/PCI/pci.rst); member and function
 * set cross-checked against FreeBSD's LinuxKPI pci.h (BSD-2-Clause).
 *
 * A struct pci_dev wraps an illumos dev_info_t (src/linux_pci.c):
 *
 *   identity/resources  from the PCI properties ("vendor-id", "reg",
 *                       "assigned-addresses", ...) at creation
 *   config space        pci_config_setup(9F) handle, pci_config_get/put
 *   BARs                pci_iomap()/pcim_iomap() map via
 *                       ddi_regs_map_setup(9F) on the BAR's "reg" entry
 *                       (pci_iomap_wc() asks for DDI_MERGING_OK_ACC,
 *                       i.e. PAT write-combining)
 *   interrupts          pci_alloc_irq_vectors()/pci_enable_msi() allocate
 *                       ddi_intr vectors; pci_irq_vector() returns a KPI
 *                       irq number for request_irq() (<linux/interrupt.h>)
 *   enumeration         pci_get_device()/pci_get_class()/
 *                       pci_get_domain_bus_and_slot() walk the devinfo
 *                       tree and return refcounted wrappers (pci_dev_put)
 *   drivers             pci_register_driver() records the driver; the
 *                       illumos attach(9E) glue (<linuxkpi/ddi.h>) matches
 *                       the id_table and calls probe()/remove() and the
 *                       dev_pm_ops on DDI_SUSPEND/DDI_RESUME.
 *
 * Not supported: SR-IOV, hotplug, bus reset, VGA arbitration beyond the
 * no-op vga_* stubs, I/O-port BAR access through pci_iomap() (memory BARs
 * only), runtime PM (D3) transitions.
 */
#ifndef _LINUXKPI_LINUX_PCI_H_
#define	_LINUXKPI_LINUX_PCI_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci_regs.h>
#include <linux/pci_ids.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#define	PCI_ANY_ID		(~0U)
#define	PCI_DEVFN(slot, fn)	((((slot) & 0x1f) << 3) | ((fn) & 0x07))
#define	PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define	PCI_FUNC(devfn)		((devfn) & 0x07)
#define	PCI_DEVID(bus, devfn)	((((u16)(bus)) << 8) | (devfn))
#define	PCI_BUS_NUM(x)		(((x) >> 8) & 0xff)

#define	PCI_STD_NUM_BARS	6
#define	PCI_ROM_RESOURCE	6
#define	DEVICE_COUNT_RESOURCE	7
#define	PCI_NUM_RESOURCES	DEVICE_COUNT_RESOURCE
#define	PCI_STD_RESOURCES	0
/*
 * Bus (bridge window) resources are not modelled: the loop body never
 * runs (only the discrete-GPU LMEM BAR resize uses it).
 */
#define	pci_bus_for_each_resource(bus, res, i)				\
	for ((void)(bus), (i) = 0, (res) = NULL; 0; )
#define	PCI_STD_RESOURCE_END	5
/*
 * Lowest address for new PCI memory allocations (x86 Linux:
 * pci_mem_start, 256 MiB by default); used by i915 to place an
 * unassigned MCHBAR (gen <= 3, not reached on Kaby Lake).
 */
#define	PCIBIOS_MIN_MEM		0x10000000UL

#define	PCI_IRQ_INTX		(1U << 0)
#define	PCI_IRQ_LEGACY		PCI_IRQ_INTX
#define	PCI_IRQ_MSI		(1U << 1)
#define	PCI_IRQ_MSIX		(1U << 2)
#define	PCI_IRQ_AFFINITY	(1U << 3)
#define	PCI_IRQ_ALL_TYPES	(PCI_IRQ_INTX | PCI_IRQ_MSI | PCI_IRQ_MSIX)

typedef int pci_power_t;
#define	PCI_D0		0
#define	PCI_D1		1
#define	PCI_D2		2
#define	PCI_D3hot	3
#define	PCI_D3cold	4
#define	PCI_UNKNOWN	5
typedef unsigned int pci_channel_state_t;
typedef unsigned int pci_ers_result_t;
#define	pci_channel_io_normal		1
#define	pci_channel_io_frozen		2
#define	pci_channel_io_perm_failure	3
#define	PCI_ERS_RESULT_NONE		1
#define	PCI_ERS_RESULT_CAN_RECOVER	2
#define	PCI_ERS_RESULT_NEED_RESET	3
#define	PCI_ERS_RESULT_DISCONNECT	4
#define	PCI_ERS_RESULT_RECOVERED	5

enum pci_bus_speed {
	PCIE_SPEED_2_5GT = 0x14,
	PCIE_SPEED_5_0GT = 0x15,
	PCIE_SPEED_8_0GT = 0x16,
	PCIE_SPEED_16_0GT = 0x17,
	PCIE_SPEED_32_0GT = 0x18,
	PCI_SPEED_UNKNOWN = 0xff
};

enum pcie_link_width {
	PCIE_LNK_WIDTH_UNKNOWN = 0xff
};

struct pci_device_id {
	u32		vendor, device;
	u32		subvendor, subdevice;
	u32		class, class_mask;
	kernel_ulong_t	driver_data;
	u32		override_only;
};

#define	PCI_DEVICE(vend, dev)						\
	.vendor = (vend), .device = (dev),				\
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define	PCI_DEVICE_SUB(vend, dev, subvend, subdev)			\
	.vendor = (vend), .device = (dev),				\
	.subvendor = (subvend), .subdevice = (subdev)
#define	PCI_DEVICE_CLASS(dev_class, dev_class_mask)			\
	.class = (dev_class), .class_mask = (dev_class_mask),		\
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID,			\
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define	PCI_VDEVICE(vend, dev)						\
	.vendor = PCI_VENDOR_ID_##vend, .device = (dev),		\
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0

struct pci_bus {
	struct pci_bus		*parent;
	struct pci_dev		*self;		/* bridge, NULL for root */
	unsigned char		number;
	int			domain_nr;
	struct list_head	devices;
};

struct pci_driver;
struct lkpi_pci_irqs;

struct pci_dev {
	struct device		dev;
	struct list_head	bus_list;
	struct pci_bus		*bus;
	unsigned int		devfn;
	unsigned short		vendor;
	unsigned short		device;
	unsigned short		subsystem_vendor;
	unsigned short		subsystem_device;
	unsigned int		class;
	u8			revision;
	u8			hdr_type;
	u8			pin;
	u8			pcie_cap;
	u8			msi_cap;
	u8			msix_cap;
	u8			pm_cap;
	u16			pcie_flags_reg;
	int			cfg_size;
	int			irq;
	u64			dma_mask;
	struct pci_driver	*driver;
	struct resource		resource[DEVICE_COUNT_RESOURCE];
	pci_power_t		current_state;
	unsigned int		msi_enabled:1;
	unsigned int		msix_enabled:1;
	unsigned int		is_busmaster:1;
	unsigned int		no_64bit_msi:1;
	unsigned int		is_virtfn:1;
	unsigned int		is_physfn:1;
	unsigned int		untrusted:1;
	unsigned int		external_facing:1;
	unsigned int		broken_intx_masking:1;
	/* KPI private */
	void			*lkpi_cfg;	/* config space handle */
	int			lkpi_rnumber[PCI_STD_NUM_BARS];
	struct list_head	lkpi_iomaps;
	struct lkpi_pci_irqs	*lkpi_irqs;
	int			lkpi_enable_cnt;
	bool			lkpi_attached;	/* bound via attach(9E) */
	bool			lkpi_managed;	/* pcim_enable_device() */
	struct pci_bus		lkpi_bus;	/* own bus record */
};

#define	to_pci_dev(d)		container_of(d, struct pci_dev, dev)
#define	for_each_pci_dev(d)						\
	while (((d) = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, (d))) != NULL)

struct pci_error_handlers {
	pci_ers_result_t (*error_detected)(struct pci_dev *dev,
	    pci_channel_state_t error);
	pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);
	pci_ers_result_t (*slot_reset)(struct pci_dev *dev);
	void (*resume)(struct pci_dev *dev);
};

struct pci_driver {
	const char			*name;
	const struct pci_device_id	*id_table;
	int	(*probe)(struct pci_dev *dev, const struct pci_device_id *id);
	void	(*remove)(struct pci_dev *dev);
	void	(*shutdown)(struct pci_dev *dev);
	int	(*suspend)(struct pci_dev *dev, pm_message_t state);
	int	(*resume)(struct pci_dev *dev);
	const struct pci_error_handlers	*err_handler;
	struct device_driver		driver;
	/* KPI private */
	struct list_head		lkpi_entry;
};

#define	to_pci_driver(drv)	container_of(drv, struct pci_driver, driver)

static inline const char *
pci_name(const struct pci_dev *pdev)
{
	return (dev_name(&pdev->dev));
}

static inline void *
pci_get_drvdata(struct pci_dev *pdev)
{
	return (dev_get_drvdata(&pdev->dev));
}

static inline void
pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

static inline int
pci_domain_nr(const struct pci_bus *bus)
{
	return (bus->domain_nr);
}

static inline u16
pci_dev_id(const struct pci_dev *pdev)
{
	return (PCI_DEVID(pdev->bus->number, pdev->devfn));
}

#define	pci_resource_start(p, b)	((p)->resource[(b)].start)
#define	pci_resource_end(p, b)		((p)->resource[(b)].end)
#define	pci_resource_flags(p, b)	((p)->resource[(b)].flags)
#define	pci_resource_n(p, b)		(&(p)->resource[(b)])
#define	pci_resource_len(p, b)						\
	((p)->resource[(b)].start == 0 && (p)->resource[(b)].end == 0 ? 0 : \
	    resource_size(&(p)->resource[(b)]))

/* Config space. 0 on success (PCIBIOS_SUCCESSFUL). */
#define	PCIBIOS_SUCCESSFUL		0x00
#define	PCIBIOS_DEVICE_NOT_FOUND	0x86
#define	PCIBIOS_BAD_REGISTER_NUMBER	0x87
int pci_read_config_byte(const struct pci_dev *pdev, int where, u8 *val);
int pci_read_config_word(const struct pci_dev *pdev, int where, u16 *val);
int pci_read_config_dword(const struct pci_dev *pdev, int where, u32 *val);
int pci_write_config_byte(const struct pci_dev *pdev, int where, u8 val);
int pci_write_config_word(const struct pci_dev *pdev, int where, u16 val);
int pci_write_config_dword(const struct pci_dev *pdev, int where, u32 val);
#define	pcibios_err_to_errno(e)		((e) == 0 ? 0 : -EIO)

u8 pci_find_capability(struct pci_dev *pdev, int cap);
u8 pci_find_next_capability(struct pci_dev *pdev, u8 pos, int cap);
u16 pci_find_ext_capability(struct pci_dev *pdev, int cap);
u16 pci_find_next_ext_capability(struct pci_dev *pdev, u16 pos, int cap);

static inline bool
pci_is_pcie(const struct pci_dev *pdev)
{
	return (pdev->pcie_cap != 0);
}

static inline int
pci_pcie_type(const struct pci_dev *pdev)
{
	return ((pdev->pcie_flags_reg & PCI_EXP_FLAGS_TYPE) >> 4);
}

static inline int
pci_pcie_cap(const struct pci_dev *pdev)
{
	return (pdev->pcie_cap);
}

int pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val);
int pcie_capability_read_dword(struct pci_dev *pdev, int pos, u32 *val);
int pcie_capability_write_word(struct pci_dev *pdev, int pos, u16 val);
int pcie_capability_write_dword(struct pci_dev *pdev, int pos, u32 val);
int pcie_capability_clear_and_set_word(struct pci_dev *pdev, int pos,
    u16 clear, u16 set);
#define	pcie_capability_set_word(d, p, s)				\
	pcie_capability_clear_and_set_word(d, p, 0, s)
#define	pcie_capability_clear_word(d, p, c)				\
	pcie_capability_clear_and_set_word(d, p, c, 0)
enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *pdev);
enum pcie_link_width pcie_get_width_cap(struct pci_dev *pdev);

/* Enable/disable, bus mastering, power state. */
int pci_enable_device(struct pci_dev *pdev);
int pci_enable_device_mem(struct pci_dev *pdev);
int pcim_enable_device(struct pci_dev *pdev);
void pci_disable_device(struct pci_dev *pdev);
void pci_set_master(struct pci_dev *pdev);
void pci_clear_master(struct pci_dev *pdev);
int pci_set_power_state(struct pci_dev *pdev, pci_power_t state);
int pci_save_state(struct pci_dev *pdev);
void pci_restore_state(struct pci_dev *pdev);
int pci_request_regions(struct pci_dev *pdev, const char *name);
void pci_release_regions(struct pci_dev *pdev);
int pci_request_region(struct pci_dev *pdev, int bar, const char *name);
void pci_release_region(struct pci_dev *pdev, int bar);
#define	pcim_request_all_regions(p, n)	pci_request_regions(p, n)
#define	pci_is_enabled(p)		((p)->lkpi_enable_cnt > 0)
#define	pci_dev_present(ids)		(lkpi_pci_dev_present(ids))
#define	pci_ignore_hotplug(p)		do { (void)(p); } while (0)
#define	pci_d3cold_disable(p)		do { (void)(p); } while (0)
#define	pci_d3cold_enable(p)		do { (void)(p); } while (0)
#define	pci_wake_from_d3(p, e)		((void)(p), 0)
#define	pci_enable_wake(p, s, e)	((void)(p), 0)
#define	pci_set_mwi(p)			((void)(p), 0)
#define	pci_try_set_mwi(p)		((void)(p), 0)
#define	pci_intx(p, e)			do { (void)(p); } while (0)
#define	pci_dev_run_wake(p)		(false)
#define	pci_is_thunderbolt_attached(p)	(false)
#define	pci_is_root_bus(b)		((b)->parent == NULL)
#define	pci_bus_address(p, b)		pci_resource_start(p, b)
#define	pci_dev_is_disconnected(p)	(false)
#define	pci_rebar_get_possible_sizes(p, b) (0U)
#define	pci_resize_resource(p, b, s)	(-ENOTSUPP)
#define	pci_rebar_bytes_to_size(b)	(0)

/* BAR mappings. */
void __iomem *pci_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen);
void __iomem *pci_iomap_range(struct pci_dev *pdev, int bar,
    unsigned long offset, unsigned long maxlen);
void __iomem *pci_iomap_wc(struct pci_dev *pdev, int bar,
    unsigned long maxlen);
void __iomem *pci_iomap_wc_range(struct pci_dev *pdev, int bar,
    unsigned long offset, unsigned long maxlen);
void pci_iounmap(struct pci_dev *pdev, void __iomem *addr);
void __iomem *pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen);
void pcim_iounmap(struct pci_dev *pdev, void __iomem *addr);
int pcim_iomap_regions(struct pci_dev *pdev, int mask, const char *name);
void __iomem *const *pcim_iomap_table(struct pci_dev *pdev);
#define	pci_ioremap_bar(p, b)		pci_iomap(p, b, 0)
#define	pci_ioremap_wc_bar(p, b)	pci_iomap_wc(p, b, 0)
void __iomem *pci_map_rom(struct pci_dev *pdev, size_t *size);
void pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom);
void __iomem *pci_platform_rom(struct pci_dev *pdev, size_t *size);

/* Interrupt vectors (MSI-X, MSI, INTx via ddi_intr). */
int pci_alloc_irq_vectors(struct pci_dev *pdev, unsigned int min_vecs,
    unsigned int max_vecs, unsigned int flags);
void pci_free_irq_vectors(struct pci_dev *pdev);
int pci_irq_vector(struct pci_dev *pdev, unsigned int nr);
int pci_enable_msi(struct pci_dev *pdev);
void pci_disable_msi(struct pci_dev *pdev);
#define	pci_alloc_irq_vectors_affinity(p, mn, mx, f, a)			\
	pci_alloc_irq_vectors(p, mn, mx, f)
#define	pci_msi_enabled()		(1)
#define	pci_dev_msi_enabled(p)		((p)->msi_enabled || (p)->msix_enabled)

/* Enumeration (refcounted; release with pci_dev_put()). */
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
    struct pci_dev *from);
struct pci_dev *pci_get_subsys(unsigned int vendor, unsigned int device,
    unsigned int ss_vendor, unsigned int ss_device, struct pci_dev *from);
struct pci_dev *pci_get_class(unsigned int class, struct pci_dev *from);
struct pci_dev *pci_get_base_class(unsigned int class, struct pci_dev *from);
struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
    unsigned int devfn);
struct pci_dev *pci_dev_get(struct pci_dev *pdev);
void pci_dev_put(struct pci_dev *pdev);
struct pci_dev *pci_upstream_bridge(struct pci_dev *pdev);
bool lkpi_pci_dev_present(const struct pci_device_id *ids);
#define	pci_get_slot(bus, devfn)					\
	pci_get_domain_bus_and_slot((bus)->domain_nr, (bus)->number, (devfn))
#define	pci_get_bus_and_slot(bus, devfn)				\
	pci_get_domain_bus_and_slot(0, (bus), (devfn))
static inline struct pci_dev *
pci_physfn(struct pci_dev *pdev)
{
	return (pdev);
}

int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
    int where, u8 *val);
int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
    int where, u16 *val);
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 *val);
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
    int where, u32 val);

/* Drivers. */
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
    struct pci_dev *pdev);
int __pci_register_driver(struct pci_driver *drv, struct lkpi_module *owner,
    const char *mod_name);
#define	pci_register_driver(drv)					\
	__pci_register_driver(drv, THIS_MODULE, KBUILD_MODNAME)
void pci_unregister_driver(struct pci_driver *drv);
#define	module_pci_driver(drv)						\
	static int lkpi_pci_init_##drv(void)				\
	{ return (pci_register_driver(&(drv))); }			\
	static void lkpi_pci_exit_##drv(void)				\
	{ pci_unregister_driver(&(drv)); }				\
	module_init(lkpi_pci_init_##drv);				\
	module_exit(lkpi_pci_exit_##drv)

/* VGA arbitration and decoding: not provided on illumos. */
#define	VGA_RSRC_NONE		0x00
#define	VGA_RSRC_LEGACY_IO	0x01
#define	VGA_RSRC_LEGACY_MEM	0x02
#define	VGA_RSRC_LEGACY_MASK	(VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM)
#define	VGA_RSRC_NORMAL_IO	0x04
#define	VGA_RSRC_NORMAL_MEM	0x08
#define	vga_get_uninterruptible(p, r)	((void)(p), 0)
#define	vga_get_interruptible(p, r)	((void)(p), 0)
#define	vga_put(p, r)			do { (void)(p); } while (0)
#define	vga_set_legacy_decoding(p, d)	do { (void)(p); } while (0)
#define	vga_client_register(p, f)	((void)(p), 0)
#define	vga_client_unregister(p)	do { (void)(p); } while (0)
#define	vga_remove_vgacon(p)		((void)(p), 0)
#define	vga_default_device()		((struct pci_dev *)NULL)

/* KPI internals for the DDI glue (<linuxkpi/ddi.h>) and tests. */
struct pci_dev *lkpi_pci_dev_alloc(void *dip);
struct pci_driver *lkpi_pci_find_driver(struct pci_dev *pdev,
    const struct pci_device_id **idp);
int lkpi_pci_probe(struct pci_dev *pdev);
void lkpi_pci_remove(struct pci_dev *pdev);
int lkpi_pci_suspend(struct pci_dev *pdev);
int lkpi_pci_resume(struct pci_dev *pdev);
void lkpi_pci_shutdown(struct pci_dev *pdev);

#endif /* _LINUXKPI_LINUX_PCI_H_ */
