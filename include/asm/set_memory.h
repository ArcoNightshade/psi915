/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Cache attributes of kernel mappings (src/linux_page.c):
 * gfx_private gfxp_fix_mem_cache_attrs() on illumos, a no-op in userland.
 * 0 or a negative errno.
 */
#ifndef _LINUXKPI_ASM_SET_MEMORY_H_
#define	_LINUXKPI_ASM_SET_MEMORY_H_

#include <linux/mm.h>

int set_memory_wc(unsigned long addr, int numpages);
int set_memory_uc(unsigned long addr, int numpages);
int set_memory_wb(unsigned long addr, int numpages);
int set_pages_array_wc(struct page **pages, int n);
int set_pages_array_uc(struct page **pages, int n);
int set_pages_array_wb(struct page **pages, int n);
int set_pages_wb(struct page *page, int n);
int set_pages_uc(struct page *page, int n);
#define	set_pages_array_wt(p, n)	set_pages_array_uc(p, n)

#endif /* _LINUXKPI_ASM_SET_MEMORY_H_ */
