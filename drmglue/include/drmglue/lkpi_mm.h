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
#include <linuxkpi/itree.h>
/* pgprot_t, cache modes and PAGE_* (KPI, from drmglue R3); struct page. */
#include <linux/pgtable.h>
#include <linux/mm_types.h>
#include <linux/mm.h>

struct mm_struct;


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
