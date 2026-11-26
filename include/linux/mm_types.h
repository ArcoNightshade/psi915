/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. struct page (renamed lkpi_page by <linuxkpi/platform.h>,
 * since illumos has its own struct page/page_t).
 *
 * A KPI page describes one page of physically contiguous kernel memory
 * allocated by alloc_pages() (src/linux_page.c); it is not the illumos
 * page_t. amd64 has no highmem, so every page has a permanent kernel
 * virtual address (lkpi_kva). For an order-n allocation the descriptors
 * form an array of 2^n entries and the first (head) holds the reference
 * count and the order.
 */
#ifndef _LINUXKPI_LINUX_MM_TYPES_H_
#define	_LINUXKPI_LINUX_MM_TYPES_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/pgtable.h>
#include <linux/gfp.h>

struct address_space;
struct lkpi_file;
struct mm_struct;
struct vm_operations_struct;

struct page {
	unsigned long		flags;
	struct list_head	lru;
	struct address_space	*mapping;
	pgoff_t			index;
	unsigned long		private;
	atomic_t		_refcount;
	atomic_t		_mapcount;
	/* KPI private */
	void			*lkpi_kva;
	phys_addr_t		lkpi_phys;
	struct page		*lkpi_head;
	unsigned int		lkpi_order;
	void			*lkpi_cookie;
};

struct folio {
	struct page		page;
};

/*
 * VMAs (moved from drmglue, docs/10 R3). The DRM glue (drmglue/src/vma.c)
 * creates them over illumos devmap handles and implements the helpers
 * declared in <linux/mm.h>.
 */
typedef unsigned long vm_flags_t;

#define	VM_NONE		0x00000000UL
#define	VM_READ		0x00000001UL
#define	VM_WRITE	0x00000002UL
#define	VM_EXEC		0x00000004UL
#define	VM_SHARED	0x00000008UL
#define	VM_MAYREAD	0x00000010UL
#define	VM_MAYWRITE	0x00000020UL
#define	VM_MAYEXEC	0x00000040UL
#define	VM_MAYSHARE	0x00000080UL
#define	VM_PFNMAP	0x00000400UL
#define	VM_LOCKED	0x00002000UL
#define	VM_IO		0x00004000UL
#define	VM_DONTCOPY	0x00020000UL
#define	VM_DONTEXPAND	0x00040000UL
#define	VM_NORESERVE	0x00200000UL
#define	VM_DONTDUMP	0x04000000UL
#define	VM_MIXEDMAP	0x10000000UL

pgprot_t vm_get_page_prot(vm_flags_t flags);

struct vm_area_struct {
	unsigned long			vm_start;
	unsigned long			vm_end;
	unsigned long			vm_pgoff;
	/* Linux 6.3+: read vm_flags, modify through vm_flags_*(). */
	union {
		const vm_flags_t	vm_flags;
		vm_flags_t		__vm_flags;
	};
	pgprot_t			vm_page_prot;
	const struct vm_operations_struct *vm_ops;
	void				*vm_private_data;
	struct lkpi_file		*vm_file;
	struct mm_struct		*vm_mm;
};

static inline void
vm_flags_init(struct vm_area_struct *vma, vm_flags_t f)
{
	vma->__vm_flags = f;
}

static inline void
vm_flags_set(struct vm_area_struct *vma, vm_flags_t f)
{
	vma->__vm_flags |= f;
}

static inline void
vm_flags_clear(struct vm_area_struct *vma, vm_flags_t f)
{
	vma->__vm_flags &= ~f;
}

static inline void
vm_flags_mod(struct vm_area_struct *vma, vm_flags_t set, vm_flags_t clear)
{
	vma->__vm_flags = (vma->__vm_flags | set) & ~clear;
}

static inline unsigned long
vma_pages(const struct vm_area_struct *vma)
{
	return ((vma->vm_end - vma->vm_start) >> PAGE_SHIFT);
}

/* Replace vm_file, moving the reference (Linux vma_set_file). */
void vma_set_file(struct vm_area_struct *vma, struct lkpi_file *file);

typedef unsigned int vm_fault_t;

#define	VM_FAULT_OOM		0x000001U
#define	VM_FAULT_SIGBUS		0x000002U
#define	VM_FAULT_MAJOR		0x000004U
#define	VM_FAULT_HWPOISON	0x000010U
#define	VM_FAULT_HWPOISON_LARGE	0x000020U
#define	VM_FAULT_SIGSEGV	0x000040U
#define	VM_FAULT_NOPAGE		0x000100U
#define	VM_FAULT_LOCKED		0x000200U
#define	VM_FAULT_RETRY		0x000400U
#define	VM_FAULT_FALLBACK	0x000800U
#define	VM_FAULT_DONE_COW	0x001000U
#define	VM_FAULT_ERROR		(VM_FAULT_OOM | VM_FAULT_SIGBUS | \
				    VM_FAULT_SIGSEGV | VM_FAULT_HWPOISON | \
				    VM_FAULT_HWPOISON_LARGE | VM_FAULT_FALLBACK)

#define	FAULT_FLAG_WRITE	0x01U
#define	FAULT_FLAG_MKWRITE	0x02U
#define	FAULT_FLAG_ALLOW_RETRY	0x04U
#define	FAULT_FLAG_RETRY_NOWAIT	0x08U
#define	FAULT_FLAG_KILLABLE	0x10U
#define	FAULT_FLAG_TRIED	0x20U
#define	FAULT_FLAG_USER		0x40U
#define	FAULT_FLAG_REMOTE	0x80U
#define	FAULT_FLAG_INTERRUPTIBLE 0x200U

struct vm_fault {
	struct vm_area_struct	*vma;
	unsigned int		flags;
	gfp_t			gfp_mask;
	pgoff_t			pgoff;
	unsigned long		address;
	struct page		*page;
};

struct vm_operations_struct {
	void		(*open)(struct vm_area_struct *);
	void		(*close)(struct vm_area_struct *);
	vm_fault_t	(*fault)(struct vm_fault *);
	vm_fault_t	(*page_mkwrite)(struct vm_fault *);
	int		(*access)(struct vm_area_struct *, unsigned long, void *,
			    int, int);
	const char	*(*name)(struct vm_area_struct *);
};

#define	page_folio(p)		((struct folio *)(p))
#define	folio_page(f, n)	(&(f)->page + (n))

#endif /* _LINUXKPI_LINUX_MM_TYPES_H_ */
