/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Shrinker registry and seq_buf (see <linux/shrinker.h>,
 * <linux/seq_buf.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/shrinker.h>
#include <linux/seq_buf.h>
#include <linux/slab.h>
#include <linux/mutex.h>

static LIST_HEAD(lkpi_shrinkers);
static DEFINE_MUTEX(lkpi_shrinker_lock);

struct shrinker *
shrinker_alloc(unsigned int flags, const char *fmt, ...)
{
	struct shrinker *s = kzalloc(sizeof (*s), GFP_KERNEL);

	(void) fmt;
	if (s == NULL)
		return (NULL);
	s->flags = flags;
	s->seeks = DEFAULT_SEEKS;
	INIT_LIST_HEAD(&s->lkpi_entry);
	return (s);
}

void
shrinker_register(struct shrinker *s)
{
	mutex_lock(&lkpi_shrinker_lock);
	list_add_tail(&s->lkpi_entry, &lkpi_shrinkers);
	mutex_unlock(&lkpi_shrinker_lock);
}

void
shrinker_free(struct shrinker *s)
{
	if (s == NULL)
		return;
	mutex_lock(&lkpi_shrinker_lock);
	if (!list_empty(&s->lkpi_entry))
		list_del_init(&s->lkpi_entry);
	mutex_unlock(&lkpi_shrinker_lock);
	kfree(s);
}

unsigned long
lkpi_shrinkers_run(unsigned long nr, gfp_t gfp)
{
	struct shrink_control sc = { .gfp_mask = gfp, .nid = 0 };
	struct shrinker *s;
	unsigned long freed = 0, r, n;

	mutex_lock(&lkpi_shrinker_lock);
	list_for_each_entry(s, &lkpi_shrinkers, lkpi_entry) {
		if (freed >= nr)
			break;
		n = s->count_objects(s, &sc);
		if (n == 0 || n == SHRINK_EMPTY)
			continue;
		sc.nr_to_scan = n < nr - freed ? n : nr - freed;
		sc.nr_scanned = sc.nr_to_scan;
		r = s->scan_objects(s, &sc);
		if (r != SHRINK_STOP)
			freed += r;
	}
	mutex_unlock(&lkpi_shrinker_lock);
	return (freed);
}

int
seq_buf_printf(struct seq_buf *s, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (s->len >= s->size)
		return (-1);
	va_start(ap, fmt);
	n = lkpi_vsnprintf(s->buffer + s->len, s->size - s->len, fmt, ap);
	va_end(ap);
	s->len += (size_t)n;
	return (s->len < s->size ? 0 : -1);
}
