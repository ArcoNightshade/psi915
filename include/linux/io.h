/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux MMIO API from its documented
 * behaviour (Documentation/driver-api/device-io.rst).
 *
 * Accessors are volatile loads/stores with compiler barriers. On x86,
 * uncached (UC) MMIO accesses are not reordered with each other or with
 * earlier normal stores, so readl()/writel() need no fence, as in Linux;
 * the _relaxed forms are identical. Mappings with write-combining need
 * the explicit wmb() Linux code already issues.
 *
 * ioremap*() map a physical range (src/linux_io.c) through the illumos
 * gfx_private module (gfxp_map_kernel_space(): UC, WC via PAT, or WB);
 * prefer pci_iomap() for BARs of the device being driven, which uses
 * ddi_regs_map_setup(9F). memremap() returns ordinary (non-__iomem)
 * memory. The linuxkpi module therefore depends on misc/gfx_private.
 */
#ifndef _LINUXKPI_LINUX_IO_H_
#define	_LINUXKPI_LINUX_IO_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/err.h>
#include <asm/barrier.h>

#define	__lkpi_mmio_rd(type, a) ({					\
	type __v;							\
	barrier();							\
	__v = *(const volatile type *)(a);				\
	barrier();							\
	__v;								\
})
#define	__lkpi_mmio_wr(type, v, a) do {					\
	barrier();							\
	*(volatile type *)(a) = (v);					\
	barrier();							\
} while (0)

static inline u8 readb(const volatile void __iomem *a) { return (__lkpi_mmio_rd(u8, a)); }
static inline u16 readw(const volatile void __iomem *a) { return (__lkpi_mmio_rd(u16, a)); }
static inline u32 readl(const volatile void __iomem *a) { return (__lkpi_mmio_rd(u32, a)); }
static inline u64 readq(const volatile void __iomem *a) { return (__lkpi_mmio_rd(u64, a)); }
static inline void writeb(u8 v, volatile void __iomem *a) { __lkpi_mmio_wr(u8, v, a); }
static inline void writew(u16 v, volatile void __iomem *a) { __lkpi_mmio_wr(u16, v, a); }
static inline void writel(u32 v, volatile void __iomem *a) { __lkpi_mmio_wr(u32, v, a); }
static inline void writeq(u64 v, volatile void __iomem *a) { __lkpi_mmio_wr(u64, v, a); }

#define	readb_relaxed(a)	readb(a)
#define	readw_relaxed(a)	readw(a)
#define	readl_relaxed(a)	readl(a)
#define	readq_relaxed(a)	readq(a)
#define	writeb_relaxed(v, a)	writeb(v, a)
#define	writew_relaxed(v, a)	writew(v, a)
#define	writel_relaxed(v, a)	writel(v, a)
#define	writeq_relaxed(v, a)	writeq(v, a)
#define	__raw_readl(a)		readl(a)
#define	__raw_writel(v, a)	writel(v, a)
#define	__raw_readq(a)		readq(a)
#define	__raw_writeq(v, a)	writeq(v, a)
#define	ioread8(a)		readb(a)
#define	ioread16(a)		readw(a)
#define	ioread32(a)		readl(a)
#define	ioread64(a)		readq(a)
#define	iowrite8(v, a)		writeb(v, a)
#define	iowrite16(v, a)		writew(v, a)
#define	iowrite32(v, a)		writel(v, a)
#define	iowrite64(v, a)		writeq(v, a)
#define	ioread64_lo_hi(a)	readq(a)
#define	iowrite64_lo_hi(v, a)	writeq(v, a)
#define	lo_hi_readq(a)		readq(a)
#define	lo_hi_writeq(v, a)	writeq(v, a)

static inline void
memcpy_toio(volatile void __iomem *dst, const void *src, size_t n)
{
	barrier();
	memcpy((void *)(uintptr_t)dst, src, n);
	barrier();
}

static inline void
memcpy_fromio(void *dst, const volatile void __iomem *src, size_t n)
{
	barrier();
	memcpy(dst, (const void *)(uintptr_t)src, n);
	barrier();
}

static inline void
memset_io(volatile void __iomem *dst, int c, size_t n)
{
	barrier();
	memset((void *)(uintptr_t)dst, c, n);
	barrier();
}

#define	mmiowb()		barrier()

/* Physical-range mappings (src/linux_io.c). */
void __iomem *ioremap(resource_size_t phys, size_t size);
void __iomem *ioremap_wc(resource_size_t phys, size_t size);
void __iomem *ioremap_cache(resource_size_t phys, size_t size);
void iounmap(volatile void __iomem *addr);
#define	ioremap_uc(p, s)	ioremap(p, s)
#define	ioremap_np(p, s)	ioremap(p, s)
#define	ioremap_wt(p, s)	ioremap(p, s)
#define	ioremap_nocache(p, s)	ioremap(p, s)

enum {
	MEMREMAP_WB = 1 << 0,
	MEMREMAP_WT = 1 << 1,
	MEMREMAP_WC = 1 << 2,
	MEMREMAP_ENC = 1 << 3,
	MEMREMAP_DEC = 1 << 4
};
void *memremap(resource_size_t phys, size_t size, unsigned long flags);
void memunmap(void *addr);

struct device;
void __iomem *devm_ioremap(struct device *dev, resource_size_t phys,
    resource_size_t size);
void __iomem *devm_ioremap_wc(struct device *dev, resource_size_t phys,
    resource_size_t size);
void devm_iounmap(struct device *dev, void __iomem *addr);

/* PAT does the work; MTRR write-combining requests are no-ops. */
#define	arch_phys_wc_add(base, size)		(0)
#define	arch_phys_wc_del(handle)		do { (void)(handle); } while (0)
#define	arch_phys_wc_index(handle)		(-1)
#define	arch_io_reserve_memtype_wc(b, s)	(0)
#define	arch_io_free_memtype_wc(b, s)		do { } while (0)
#define	devm_arch_phys_wc_add(d, b, s)		(0)
#define	devm_arch_io_reserve_memtype_wc(d, b, s) (0)

#endif /* _LINUXKPI_LINUX_IO_H_ */
