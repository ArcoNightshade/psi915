/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Userland simulation of the illumos pieces under the glue core: a
 * segdev-like mapping layer (handles, user translations, kernel windows),
 * a per-process fd table with file_t/vnode reference counting, and poll
 * heads that count wake-ups. It models the semantics docs/10 relies on,
 * not illumos internals; see docs/10, section 6 for what this does and does
 * not prove.
 */
#ifndef _DRMGLUE_SIM_BACKEND_H_
#define	_DRMGLUE_SIM_BACKEND_H_

#include <linux/types.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>
#include <drmglue/backend.h>

/* Install the simulation backend (idempotent). */
void sim_init(void);

/* Counters for assertions. */
extern long sim_win_loads, sim_win_unloads, sim_user_unloads, sim_ph_wakes;
extern long sim_live_wins, sim_live_vnodes, sim_live_phs;

/* Fake struct page: pfn encoded in the pointer. */
struct page *sim_page(unsigned long pfn);
#define	SIM_PAGE(pfn)	sim_page(pfn)
extern long sim_page_puts;

/*
 * Mapping layer. A sim_seg is a devmap handle: user translations for
 * npages starting at mmap offset uoff, bound to one lkpi_vma.
 */
struct sim_seg {
	uint64_t		uoff;
	unsigned long		npages;
	unsigned long		*user;	/* pfn + 1, 0 = no translation */
	struct lkpi_vma		*lv;
};

/* mmap(2): returns 0 or an errno. */
int sim_mmap(struct lkpi_file *f, uint64_t off, size_t len,
    unsigned int prot, struct sim_seg **segp);
/* A user access to page pg of seg: returns the pfn or -errno. */
long sim_access(struct sim_seg *seg, unsigned long pg, bool write);
/* Current user translation of page pg (pfn) or -1. */
long sim_user_pfn(struct sim_seg *seg, unsigned long pg);
/* fork(): duplicate seg. */
struct sim_seg *sim_fork(struct sim_seg *seg);
/* munmap(pages [pg, pg + n)): returns the surviving pieces. */
void sim_munmap(struct sim_seg *seg, unsigned long pg, unsigned long n,
    struct sim_seg **before, struct sim_seg **after);

/*
 * Processes and descriptors.
 */
struct sim_proc;
struct sim_proc *sim_proc_new(void);
void sim_proc_exit(struct sim_proc *p);	/* closes every fd */
void sim_proc_set(struct sim_proc *p);	/* "curproc" for fd ops */
int sim_close(int fd);
int sim_dup(int fd);
/* SCM_RIGHTS: install a copy of fd of from into to; returns new fd. */
int sim_send_fd(struct sim_proc *from, int fd, struct sim_proc *to);
int sim_fd_count(struct sim_proc *p);
bool sim_fd_cloexec(int fd);
/* Simulated getattr size (va_size = i_size) of an fd's file. */
long long sim_fd_size(int fd);

#endif /* _DRMGLUE_SIM_BACKEND_H_ */
