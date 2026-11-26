/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Folio batches (pages being released together).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PAGEVEC_H_
#define	_LINUXKPI_INCLUDE_LINUX_PAGEVEC_H_

#include <linux/mm.h>

#define	PAGEVEC_SIZE	31

struct folio_batch {
	unsigned char	nr;
	unsigned char	i;
	bool		percpu_pvec_drained;
	struct folio	*folios[PAGEVEC_SIZE];
};

static inline void
folio_batch_init(struct folio_batch *fb)
{
	fb->nr = 0;
	fb->i = 0;
	fb->percpu_pvec_drained = false;
}

static inline unsigned int
folio_batch_count(struct folio_batch *fb)
{
	return (fb->nr);
}

static inline unsigned int
folio_batch_space(struct folio_batch *fb)
{
	return (PAGEVEC_SIZE - fb->nr);
}

static inline unsigned int
folio_batch_add(struct folio_batch *fb, struct folio *f)
{
	fb->folios[fb->nr++] = f;
	return (folio_batch_space(fb));
}

static inline void
folio_batch_release(struct folio_batch *fb)
{
	unsigned int i;

	for (i = 0; i < fb->nr; i++)
		folio_put(fb->folios[i]);
	fb->nr = 0;
}

#define	check_move_unevictable_folios(fb)	do { (void)(fb); } while (0)

static inline void
__folio_batch_release(struct folio_batch *fb)
{
	unsigned int i;

	for (i = 0; i < fb->nr; i++)
		folio_put(fb->folios[i]);
	folio_batch_init(fb);
}
#define	folio_batch_release(fb)	__folio_batch_release(fb)
#define	check_move_unevictable_folios(fb)	do { (void)(fb); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_PAGEVEC_H_ */
