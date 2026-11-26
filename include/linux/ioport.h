/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. struct resource and helpers. There is no global resource
 * tree: request_mem_region()/request_region() always succeed and
 * release_*() do nothing (illumos already owns resource assignment).
 */
#ifndef _LINUXKPI_LINUX_IOPORT_H_
#define	_LINUXKPI_LINUX_IOPORT_H_

#include <linux/types.h>
#include <linux/compiler.h>

/* Resource descriptors (resource.desc). */
enum {
	IORES_DESC_NONE = 0,
	IORES_DESC_CRASH_KERNEL = 1,
	IORES_DESC_ACPI_TABLES = 2,
	IORES_DESC_ACPI_NV_STORAGE = 3,
	IORES_DESC_PERSISTENT_MEMORY = 4,
	IORES_DESC_PERSISTENT_MEMORY_LEGACY = 5,
	IORES_DESC_DEVICE_PRIVATE_MEMORY = 6,
	IORES_DESC_RESERVED = 7,
	IORES_DESC_SOFT_RESERVED = 8,
	IORES_DESC_CXL = 9
};

struct resource {
	resource_size_t		start;
	resource_size_t		end;	/* inclusive */
	const char		*name;
	unsigned long		flags;
	unsigned long		desc;
	struct resource		*parent, *sibling, *child;
};

#define	IORESOURCE_BITS		0x000000ff
#define	IORESOURCE_TYPE_BITS	0x00001f00
#define	IORESOURCE_IO		0x00000100
#define	IORESOURCE_MEM		0x00000200
#define	IORESOURCE_REG		0x00000300
#define	IORESOURCE_IRQ		0x00000400
#define	IORESOURCE_DMA		0x00000800
#define	IORESOURCE_BUS		0x00001000
#define	IORESOURCE_PREFETCH	0x00002000
#define	IORESOURCE_READONLY	0x00004000
#define	IORESOURCE_CACHEABLE	0x00008000
#define	IORESOURCE_MEM_64	0x00100000
#define	IORESOURCE_UNSET	0x20000000
#define	IORESOURCE_BUSY		0x80000000

#define	DEFINE_RES_MEM(_start, _size)					\
	((struct resource){ .start = (_start),				\
	    .end = (_start) + (_size) - 1, .flags = IORESOURCE_MEM })

static inline resource_size_t
resource_size(const struct resource *res)
{
	return (res->end - res->start + 1);
}

static inline unsigned long
resource_type(const struct resource *res)
{
	return (res->flags & IORESOURCE_TYPE_BITS);
}

static inline bool
resource_contains(const struct resource *r1, const struct resource *r2)
{
	return (r1->start <= r2->start && r1->end >= r2->end);
}

static inline bool
resource_overlaps(const struct resource *r1, const struct resource *r2)
{
	return (r1->start <= r2->end && r1->end >= r2->start);
}

#define	request_mem_region(s, n, name)	((struct resource *)(uintptr_t)1)
#define	request_region(s, n, name)	((struct resource *)(uintptr_t)1)
#define	release_mem_region(s, n)	do { } while (0)
#define	release_region(s, n)		do { } while (0)
#define	devm_request_mem_region(d, s, n, name) ((struct resource *)(uintptr_t)1)
#define	request_resource(root, new)	(0)
#define	release_resource(r)		(0)
#define	insert_resource(root, new)	(0)

extern struct resource iomem_resource;
extern struct resource ioport_resource;

#endif /* _LINUXKPI_LINUX_IOPORT_H_ */
