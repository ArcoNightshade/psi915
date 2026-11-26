/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Notifier chains (see <linux/notifier.h>). Clean-room. Chains are
 * singly linked lists sorted by descending priority; registration of an
 * already registered block is refused with -EEXIST.
 */

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/bug.h>

static int
lkpi_nb_register(struct notifier_block **head, struct notifier_block *nb)
{
	struct notifier_block **pp;

	for (pp = head; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == nb)
			return (-EEXIST);
	}
	for (pp = head; *pp != NULL && (*pp)->priority >= nb->priority;
	    pp = &(*pp)->next)
		continue;
	nb->next = *pp;
	WRITE_ONCE(*pp, nb);
	return (0);
}

static int
lkpi_nb_unregister(struct notifier_block **head, struct notifier_block *nb)
{
	struct notifier_block **pp;

	for (pp = head; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == nb) {
			WRITE_ONCE(*pp, nb->next);
			return (0);
		}
	}
	return (-ENOENT);
}

static int
lkpi_nb_call(struct notifier_block *nb, unsigned long val, void *v)
{
	int r = NOTIFY_DONE;

	while (nb != NULL) {
		struct notifier_block *next = READ_ONCE(nb->next);

		r = nb->notifier_call(nb, val, v);
		if (r & NOTIFY_STOP_MASK)
			break;
		nb = next;
	}
	return (r);
}

int
atomic_notifier_chain_register(struct atomic_notifier_head *nh,
    struct notifier_block *nb)
{
	int r;

	spin_lock(&nh->lock);
	r = lkpi_nb_register(&nh->head, nb);
	spin_unlock(&nh->lock);
	return (r);
}

int
atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
    struct notifier_block *nb)
{
	int r;

	spin_lock(&nh->lock);
	r = lkpi_nb_unregister(&nh->head, nb);
	spin_unlock(&nh->lock);
	synchronize_rcu();
	return (r);
}

/* Callers run under rcu_read_lock(), as in Linux; unregister waits. */
int
atomic_notifier_call_chain(struct atomic_notifier_head *nh,
    unsigned long val, void *v)
{
	int r;

	rcu_read_lock();
	r = lkpi_nb_call(READ_ONCE(nh->head), val, v);
	rcu_read_unlock();
	return (r);
}

int
blocking_notifier_chain_register(struct blocking_notifier_head *nh,
    struct notifier_block *nb)
{
	int r;

	down_write(&nh->rwsem);
	r = lkpi_nb_register(&nh->head, nb);
	up_write(&nh->rwsem);
	return (r);
}

int
blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
    struct notifier_block *nb)
{
	int r;

	down_write(&nh->rwsem);
	r = lkpi_nb_unregister(&nh->head, nb);
	up_write(&nh->rwsem);
	return (r);
}

int
blocking_notifier_call_chain(struct blocking_notifier_head *nh,
    unsigned long val, void *v)
{
	int r = NOTIFY_DONE;

	if (READ_ONCE(nh->head) == NULL)
		return (r);
	down_read(&nh->rwsem);
	r = lkpi_nb_call(nh->head, val, v);
	up_read(&nh->rwsem);
	return (r);
}

int
raw_notifier_chain_register(struct raw_notifier_head *nh,
    struct notifier_block *nb)
{
	return (lkpi_nb_register(&nh->head, nb));
}

int
raw_notifier_chain_unregister(struct raw_notifier_head *nh,
    struct notifier_block *nb)
{
	return (lkpi_nb_unregister(&nh->head, nb));
}

int
raw_notifier_call_chain(struct raw_notifier_head *nh, unsigned long val,
    void *v)
{
	return (lkpi_nb_call(nh->head, val, v));
}
