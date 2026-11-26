/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Physical-range mappings (see <linux/io.h>). Clean-room. Mappings are
 * recorded (address -> length) so iounmap()/memunmap() can undo them.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <linuxkpi/plat_dev.h>

struct lkpi_iomem {
	struct list_head	entry;
	void			*va;
	size_t			len;
};

static LIST_HEAD(lkpi_iomem_list);
static DEFINE_SPINLOCK(lkpi_iomem_lock);

static void *
lkpi_ioremap(resource_size_t phys, size_t size, int cache)
{
	struct lkpi_iomem *m;
	void *va;

	if (size == 0)
		return (NULL);
	m = kmalloc(sizeof (*m), GFP_KERNEL);
	if (m == NULL)
		return (NULL);
	va = lkpi_plat_map_phys(phys, size, cache);
	if (va == NULL) {
		kfree(m);
		return (NULL);
	}
	m->va = va;
	m->len = size;
	spin_lock(&lkpi_iomem_lock);
	list_add(&m->entry, &lkpi_iomem_list);
	spin_unlock(&lkpi_iomem_lock);
	return (va);
}

static void
lkpi_iounmap(const volatile void *va)
{
	struct lkpi_iomem *m, *found = NULL;

	if (va == NULL)
		return;
	spin_lock(&lkpi_iomem_lock);
	list_for_each_entry(m, &lkpi_iomem_list, entry) {
		if (m->va == va) {
			list_del(&m->entry);
			found = m;
			break;
		}
	}
	spin_unlock(&lkpi_iomem_lock);
	if (found == NULL) {
		WARN_ONCE(1, "linuxkpi: unmap of unknown mapping %p\n", va);
		return;
	}
	lkpi_plat_unmap_phys(found->va, found->len);
	kfree(found);
}

void __iomem *
ioremap(resource_size_t phys, size_t size)
{
	return (lkpi_ioremap(phys, size, LKPI_CACHE_ATTR_UC));
}

void __iomem *
ioremap_wc(resource_size_t phys, size_t size)
{
	return (lkpi_ioremap(phys, size, LKPI_CACHE_ATTR_WC));
}

void __iomem *
ioremap_cache(resource_size_t phys, size_t size)
{
	return (lkpi_ioremap(phys, size, LKPI_CACHE_ATTR_WB));
}

void
iounmap(volatile void __iomem *addr)
{
	lkpi_iounmap(addr);
}

void *
memremap(resource_size_t phys, size_t size, unsigned long flags)
{
	if (flags & MEMREMAP_WB)
		return (lkpi_ioremap(phys, size, LKPI_CACHE_ATTR_WB));
	if (flags & (MEMREMAP_WC | MEMREMAP_WT))
		return (lkpi_ioremap(phys, size, LKPI_CACHE_ATTR_WC));
	return (NULL);
}

void
memunmap(void *addr)
{
	lkpi_iounmap(addr);
}

static void
lkpi_devm_iounmap_release(struct device *dev, void *res)
{
	(void) dev;
	iounmap(*(void **)res);
}

static void __iomem *
lkpi_devm_ioremap(struct device *dev, resource_size_t phys,
    resource_size_t size, int cache)
{
	void **p = devres_alloc(lkpi_devm_iounmap_release, sizeof (*p),
	    GFP_KERNEL);

	if (p == NULL)
		return (NULL);
	*p = lkpi_ioremap(phys, (size_t)size, cache);
	if (*p == NULL) {
		devres_free(p);
		return (NULL);
	}
	devres_add(dev, p);
	return (*p);
}

void __iomem *
devm_ioremap(struct device *dev, resource_size_t phys, resource_size_t size)
{
	return (lkpi_devm_ioremap(dev, phys, size, LKPI_CACHE_ATTR_UC));
}

void __iomem *
devm_ioremap_wc(struct device *dev, resource_size_t phys,
    resource_size_t size)
{
	return (lkpi_devm_ioremap(dev, phys, size, LKPI_CACHE_ATTR_WC));
}

static int
lkpi_devm_iomap_match(struct device *dev, void *res, void *data)
{
	(void) dev;
	return (*(void **)res == data);
}

void
devm_iounmap(struct device *dev, void __iomem *addr)
{
	WARN_ON(devres_release(dev, lkpi_devm_iounmap_release,
	    lkpi_devm_iomap_match, (void *)addr) != 0);
}
