/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room shmem page cache, from the documented behaviour of the Linux
 * API (shmem_read_mapping_page_gfp, shmem_truncate_range, find_lock_page,
 * address_space_operations write_begin/write_end):
 *
 *   - mapping->i_pages maps a page index to a struct page that holds one
 *     reference for the cache;
 *   - a page is allocated zero-filled on first read (pages beyond i_size
 *     are refused with -EINVAL), and each lookup returns an additional
 *     reference the caller drops with put_page();
 *   - truncation drops the cache reference of whole pages in the range;
 *   - there is no swap: pages stay resident until truncated or until the
 *     inode is released (lkpi_release hook, called by lkpi_inode_put).
 *
 * Concurrent first reads of one index are resolved by xa_insert(): the
 * loser frees its page and uses the winner's.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/xarray.h>
#include <linux/fs.h>
#include <linux/shmem_fs.h>
#include <linux/bug.h>

static unsigned long
lkpi_shmem_size_pages(struct address_space *m)
{
	loff_t size = m->host != NULL ? m->host->i_size : 0;

	return ((unsigned long)((size + PAGE_SIZE - 1) >> PAGE_SHIFT));
}

static struct page *
lkpi_shmem_get(struct address_space *m, unsigned long index, gfp_t gfp)
{
	struct page *p, *np;
	int r;

	if (index >= lkpi_shmem_size_pages(m))
		return (ERR_PTR(-EINVAL));
	for (;;) {
		xa_lock(&m->i_pages);
		p = xa_load(&m->i_pages, index);
		if (p != NULL)
			get_page(p);
		xa_unlock(&m->i_pages);
		if (p != NULL)
			return (p);
		np = alloc_page((gfp & ~__GFP_HIGHMEM) | __GFP_ZERO);
		if (np == NULL)
			return (ERR_PTR(-ENOMEM));
		r = xa_insert(&m->i_pages, index, np, gfp);
		if (r == 0) {
			get_page(np);
			return (np);
		}
		__free_page(np);
		if (r != -EBUSY)
			return (ERR_PTR(r));
	}
}

struct page *
shmem_read_mapping_page_gfp(struct address_space *m, pgoff_t index, gfp_t gfp)
{
	return (lkpi_shmem_get(m, index, gfp));
}

struct folio *
shmem_read_folio_gfp(struct address_space *m, pgoff_t index, gfp_t gfp)
{
	struct page *p = lkpi_shmem_get(m, index, gfp);

	if (IS_ERR(p))
		return (ERR_CAST(p));
	return (page_folio(p));
}

/* Present pages only (no allocation); a reference for the caller. */
struct page *
find_lock_page(struct address_space *m, unsigned long index)
{
	struct page *p;

	xa_lock(&m->i_pages);
	p = xa_load(&m->i_pages, index);
	if (p != NULL)
		get_page(p);
	xa_unlock(&m->i_pages);
	return (p);
}

/* Drops the cache pages wholly inside [start, end] (end inclusive). */
void
shmem_truncate_range(struct inode *ip, loff_t start, loff_t end)
{
	struct address_space *m = ip->i_mapping;
	unsigned long first, last, index;
	struct page *p;

	if (start < 0 || (end >= 0 && end < start))
		return;
	first = (unsigned long)((start + PAGE_SIZE - 1) >> PAGE_SHIFT);
	last = end < 0 || (u64)end >= (u64)LLONG_MAX - PAGE_SIZE ? ULONG_MAX :
	    (unsigned long)((end + 1) >> PAGE_SHIFT);
	if (last == 0)
		return;
	if (last != ULONG_MAX)
		last--;
	xa_for_each_range(&m->i_pages, index, p, first, last) {
		p = xa_erase(&m->i_pages, index);
		if (p != NULL)
			put_page(p);
	}
}

unsigned long
lkpi_shmem_nr_pages(struct address_space *m)
{
	unsigned long index, n = 0;
	struct page *p;

	xa_for_each(&m->i_pages, index, p)
		n++;
	return (n);
}

static int
lkpi_shmem_write_begin(struct lkpi_file *file, struct address_space *m,
    long long pos, unsigned int len, struct folio **foliop, void **fsdata)
{
	struct page *p;

	(void) file;
	(void) len;
	*fsdata = NULL;
	p = lkpi_shmem_get(m, (unsigned long)(pos >> PAGE_SHIFT), GFP_KERNEL);
	if (IS_ERR(p))
		return ((int)PTR_ERR(p));
	*foliop = page_folio(p);
	return (0);
}

static int
lkpi_shmem_write_end(struct lkpi_file *file, struct address_space *m,
    long long pos, unsigned int len, unsigned int copied,
    struct folio *folio, void *fsdata)
{
	(void) file;
	(void) m;
	(void) pos;
	(void) len;
	(void) fsdata;
	folio_put(folio);
	return ((int)copied);
}

static const struct address_space_operations lkpi_shmem_aops = {
	.write_begin = lkpi_shmem_write_begin,
	.write_end = lkpi_shmem_write_end,
};

static void
lkpi_shmem_release(struct address_space *m)
{
	unsigned long index;
	struct page *p;

	xa_for_each(&m->i_pages, index, p) {
		(void) xa_erase(&m->i_pages, index);
		put_page(p);
	}
	xa_destroy(&m->i_pages);
}

void
lkpi_shmem_mapping_init(struct address_space *m)
{
	xa_init(&m->i_pages);
	m->a_ops = &lkpi_shmem_aops;
	m->lkpi_release = lkpi_shmem_release;
}
