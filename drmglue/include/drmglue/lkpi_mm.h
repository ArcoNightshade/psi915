/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux VMA emulation over illumos devmap/segdev (docs/10, section 3).
 *
 * Linux types: struct vm_area_struct, struct vm_operations_struct,
 * struct vm_fault, vm_fault_t, pgprot_t, VM_* and FAULT_FLAG_* values,
 * and the PTE-installing helpers drivers call from mmap and fault
 * handlers (vmf_insert_pfn*, remap_pfn_range, vm_insert_page,
 * zap_vma_ptes, unmap_mapping_range). Clean-room from the documented API.
 *
 * REQUESTED CHANGE (docs/10, R3): the Linux-named parts belong in
 * include/linux/mm.h, mm_types.h and pgtable.h of the KPI.
 *
 * Model. Each user mapping (one mmap(2), or one piece of it after a
 * partial munmap or fork) is a struct lkpi_vma wrapping the Linux
 * vm_area_struct the driver sees. A Linux "PTE" is a page of a per-VMA
 * kernel window: drivers' pfn insertions load the window (kernel hat),
 * and the devmap access callback copies window translations into the
 * user's hat with devmap_load(). Zapping (unmap_mapping_range) unloads
 * the user translations with devmap_unload() and the window pages.
 */
#ifndef _DRMGLUE_LKPI_MM_H_
#define	_DRMGLUE_LKPI_MM_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/itree.h>

#ifndef PAGE_SHIFT
#define	PAGE_SHIFT	12
#endif
#ifndef PAGE_SIZE
#define	PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
#define	PAGE_MASK	(~(PAGE_SIZE - 1))
#endif
#ifndef PAGE_ALIGN
#define	PAGE_ALIGN(x)	(((x) + PAGE_SIZE - 1) & PAGE_MASK)
#endif

struct page;		/* KPI struct page (P2) */
struct mm_struct;

/*
 * Page protection: protection bits plus a cache mode. Only the cache mode
 * is honoured per mapping (illumos devmap handles carry one attribute).
 */
enum lkpi_cache_mode {
	LKPI_CACHE_WB = 0,	/* write-back (vm_get_page_prot) */
	LKPI_CACHE_WC = 1,	/* write-combining (pgprot_writecombine) */
	LKPI_CACHE_UC = 2	/* uncached (pgprot_noncached) */
};

typedef struct {
	unsigned long	pgprot;
} pgprot_t;

#define	LKPI_PGPROT_CACHE_MASK	0x3UL
#define	LKPI_PGPROT_READ	0x10UL
#define	LKPI_PGPROT_WRITE	0x20UL
#define	LKPI_PGPROT_EXEC	0x40UL

#define	__pgprot(x)		((pgprot_t){ (x) })
#define	pgprot_val(p)		((p).pgprot)

static inline enum lkpi_cache_mode
lkpi_pgprot_cache(pgprot_t p)
{
	return ((enum lkpi_cache_mode)(p.pgprot & LKPI_PGPROT_CACHE_MASK));
}

static inline pgprot_t
lkpi_pgprot_set_cache(pgprot_t p, enum lkpi_cache_mode c)
{
	return (__pgprot((p.pgprot & ~LKPI_PGPROT_CACHE_MASK) |
	    (unsigned long)c));
}

#define	pgprot_writecombine(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_WC)
#define	pgprot_noncached(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_UC)
#define	pgprot_device(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_UC)
#define	pgprot_decrypted(p)	(p)

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

/*
 * PTE-installing helpers (src/vma.c). addr is a user address inside the
 * vma; only (addr - vm_start) matters. A page that is already present is
 * left alone (Linux insert_pfn semantics).
 */
vm_fault_t vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn);
vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, pgprot_t prot);
int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, unsigned long size, pgprot_t prot);
#define	io_remap_pfn_range(v, a, p, s, pr)	remap_pfn_range(v, a, p, s, pr)
int vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page);
vm_fault_t vmf_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page);
void zap_vma_ptes(struct vm_area_struct *vma, unsigned long addr,
    unsigned long size);
void unmap_mapping_range(struct address_space *mapping, loff_t holebegin,
    loff_t holelen, int even_cows);

/*
 * Batched insertion of n contiguous-in-address pfns from pfns[] (or, if
 * pfns is NULL, pfn_base + i). For i915's remap_io_sg/remap_io_mapping
 * replacement (docs/10, R7). Returns 0 or -EINVAL/-EFAULT.
 */
int lkpi_vma_insert_pfns(struct vm_area_struct *vma, unsigned long addr,
    const unsigned long *pfns, unsigned long pfn_base, unsigned long n,
    pgprot_t prot);

/* KPI hooks this file needs (requested change R5; struct page is P2). */
unsigned long lkpi_page_to_pfn(struct page *page);
void lkpi_put_page(struct page *page);

/*
 * The glue's VMA object (src/vma.c). One per user mapping piece.
 *
 * Lifetime: created by lkpi_vma_create() from devmap(9E), owned by the
 * devmap handle (its private pointer) from devmap_map until devmap_unmap,
 * then destroyed. No reference count: segdev guarantees fault and unmap
 * callbacks of one handle never overlap (both run under the address-space
 * lock, reader and writer respectively), and zappers reach a lkpi_vma only
 * under i_mmap_rwsem, which destruction takes as writer first.
 *
 * Locks, in acquisition order (docs/10, section 3.6):
 *   driver locks (object, GGTT, ...)  ->  mapping->i_mmap_rwsem  ->  lv->lock
 * lv->lock is never held across a call into the driver (vm_ops->fault,
 * ->open, ->close, f_op->mmap) nor across fput().
 */
enum lkpi_vma_state {
	LKPI_VMA_NEW = 0,	/* created, f_op->mmap not yet run */
	LKPI_VMA_LIVE = 1,	/* mapped, faults allowed */
	LKPI_VMA_DEAD = 2	/* being unmapped: faults fail, zaps skip */
};

struct lkpi_vma {
	struct vm_area_struct	vma;
	struct lkpi_itree_node	it;	/* [vm_pgoff, vm_pgoff + npages - 1] */
	struct address_space	*mapping; /* where it is registered, or NULL */
	struct mutex		lock;
	enum lkpi_vma_state	state;
	enum lkpi_cache_mode	cache;	/* user and window attribute */
	unsigned long		npages;
	unsigned long		*present; /* window page loaded */
	uint64_t		zap_gen;  /* bumped by every zap */
	uint64_t		base_off; /* mmap offset of page 0, bytes */
	void			*seg;	/* backend: devmap handle */
	void			*win;	/* backend: kernel window */
	unsigned int		attr_warned;
};

#define	lkpi_vma_from_vma(v)	container_of((v), struct lkpi_vma, vma)

/* mmap(2) flags/prot as the glue sees them (independent of headers). */
#define	LKPI_PROT_READ		0x1U
#define	LKPI_PROT_WRITE		0x2U
#define	LKPI_PROT_EXEC		0x4U
#define	LKPI_MAP_SHARED		0x1U
#define	LKPI_MAP_PRIVATE	0x2U

/*
 * Create the VMA for mmap(filp, off, len) and run f_op->mmap. On success
 * the VMA is LIVE and registered in vm_file->f_mapping (if any); the
 * caller binds it to its devmap handle with lkpi_vma_bind() or, if the
 * devmap setup fails, calls lkpi_vma_destroy().
 */
int lkpi_vma_create(struct lkpi_file *filp, uint64_t off, size_t len,
    unsigned int prot, unsigned int maxprot, unsigned int mflags,
    unsigned long uvaddr, struct lkpi_vma **lvp);
void lkpi_vma_bind(struct lkpi_vma *lv, void *seg);

/* Fault types as the backend reports them. */
#define	LKPI_FAULT_INVAL	0	/* ordinary fault (F_INVAL/F_PROT) */
#define	LKPI_FAULT_SOFTLOCK	1	/* F_SOFTLOCK: translations must load */
#define	LKPI_FAULT_PROT		2	/* F_PROT: protection fault */

/*
 * devmap_access: make [off, off + len) present in the user's mapping.
 * Returns 0 or a positive illumos errno (EFAULT -> SIGSEGV/SIGBUS, ENOMEM,
 * EAGAIN).
 */
int lkpi_vma_fault(struct lkpi_vma *lv, uint64_t off, size_t len, int type,
    bool write);
/* devmap_dup (fork): the child's piece. */
int lkpi_vma_dup(struct lkpi_vma *lv, void *new_seg, struct lkpi_vma **nlvp);
/*
 * devmap_unmap: [off, off + len) of lv goes away. new_seg1/new_seg2 are the
 * handles of the surviving pieces before/after the hole (or NULL); their
 * VMAs are returned in *nlv1 and *nlv2. lv is destroyed.
 */
void lkpi_vma_unmap(struct lkpi_vma *lv, uint64_t off, size_t len,
    void *new_seg1, struct lkpi_vma **nlv1, void *new_seg2,
    struct lkpi_vma **nlv2);
/* Tear down a VMA that never got (or no longer has) a handle. */
void lkpi_vma_destroy(struct lkpi_vma *lv);

#endif /* _DRMGLUE_LKPI_MM_H_ */
