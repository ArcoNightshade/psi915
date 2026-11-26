/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * The platform interface under the portable glue core (file.c, vma.c,
 * anonfile.c, cdev.c, dma_buf.c, sync_file.c).
 *
 * illumos kernel: src/segdev_illumos.c, src/anonfs_illumos.c and
 * src/cdev_illumos.c implement it with devmap(9F), gfx_private kernel
 * windows, file_t/vnode_t and pollhead. Userland tests: tests/sim_backend.c
 * implements it with a simulated segdev, fd table and vnode layer, so the
 * lifecycle and locking logic of the core is executed on every host.
 *
 * All operations may sleep unless noted.
 */
#ifndef _DRMGLUE_BACKEND_H_
#define	_DRMGLUE_BACKEND_H_

#include <linux/types.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>

struct lkpi_glue_backend {
	/*
	 * Kernel window of a VMA: npages of kernel VA with no translations.
	 * win_load(pgidx, pfn, cache) maps one page (called with lv->lock
	 * held, never for a page already loaded); win_unload unmaps a run of
	 * loaded pages.
	 */
	int	(*win_create)(struct lkpi_vma *lv, void **winp);
	void	(*win_destroy)(struct lkpi_vma *lv, void *win);
	int	(*win_load)(void *win, unsigned long pgidx, unsigned long pfn,
		    enum lkpi_cache_mode cache);
	void	(*win_unload)(void *win, unsigned long pgidx,
		    unsigned long npages);

	/*
	 * User translations through the mapping handle (devmap_load and
	 * devmap_unload). off is in the handle's offset space (the mmap
	 * offset). user_load returns 0 or a positive errno and is only called
	 * from lkpi_vma_fault() (the address-space lock is held by the
	 * caller's caller); user_unload may be called from any thread.
	 */
	int	(*user_load)(void *seg, uint64_t off, size_t len, int type,
		    bool write);
	void	(*user_unload)(void *seg, uint64_t off, size_t len);

	/*
	 * Per-file poll head. ph_wake may be called from any context,
	 * including interrupt context with spinlocks held; implementations
	 * defer the real wakeup (pollwakeup(9F) must not run under locks the
	 * chpoll path takes).
	 */
	void	*(*ph_alloc)(struct lkpi_file *f);
	void	(*ph_free)(void *ph);
	void	(*ph_wake)(void *ph, __poll_t events);

	/*
	 * Anonymous-file descriptors (anonfile.c).
	 *   vn_alloc     new vnode for f, count 1, v_data = f
	 *   vn_hold      count++ (called with f->f_lkpi_lock held)
	 *   vn_rele_last called from the vnode's inactive routine with
	 *                f->f_lkpi_lock held: if the count is above one,
	 *                decrement it and return false (someone took a new
	 *                hold); else mark it dead and return true
	 *   vn_free      free a dead vnode
	 *   fd_reserve   reserve the lowest free descriptor; returns it or a
	 *                negative errno (-EMFILE)
	 *   fd_unreserve release a reserved, never-installed descriptor
	 *   fd_install   bind vn to reserved fd with open flags (O_CLOEXEC,
	 *                O_RDWR...); consumes one vnode hold
	 *   fd_get       file of an anonymous descriptor with a new
	 *                reference, or NULL (bad fd, other file type)
	 */
	void	*(*vn_alloc)(struct lkpi_file *f);
	void	(*vn_hold)(void *vn);
	bool	(*vn_rele_last)(void *vn);
	void	(*vn_free)(void *vn);
	int	(*fd_reserve)(unsigned int oflags);
	void	(*fd_unreserve)(int fd);
	void	(*fd_install)(int fd, void *vn, unsigned int oflags);
	struct lkpi_file *(*fd_get)(int fd);

	/* Optional: release backend state of a file being freed. */
	void	(*file_fini)(struct lkpi_file *f);
};

extern const struct lkpi_glue_backend *lkpi_glue_be;

void lkpi_glue_set_backend(const struct lkpi_glue_backend *be);

/* Called by the backend's vnode inactive routine (anonfile.c). */
void lkpi_anon_vnode_inactive(struct lkpi_file *f, void *vn);

#endif /* _DRMGLUE_BACKEND_H_ */
