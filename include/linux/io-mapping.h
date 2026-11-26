/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux io_mapping API from its
 * documented behaviour. As on Linux x86-64, the whole range is mapped
 * write-combined once at creation (ioremap_wc()), and the map functions
 * return offsets into that mapping; unmap is a no-op. The aperture of a
 * Gen9 GPU (256 MiB) fits comfortably in amd64 kernel address space.
 */
#ifndef _LINUXKPI_LINUX_IO_MAPPING_H_
#define	_LINUXKPI_LINUX_IO_MAPPING_H_

#include <linux/types.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/bug.h>

struct io_mapping {
	resource_size_t		base;
	unsigned long		size;
	void __iomem		*iomem;
	unsigned long		prot;	/* unused; pgprot placeholder */
};

static inline struct io_mapping *
io_mapping_init_wc(struct io_mapping *m, resource_size_t base,
    unsigned long size)
{
	m->iomem = ioremap_wc(base, size);
	if (m->iomem == NULL)
		return (NULL);
	m->base = base;
	m->size = size;
	return (m);
}

static inline void
io_mapping_fini(struct io_mapping *m)
{
	iounmap(m->iomem);
	m->iomem = NULL;
}

static inline struct io_mapping *
io_mapping_create_wc(resource_size_t base, unsigned long size)
{
	struct io_mapping *m = kmalloc(sizeof (*m), GFP_KERNEL);

	if (m == NULL)
		return (NULL);
	if (io_mapping_init_wc(m, base, size) == NULL) {
		kfree(m);
		return (NULL);
	}
	return (m);
}

static inline void
io_mapping_free(struct io_mapping *m)
{
	io_mapping_fini(m);
	kfree(m);
}

static inline void __iomem *
io_mapping_map_wc(struct io_mapping *m, unsigned long off, unsigned long size)
{
	WARN_ON(off + size > m->size);
	return ((char __iomem *)m->iomem + off);
}

static inline void __iomem *
io_mapping_map_atomic_wc(struct io_mapping *m, unsigned long off)
{
	return ((char __iomem *)m->iomem + off);
}

#define	io_mapping_map_local_wc(m, off)	io_mapping_map_atomic_wc(m, off)
#define	io_mapping_unmap(v)		do { (void)(v); } while (0)
#define	io_mapping_unmap_atomic(v)	do { (void)(v); } while (0)
#define	io_mapping_unmap_local(v)	do { (void)(v); } while (0)

#endif /* _LINUXKPI_LINUX_IO_MAPPING_H_ */
