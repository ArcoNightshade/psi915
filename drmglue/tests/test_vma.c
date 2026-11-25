/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * VMA emulation (src/vma.c) driven through the simulated segdev: a fake
 * GEM driver with a fault handler whose backing pfns move, mmap, fault,
 * zap (unmap_mapping_range), partial munmap, fork, release deferral, the
 * error paths, and a multi-threaded fault-versus-zap race with the
 * driver lock order of docs/10, section 3.6.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>
#include <drmglue/glue.h>
#include "sim_backend.h"

#include "harness.h"

#define	OBJ_PGOFF	0x100000UL	/* fake mmap offset of the object */
#define	OBJ_OFF		((uint64_t)OBJ_PGOFF << PAGE_SHIFT)
#define	OBJ_PAGES	8

struct fobj {
	struct mutex	lock;		/* "driver lock" */
	unsigned long	pfn_base;
	atomic_t	refs;		/* vm_ops open/close + mmap */
	atomic_t	faults;
	int		mode;		/* see fobj_fault */
	int		released;
};

static struct fobj obj;

static vm_fault_t
fobj_fault(struct vm_fault *vmf)
{
	struct fobj *o = vmf->vma->vm_private_data;
	unsigned long idx = vmf->pgoff - OBJ_PGOFF;
	vm_fault_t r;

	atomic_inc(&o->faults);
	if (o->mode == 1)
		return (VM_FAULT_SIGBUS);
	if (o->mode == 2)
		return (VM_FAULT_OOM);
	if (o->mode == 3) {
		vmf->page = SIM_PAGE(o->pfn_base + idx);
		return (0);
	}
	mutex_lock(&o->lock);
	/* Prefault the whole VMA like i915's vm_fault_cpu (remap_io_sg). */
	r = lkpi_vma_insert_pfns(vmf->vma, vmf->vma->vm_start, NULL,
	    o->pfn_base + (vmf->vma->vm_pgoff - OBJ_PGOFF), vma_pages(vmf->vma),
	    vmf->vma->vm_page_prot) == 0 ? VM_FAULT_NOPAGE : VM_FAULT_SIGBUS;
	mutex_unlock(&o->lock);
	return (r);
}

static void
fobj_vm_open(struct vm_area_struct *vma)
{
	atomic_inc(&((struct fobj *)vma->vm_private_data)->refs);
}

static void
fobj_vm_close(struct vm_area_struct *vma)
{
	atomic_dec(&((struct fobj *)vma->vm_private_data)->refs);
}

static const struct vm_operations_struct fobj_vm_ops = {
	.open = fobj_vm_open,
	.close = fobj_vm_close,
	.fault = fobj_fault,
};

static int remap_at_mmap;

static int
fobj_mmap(struct lkpi_file *f, struct vm_area_struct *vma)
{
	if (vma->vm_pgoff < OBJ_PGOFF ||
	    vma->vm_pgoff + vma_pages(vma) > OBJ_PGOFF + OBJ_PAGES)
		return (-EINVAL);
	vm_flags_set(vma, VM_PFNMAP | VM_IO | VM_DONTEXPAND);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_private_data = &obj;
	atomic_inc(&obj.refs);
	if (remap_at_mmap) {
		vma->vm_ops = NULL;
		return (remap_pfn_range(vma, vma->vm_start, 7000,
		    vma->vm_end - vma->vm_start, vma->vm_page_prot));
	}
	vma->vm_ops = &fobj_vm_ops;
	return (0);
}

static int
fobj_release(struct inode *ip, struct lkpi_file *f)
{
	obj.released++;
	return (0);
}

static const struct file_operations fobj_fops = {
	.mmap = fobj_mmap,
	.release = fobj_release,
};

static struct lkpi_file *
new_file(void)
{
	struct inode *ip = lkpi_inode_alloc(0, 0100600);
	struct lkpi_file *f;

	f = lkpi_file_alloc(LKPI_FILE_CDEV, &fobj_fops, ip, O_RDWR,
	    FMODE_READ | FMODE_WRITE);
	f->f_lkpi_own_inode = ip;
	return (f);
}

static void
reset_obj(void)
{
	mutex_init(&obj.lock);
	obj.pfn_base = 1000;
	atomic_set(&obj.refs, 0);
	atomic_set(&obj.faults, 0);
	obj.mode = 0;
	obj.released = 0;
	remap_at_mmap = 0;
}

static void
zap_obj(struct lkpi_file *f, unsigned long newbase)
{
	mutex_lock(&obj.lock);
	obj.pfn_base = newbase;
	unmap_mapping_range(f->f_mapping, OBJ_OFF,
	    (loff_t)OBJ_PAGES << PAGE_SHIFT, 1);
	mutex_unlock(&obj.lock);
}

TEST(mmap_fault_zap)
{
	struct lkpi_file *f;
	struct sim_seg *s;
	long wins = sim_live_wins;

	reset_obj();
	f = new_file();
	CHECK_EQ(sim_mmap(f, OBJ_OFF, OBJ_PAGES * PAGE_SIZE,
	    LKPI_PROT_READ | LKPI_PROT_WRITE, &s), 0);
	CHECK_EQ(s->lv->cache, LKPI_CACHE_WC);
	CHECK_EQ(atomic_read(&obj.refs), 1);
	CHECK_EQ(sim_user_pfn(s, 3), -1);
	CHECK_EQ(sim_access(s, 3, false), 1003);
	CHECK_EQ(atomic_read(&obj.faults), 1);
	/* Prefaulted window: another page needs no driver call. */
	CHECK_EQ(sim_access(s, 0, true), 1000);
	CHECK_EQ(atomic_read(&obj.faults), 1);

	/* Object moves: the zap must remove user translations. */
	zap_obj(f, 2000);
	CHECK_EQ(sim_user_pfn(s, 3), -1);
	CHECK_EQ(sim_user_pfn(s, 0), -1);
	CHECK_EQ(sim_access(s, 3, false), 2003);
	CHECK_EQ(atomic_read(&obj.faults), 2);

	/* Zap outside the object is a no-op. */
	unmap_mapping_range(f->f_mapping, OBJ_OFF + (64UL << PAGE_SHIFT),
	    PAGE_SIZE, 1);
	CHECK_EQ(sim_user_pfn(s, 3), 2003);

	/* Release waits for the mapping (Linux: vma holds vm_file). */
	fput(f);
	CHECK_EQ(obj.released, 0);
	sim_munmap(s, 0, OBJ_PAGES, NULL, NULL);
	CHECK_EQ(obj.released, 1);
	CHECK_EQ(atomic_read(&obj.refs), 0);
	CHECK_EQ(sim_live_wins, wins);
}

TEST(partial_munmap)
{
	struct lkpi_file *f;
	struct sim_seg *s, *b, *a;

	reset_obj();
	f = new_file();
	CHECK_EQ(sim_mmap(f, OBJ_OFF, OBJ_PAGES * PAGE_SIZE,
	    LKPI_PROT_READ | LKPI_PROT_WRITE, &s), 0);
	CHECK_EQ(sim_access(s, 1, false), 1001);
	CHECK_EQ(sim_access(s, 6, false), 1006);

	/* Unmap pages 3..4: pieces [0,3) and [5,8) survive. */
	sim_munmap(s, 3, 2, &b, &a);
	CHECK(b != NULL && a != NULL);
	CHECK_EQ(atomic_read(&obj.refs), 2);	/* open x2, close x1 */
	CHECK_EQ(b->lv->vma.vm_pgoff, OBJ_PGOFF);
	CHECK_EQ(a->lv->vma.vm_pgoff, OBJ_PGOFF + 5);
	CHECK_EQ(a->lv->npages, 3);
	/* Translations made before the split survive in the pieces... */
	CHECK_EQ(sim_user_pfn(b, 1), 1001);
	CHECK_EQ(sim_user_pfn(a, 1), 1006);
	/* ...and a later zap must still find and remove them. */
	zap_obj(f, 3000);
	CHECK_EQ(sim_user_pfn(b, 1), -1);
	CHECK_EQ(sim_user_pfn(a, 1), -1);
	CHECK_EQ(sim_access(a, 2, false), 3007);
	CHECK_EQ(sim_access(b, 0, false), 3000);

	sim_munmap(b, 0, b->npages, NULL, NULL);
	sim_munmap(a, 0, 1, NULL, &a);		/* trim the front */
	CHECK_EQ(a->lv->vma.vm_pgoff, OBJ_PGOFF + 6);
	CHECK_EQ(sim_access(a, 0, false), 3006);
	sim_munmap(a, 0, a->npages, NULL, NULL);
	CHECK_EQ(atomic_read(&obj.refs), 0);
	fput(f);
	CHECK_EQ(obj.released, 1);
}

TEST(fork_dup)
{
	struct lkpi_file *f;
	struct sim_seg *s, *c;

	reset_obj();
	f = new_file();
	CHECK_EQ(sim_mmap(f, OBJ_OFF, 4 * PAGE_SIZE, LKPI_PROT_READ, &s), 0);
	CHECK_EQ(sim_access(s, 2, false), 1002);
	c = sim_fork(s);
	CHECK(c != NULL);
	CHECK_EQ(atomic_read(&obj.refs), 2);
	CHECK_EQ(sim_user_pfn(c, 2), -1);	/* child refaults */
	CHECK_EQ(sim_access(c, 2, false), 1002);
	zap_obj(f, 4000);
	CHECK_EQ(sim_user_pfn(s, 2), -1);
	CHECK_EQ(sim_user_pfn(c, 2), -1);
	fput(f);
	sim_munmap(s, 0, 4, NULL, NULL);	/* parent exits first */
	CHECK_EQ(obj.released, 0);
	CHECK_EQ(sim_access(c, 1, false), 4001);
	sim_munmap(c, 0, 4, NULL, NULL);
	CHECK_EQ(obj.released, 1);
	CHECK_EQ(atomic_read(&obj.refs), 0);
}

TEST(errors)
{
	struct lkpi_file *f;
	struct sim_seg *s;
	long puts;

	reset_obj();
	f = new_file();
	/* Outside the object: the driver's mmap refuses. */
	CHECK_EQ(sim_mmap(f, OBJ_OFF + PAGE_SIZE, OBJ_PAGES * PAGE_SIZE,
	    LKPI_PROT_READ, &s), EINVAL);
	CHECK_EQ(sim_mmap(f, OBJ_OFF + 1, PAGE_SIZE, LKPI_PROT_READ, &s),
	    EINVAL);

	CHECK_EQ(sim_mmap(f, OBJ_OFF, 2 * PAGE_SIZE, LKPI_PROT_READ, &s), 0);
	obj.mode = 1;
	CHECK_EQ(sim_access(s, 0, false), -EFAULT);
	obj.mode = 2;
	CHECK_EQ(sim_access(s, 0, false), -ENOMEM);
	/* vmf->page path */
	obj.mode = 3;
	puts = sim_page_puts;
	CHECK_EQ(sim_access(s, 1, false), 1001);
	CHECK_EQ(sim_page_puts, puts + 1);
	/* Out-of-range fault offset */
	CHECK_EQ(lkpi_vma_fault(s->lv, OBJ_OFF + 2 * PAGE_SIZE, PAGE_SIZE,
	    LKPI_FAULT_INVAL, false), EFAULT);
	sim_munmap(s, 0, 2, NULL, NULL);

	/* No fault handler: pages inserted at mmap time only. */
	remap_at_mmap = 1;
	CHECK_EQ(sim_mmap(f, OBJ_OFF, 3 * PAGE_SIZE, LKPI_PROT_READ, &s), 0);
	CHECK_EQ(sim_access(s, 2, false), 7002);
	zap_obj(f, 0);
	CHECK_EQ(sim_access(s, 2, false), -EFAULT);	/* SIGBUS */
	sim_munmap(s, 0, 3, NULL, NULL);
	fput(f);
	CHECK_EQ(obj.released, 1);
}

/*
 * Race: faulting threads against a zapper that moves the object. The
 * zapper holds the driver lock across move + unmap_mapping_range, so when
 * it returns no translation may remain (none can be re-created until the
 * lock is dropped, because the fault handler needs it). A stale window
 * page copied to the user after the zap would show up here.
 */
#define	RACE_SEGS	4
#define	RACE_ROUNDS	3000

static struct sim_seg *race_seg[RACE_SEGS];
static struct lkpi_file *race_file;
static volatile int race_stop;
static atomic_t race_bad;

static void
race_faulter(void *arg)
{
	unsigned int r = (unsigned int)(uintptr_t)&r;
	long pfn;
	int i;

	while (!race_stop) {
		r = r * 1103515245U + 12345U;
		i = (int)((r >> 8) % RACE_SEGS);
		pfn = sim_access(race_seg[i], (r >> 16) % OBJ_PAGES,
		    (r & 1) != 0);
		if (pfn < 0 && pfn != -EAGAIN)
			atomic_inc(&race_bad);
	}
}

static void
race_zapper(void *arg)
{
	int n, i, p;

	for (n = 1; n <= RACE_ROUNDS; n++) {
		mutex_lock(&obj.lock);
		obj.pfn_base = 10000UL * (unsigned long)n;
		unmap_mapping_range(race_file->f_mapping, OBJ_OFF,
		    (loff_t)OBJ_PAGES << PAGE_SHIFT, 1);
		for (i = 0; i < RACE_SEGS; i++)
			for (p = 0; p < OBJ_PAGES; p++)
				if (sim_user_pfn(race_seg[i], p) != -1)
					atomic_inc(&race_bad);
		mutex_unlock(&obj.lock);
	}
	race_stop = 1;
}

static void
race_thread(void *arg)
{
	if (arg == (void *)1)
		race_zapper(NULL);
	else
		race_faulter(NULL);
}

static atomic_t race_tid;

static void
race_dispatch(void *arg)
{
	race_thread(atomic_inc_return(&race_tid) == 1 ? (void *)1 : NULL);
}

TEST(fault_zap_race)
{
	int i;

	reset_obj();
	race_file = new_file();
	for (i = 0; i < RACE_SEGS; i++)
		CHECK_EQ(sim_mmap(race_file, OBJ_OFF, OBJ_PAGES * PAGE_SIZE,
		    LKPI_PROT_READ | LKPI_PROT_WRITE, &race_seg[i]), 0);
	race_stop = 0;
	atomic_set(&race_bad, 0);
	atomic_set(&race_tid, 0);
	harness_run_threads(5, race_dispatch, NULL);
	CHECK_EQ(atomic_read(&race_bad), 0);
	CHECK(atomic_read(&obj.faults) > 0);
	for (i = 0; i < RACE_SEGS; i++)
		sim_munmap(race_seg[i], 0, OBJ_PAGES, NULL, NULL);
	fput(race_file);
	CHECK_EQ(obj.released, 1);
	CHECK_EQ(atomic_read(&obj.refs), 0);
}

LKPI_SUITE(drmglue_vma)
{
	sim_init();
	RUN(mmap_fault_zap);
	RUN(partial_munmap);
	RUN(fork_dup);
	RUN(errors);
	RUN(fault_zap_race);
	CHECK_EQ(sim_live_wins, 0);
	CHECK_EQ(sim_live_phs, 0);
}
