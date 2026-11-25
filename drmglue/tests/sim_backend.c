/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Userland simulation backend; see sim_backend.h.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>
#include "sim_backend.h"

long sim_win_loads, sim_win_unloads, sim_user_unloads, sim_ph_wakes;
long sim_live_wins, sim_live_vnodes, sim_live_phs, sim_page_puts;

static DEFINE_MUTEX(sim_lock);

#define	SIM_ADD(v, n)	__atomic_add_fetch(&(v), (n), __ATOMIC_SEQ_CST)

/*
 * Windows: pfn + 1 per page.
 */
struct sim_win {
	unsigned long	npages;
	unsigned long	*pfn;
};

static int
sim_win_create(struct lkpi_vma *lv, void **winp)
{
	struct sim_win *w = kzalloc(sizeof (*w), GFP_KERNEL);

	w->npages = lv->npages;
	w->pfn = kcalloc(lv->npages, sizeof (unsigned long), GFP_KERNEL);
	SIM_ADD(sim_live_wins, 1);
	*winp = w;
	return (0);
}

static void
sim_win_destroy(struct lkpi_vma *lv, void *win)
{
	struct sim_win *w = win;
	unsigned long i;

	for (i = 0; i < w->npages; i++)
		BUG_ON(w->pfn[i] != 0);	/* core must unload first */
	kfree(w->pfn);
	kfree(w);
	SIM_ADD(sim_live_wins, -1);
}

static int
sim_win_load(void *win, unsigned long pg, unsigned long pfn,
    enum lkpi_cache_mode cache)
{
	struct sim_win *w = win;

	BUG_ON(pg >= w->npages);
	BUG_ON(w->pfn[pg] != 0);	/* double load = hat lock-count bug */
	__atomic_store_n(&w->pfn[pg], pfn + 1, __ATOMIC_SEQ_CST);
	SIM_ADD(sim_win_loads, 1);
	return (0);
}

static void
sim_win_unload(void *win, unsigned long pg, unsigned long n)
{
	struct sim_win *w = win;
	unsigned long i;

	for (i = pg; i < pg + n; i++) {
		BUG_ON(w->pfn[i] == 0);
		__atomic_store_n(&w->pfn[i], 0, __ATOMIC_SEQ_CST);
	}
	SIM_ADD(sim_win_unloads, n);
}

/*
 * User translations: copy from the window like segdev_faultpage() does
 * with hat_getpfnum(kas, ...); an unloaded window page is EFAULT.
 */
static int
sim_user_load(void *seg, uint64_t off, size_t len, int type, bool write)
{
	struct sim_seg *s = seg;
	struct sim_win *w = s->lv->win;
	unsigned long first = (off - s->uoff) >> PAGE_SHIFT, i, n;

	n = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	BUG_ON(first + n > s->npages);
	for (i = first; i < first + n; i++) {
		unsigned long p = __atomic_load_n(&w->pfn[i],
		    __ATOMIC_SEQ_CST);

		if (p == 0)
			return (EFAULT);
		__atomic_store_n(&s->user[i], p, __ATOMIC_SEQ_CST);
	}
	return (0);
}

static void
sim_user_unload(void *seg, uint64_t off, size_t len)
{
	struct sim_seg *s = seg;
	unsigned long first = (off - s->uoff) >> PAGE_SHIFT, i;

	for (i = first; i < first + (len >> PAGE_SHIFT) && i < s->npages; i++)
		__atomic_store_n(&s->user[i], 0, __ATOMIC_SEQ_CST);
	SIM_ADD(sim_user_unloads, 1);
}

/*
 * Poll heads.
 */
struct sim_ph {
	long	wakes;
};

static void *
sim_ph_alloc(struct lkpi_file *f)
{
	SIM_ADD(sim_live_phs, 1);
	return (kzalloc(sizeof (struct sim_ph), GFP_KERNEL));
}

static void
sim_ph_free(void *ph)
{
	SIM_ADD(sim_live_phs, -1);
	kfree(ph);
}

static void
sim_ph_wake(void *ph, __poll_t ev)
{
	SIM_ADD(((struct sim_ph *)ph)->wakes, 1);
	SIM_ADD(sim_ph_wakes, 1);
}

/*
 * Vnodes, file_t, fd tables.
 */
struct sim_vnode {
	int			count;
	bool			dead;
	struct lkpi_file	*f;
};

struct sim_filet {
	int			count;
	struct sim_vnode	*vn;
	unsigned int		flags;
};

#define	SIM_NFD	64
enum { FD_FREE = 0, FD_RESERVED, FD_USED };

struct sim_proc {
	int			state[SIM_NFD];
	bool			cloexec[SIM_NFD];
	struct sim_filet	*fp[SIM_NFD];
};

static struct sim_proc sim_proc0;
static struct sim_proc *sim_cur = &sim_proc0;

static void *
sim_vn_alloc(struct lkpi_file *f)
{
	struct sim_vnode *vn = kzalloc(sizeof (*vn), GFP_KERNEL);

	vn->count = 1;
	vn->f = f;
	SIM_ADD(sim_live_vnodes, 1);
	return (vn);
}

static void
sim_vn_hold(void *v)
{
	struct sim_vnode *vn = v;

	mutex_lock(&sim_lock);
	BUG_ON(vn->dead);
	vn->count++;
	mutex_unlock(&sim_lock);
}

static bool
sim_vn_rele_last(void *v)
{
	struct sim_vnode *vn = v;
	bool last;

	mutex_lock(&sim_lock);
	if (vn->count > 1) {
		vn->count--;
		last = false;
	} else {
		vn->count = 0;
		vn->dead = true;
		last = true;
	}
	mutex_unlock(&sim_lock);
	return (last);
}

static void
sim_vn_free(void *v)
{
	kfree(v);
	SIM_ADD(sim_live_vnodes, -1);
}

/* VN_RELE: inactive when the count is one (illumos vn_rele()). */
static void
sim_vn_rele(struct sim_vnode *vn)
{
	mutex_lock(&sim_lock);
	if (vn->count > 1) {
		vn->count--;
		mutex_unlock(&sim_lock);
		return;
	}
	mutex_unlock(&sim_lock);
	lkpi_anon_vnode_inactive(vn->f, vn);
}

static int
sim_fd_reserve(unsigned int oflags)
{
	int i;

	mutex_lock(&sim_lock);
	for (i = 0; i < SIM_NFD; i++) {
		if (sim_cur->state[i] == FD_FREE) {
			sim_cur->state[i] = FD_RESERVED;
			mutex_unlock(&sim_lock);
			return (i);
		}
	}
	mutex_unlock(&sim_lock);
	return (-EMFILE);
}

static void
sim_fd_unreserve(int fd)
{
	mutex_lock(&sim_lock);
	BUG_ON(sim_cur->state[fd] != FD_RESERVED);
	sim_cur->state[fd] = FD_FREE;
	mutex_unlock(&sim_lock);
}

static void
sim_fd_install(int fd, void *vn, unsigned int oflags)
{
	struct sim_filet *fp = kzalloc(sizeof (*fp), GFP_KERNEL);

	fp->count = 1;
	fp->vn = vn;
	fp->flags = oflags;
	mutex_lock(&sim_lock);
	BUG_ON(sim_cur->state[fd] != FD_RESERVED);
	sim_cur->state[fd] = FD_USED;
	sim_cur->fp[fd] = fp;
	sim_cur->cloexec[fd] = (oflags & O_CLOEXEC) != 0;
	mutex_unlock(&sim_lock);
}

static struct lkpi_file *
sim_fd_get(int fd)
{
	struct lkpi_file *f = NULL;

	mutex_lock(&sim_lock);
	if (fd >= 0 && fd < SIM_NFD && sim_cur->state[fd] == FD_USED)
		f = get_file(sim_cur->fp[fd]->vn->f);
	mutex_unlock(&sim_lock);
	return (f);
}

static const struct lkpi_glue_backend sim_be = {
	.win_create = sim_win_create,
	.win_destroy = sim_win_destroy,
	.win_load = sim_win_load,
	.win_unload = sim_win_unload,
	.user_load = sim_user_load,
	.user_unload = sim_user_unload,
	.ph_alloc = sim_ph_alloc,
	.ph_free = sim_ph_free,
	.ph_wake = sim_ph_wake,
	.vn_alloc = sim_vn_alloc,
	.vn_hold = sim_vn_hold,
	.vn_rele_last = sim_vn_rele_last,
	.vn_free = sim_vn_free,
	.fd_reserve = sim_fd_reserve,
	.fd_unreserve = sim_fd_unreserve,
	.fd_install = sim_fd_install,
	.fd_get = sim_fd_get,
};

void
sim_init(void)
{
	lkpi_glue_set_backend(&sim_be);
}

unsigned long
lkpi_page_to_pfn(struct page *page)
{
	return ((uintptr_t)page >> 4);
}

void
lkpi_put_page(struct page *page)
{
	SIM_ADD(sim_page_puts, 1);
}

/*
 * Mapping layer driver (what segdev + devmap(9E) do).
 */
static struct sim_seg *
sim_seg_new(uint64_t uoff, unsigned long npages)
{
	struct sim_seg *s = kzalloc(sizeof (*s), GFP_KERNEL);

	s->uoff = uoff;
	s->npages = npages;
	s->user = kcalloc(npages, sizeof (unsigned long), GFP_KERNEL);
	return (s);
}

static void
sim_seg_free(struct sim_seg *s)
{
	kfree(s->user);
	kfree(s);
}

int
sim_mmap(struct lkpi_file *f, uint64_t off, size_t len, unsigned int prot,
    struct sim_seg **segp)
{
	struct lkpi_vma *lv;
	struct sim_seg *s;
	int r;

	r = lkpi_vma_create(f, off, len, prot, LKPI_PROT_READ |
	    LKPI_PROT_WRITE, LKPI_MAP_SHARED, 0x10000000UL, &lv);
	if (r != 0)
		return (r);
	s = sim_seg_new(off, lv->npages);
	s->lv = lv;
	lkpi_vma_bind(lv, s);
	*segp = s;
	return (0);
}

long
sim_user_pfn(struct sim_seg *s, unsigned long pg)
{
	unsigned long p = __atomic_load_n(&s->user[pg], __ATOMIC_SEQ_CST);

	return (p == 0 ? -1 : (long)(p - 1));
}

long
sim_access(struct sim_seg *s, unsigned long pg, bool write)
{
	long pfn;
	int r, i;

	/* A real CPU refaults until a translation is there. */
	for (i = 0; i < 64; i++) {
		pfn = sim_user_pfn(s, pg);
		if (pfn >= 0)
			return (pfn);
		r = lkpi_vma_fault(s->lv, s->uoff + ((uint64_t)pg << PAGE_SHIFT),
		    PAGE_SIZE, LKPI_FAULT_INVAL, write);
		if (r != 0)
			return (-r);
	}
	return (-EAGAIN);
}

struct sim_seg *
sim_fork(struct sim_seg *s)
{
	struct sim_seg *n = sim_seg_new(s->uoff, s->npages);

	/* segdev does not copy translations: the child refaults. */
	if (lkpi_vma_dup(s->lv, n, &n->lv) != 0) {
		sim_seg_free(n);
		return (NULL);
	}
	return (n);
}

void
sim_munmap(struct sim_seg *s, unsigned long pg, unsigned long n,
    struct sim_seg **before, struct sim_seg **after)
{
	struct sim_seg *b = NULL, *a = NULL;
	struct lkpi_vma *bl = NULL, *al = NULL;
	unsigned long i;

	/* segdev unloads the hole first, then calls devmap_unmap. */
	for (i = pg; i < pg + n; i++)
		s->user[i] = 0;
	if (pg > 0) {
		b = sim_seg_new(s->uoff, pg);
		memcpy(b->user, s->user, pg * sizeof (unsigned long));
	}
	if (pg + n < s->npages) {
		a = sim_seg_new(s->uoff + ((uint64_t)(pg + n) << PAGE_SHIFT),
		    s->npages - pg - n);
		memcpy(a->user, s->user + pg + n,
		    a->npages * sizeof (unsigned long));
	}
	lkpi_vma_unmap(s->lv, s->uoff + ((uint64_t)pg << PAGE_SHIFT),
	    n << PAGE_SHIFT, b, &bl, a, &al);
	if (b != NULL)
		b->lv = bl;
	if (a != NULL)
		a->lv = al;
	sim_seg_free(s);
	if (before != NULL)
		*before = b;
	else if (b != NULL)
		sim_munmap(b, 0, b->npages, NULL, NULL);
	if (after != NULL)
		*after = a;
	else if (a != NULL)
		sim_munmap(a, 0, a->npages, NULL, NULL);
}

/*
 * Processes.
 */
struct sim_proc *
sim_proc_new(void)
{
	return (kzalloc(sizeof (struct sim_proc), GFP_KERNEL));
}

void
sim_proc_set(struct sim_proc *p)
{
	sim_cur = p != NULL ? p : &sim_proc0;
}

static void
sim_filet_rele(struct sim_filet *fp)
{
	bool last;

	mutex_lock(&sim_lock);
	last = --fp->count == 0;
	mutex_unlock(&sim_lock);
	if (last) {
		sim_vn_rele(fp->vn);
		kfree(fp);
	}
}

static int
sim_close_in(struct sim_proc *p, int fd)
{
	struct sim_filet *fp;

	mutex_lock(&sim_lock);
	if (fd < 0 || fd >= SIM_NFD || p->state[fd] != FD_USED) {
		mutex_unlock(&sim_lock);
		return (EBADF);
	}
	fp = p->fp[fd];
	p->fp[fd] = NULL;
	p->state[fd] = FD_FREE;
	mutex_unlock(&sim_lock);
	sim_filet_rele(fp);
	return (0);
}

int
sim_close(int fd)
{
	return (sim_close_in(sim_cur, fd));
}

void
sim_proc_exit(struct sim_proc *p)
{
	int i;

	for (i = 0; i < SIM_NFD; i++)
		if (p->state[i] == FD_USED)
			(void) sim_close_in(p, i);
	if (p != &sim_proc0)
		kfree(p);
}

static int
sim_add_fd(struct sim_proc *p, struct sim_filet *fp)
{
	int i;

	for (i = 0; i < SIM_NFD; i++) {
		if (p->state[i] == FD_FREE) {
			p->state[i] = FD_USED;
			p->fp[i] = fp;
			p->cloexec[i] = false;
			fp->count++;
			return (i);
		}
	}
	return (-EMFILE);
}

int
sim_dup(int fd)
{
	int r;

	mutex_lock(&sim_lock);
	r = sim_cur->state[fd] == FD_USED ?
	    sim_add_fd(sim_cur, sim_cur->fp[fd]) : -EBADF;
	mutex_unlock(&sim_lock);
	return (r);
}

int
sim_send_fd(struct sim_proc *from, int fd, struct sim_proc *to)
{
	int r;

	mutex_lock(&sim_lock);
	r = from->state[fd] == FD_USED ? sim_add_fd(to, from->fp[fd]) : -EBADF;
	mutex_unlock(&sim_lock);
	return (r);
}

int
sim_fd_count(struct sim_proc *p)
{
	int i, n = 0;

	for (i = 0; i < SIM_NFD; i++)
		n += p->state[i] == FD_USED;
	return (n);
}

bool
sim_fd_cloexec(int fd)
{
	return (sim_cur->cloexec[fd]);
}

long long
sim_fd_size(int fd)
{
	long long sz = -1;

	mutex_lock(&sim_lock);
	if (sim_cur->state[fd] == FD_USED)
		sz = sim_cur->fp[fd]->vn->f->f_inode->i_size;
	mutex_unlock(&sim_lock);
	return (sz);
}
