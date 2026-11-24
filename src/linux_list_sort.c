/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * list_sort(): stable bottom-up merge sort (see <linux/list_sort.h>).
 * Clean-room, textbook algorithm.
 *
 * The list is unlinked into a NULL-terminated singly linked chain (next
 * pointers only). Elements are fed one at a time into bins: bins[i] holds
 * a sorted run of 2^i elements, like a binary counter. A run in a higher
 * bin always holds earlier elements than the carry it is merged with, and
 * merge() prefers its first argument on ties, so the sort is stable. Prev
 * pointers are rebuilt at the end.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>

#define	LKPI_LIST_SORT_BINS	(sizeof (size_t) * 8)

/* Merge two sorted chains; a's elements precede b's in the input. */
static struct list_head *
lkpi_list_merge(void *priv, list_cmp_func_t cmp, struct list_head *a,
    struct list_head *b)
{
	struct list_head *head = NULL, **tail = &head;

	while (a != NULL && b != NULL) {
		if (cmp(priv, a, b) <= 0) {
			*tail = a;
			a = a->next;
		} else {
			*tail = b;
			b = b->next;
		}
		tail = &(*tail)->next;
	}
	*tail = a != NULL ? a : b;
	return (head);
}

void
list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *bins[LKPI_LIST_SORT_BINS];
	struct list_head *list, *next, *carry, *prev;
	size_t i, max_bin = 0;

	if (list_empty(head) || list_is_singular(head))
		return;
	for (i = 0; i < LKPI_LIST_SORT_BINS; i++)
		bins[i] = NULL;

	head->prev->next = NULL;
	for (list = head->next; list != NULL; list = next) {
		next = list->next;
		list->next = NULL;
		carry = list;
		for (i = 0; bins[i] != NULL; i++) {
			carry = lkpi_list_merge(priv, cmp, bins[i], carry);
			bins[i] = NULL;
		}
		bins[i] = carry;
		if (i > max_bin)
			max_bin = i;
	}

	carry = NULL;
	for (i = 0; i <= max_bin; i++) {
		if (bins[i] == NULL)
			continue;
		carry = carry == NULL ? bins[i] :
		    lkpi_list_merge(priv, cmp, bins[i], carry);
	}

	/* Relink as a circular doubly linked list. */
	prev = head;
	for (list = carry; list != NULL; list = list->next) {
		list->prev = prev;
		prev->next = list;
		prev = list;
	}
	prev->next = head;
	head->prev = prev;
}
