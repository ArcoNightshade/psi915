/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of struct iosys_map from its documented
 * behaviour: a pointer to either system memory or I/O memory, with
 * accessors that use plain memory operations or the MMIO accessors
 * (<linux/io.h>) accordingly.
 */
#ifndef _LINUXKPI_LINUX_IOSYS_MAP_H_
#define	_LINUXKPI_LINUX_IOSYS_MAP_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/io.h>

struct iosys_map {
	union {
		void __iomem	*vaddr_iomem;
		void		*vaddr;
	};
	bool			is_iomem;
};

#define	IOSYS_MAP_INIT_VADDR(v)		{ .vaddr = (v), .is_iomem = false }
#define	IOSYS_MAP_INIT_VADDR_IOMEM(v)	{ .vaddr_iomem = (v), .is_iomem = true }
#define	IOSYS_MAP_INIT_OFFSET(map, off) ({				\
	struct iosys_map __c = *(map);					\
	iosys_map_incr(&__c, (off));					\
	__c;								\
})

static inline void
iosys_map_set_vaddr(struct iosys_map *m, void *v)
{
	m->vaddr = v;
	m->is_iomem = false;
}

static inline void
iosys_map_set_vaddr_iomem(struct iosys_map *m, void __iomem *v)
{
	m->vaddr_iomem = v;
	m->is_iomem = true;
}

static inline bool
iosys_map_is_equal(const struct iosys_map *a, const struct iosys_map *b)
{
	return (a->is_iomem == b->is_iomem && a->vaddr == b->vaddr);
}

static inline bool
iosys_map_is_null(const struct iosys_map *m)
{
	return (m->vaddr == NULL);
}

static inline bool
iosys_map_is_set(const struct iosys_map *m)
{
	return (!iosys_map_is_null(m));
}

static inline void
iosys_map_clear(struct iosys_map *m)
{
	m->vaddr = NULL;
	m->is_iomem = false;
}

static inline void
iosys_map_incr(struct iosys_map *m, size_t incr)
{
	m->vaddr = (char *)m->vaddr + incr;
}

static inline void
iosys_map_memcpy_to(struct iosys_map *dst, size_t off, const void *src,
    size_t len)
{
	if (dst->is_iomem)
		memcpy_toio((char __iomem *)dst->vaddr_iomem + off, src, len);
	else
		memcpy((char *)dst->vaddr + off, src, len);
}

static inline void
iosys_map_memcpy_from(void *dst, const struct iosys_map *src, size_t off,
    size_t len)
{
	if (src->is_iomem)
		memcpy_fromio(dst, (char __iomem *)src->vaddr_iomem + off, len);
	else
		memcpy(dst, (char *)src->vaddr + off, len);
}

static inline void
iosys_map_memset(struct iosys_map *dst, size_t off, int value, size_t len)
{
	if (dst->is_iomem)
		memset_io((char __iomem *)dst->vaddr_iomem + off, value, len);
	else
		memset((char *)dst->vaddr + off, value, len);
}

static inline u64
lkpi_iosys_rd_io(const void __iomem *a, size_t size)
{
	u64 v = 0;

	switch (size) {
	case 1: return (readb(a));
	case 2: return (readw(a));
	case 4: return (readl(a));
	case 8: return (readq(a));
	default:
		memcpy_fromio(&v, a, size < 8 ? size : 8);
		return (v);
	}
}

static inline void
lkpi_iosys_wr_io(void __iomem *a, size_t size, u64 v)
{
	switch (size) {
	case 1: writeb((u8)v, a); break;
	case 2: writew((u16)v, a); break;
	case 4: writel((u32)v, a); break;
	case 8: writeq(v, a); break;
	default: memcpy_toio(a, &v, size < 8 ? size : 8); break;
	}
}

#define	iosys_map_rd(map, off, type) ({					\
	type __v;							\
	if ((map)->is_iomem) {						\
		u64 __t = lkpi_iosys_rd_io(				\
		    (char __iomem *)(map)->vaddr_iomem + (off),		\
		    sizeof (type));					\
		memcpy(&__v, &__t, sizeof (type));			\
	} else {							\
		memcpy(&__v, (char *)(map)->vaddr + (off), sizeof (type)); \
	}								\
	__v;								\
})

#define	iosys_map_wr(map, off, type, val) do {				\
	type __v = (val);						\
	if ((map)->is_iomem) {						\
		u64 __t = 0;						\
		memcpy(&__t, &__v, sizeof (type));			\
		lkpi_iosys_wr_io((char __iomem *)(map)->vaddr_iomem + (off), \
		    sizeof (type), __t);				\
	} else {							\
		memcpy((char *)(map)->vaddr + (off), &__v, sizeof (type)); \
	}								\
} while (0)

#define	iosys_map_rd_field(map, struct_off, struct_type, field)		\
	iosys_map_rd(map, (struct_off) + offsetof(struct_type, field),	\
	    typeof(((struct_type *)0)->field))
#define	iosys_map_wr_field(map, struct_off, struct_type, field, val)	\
	iosys_map_wr(map, (struct_off) + offsetof(struct_type, field),	\
	    typeof(((struct_type *)0)->field), val)

#endif /* _LINUXKPI_LINUX_IOSYS_MAP_H_ */
