/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Notifier chains (src/linux_notifier.c): atomic chains (spinlock,
 * callbacks must not sleep), blocking chains (rw_semaphore), raw chains
 * (caller locks). Callbacks run in priority order (highest first).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_NOTIFIER_H_
#define	_LINUXKPI_INCLUDE_LINUX_NOTIFIER_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/srcu.h>

struct notifier_block;
typedef int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action,
    void *data);

struct notifier_block {
	notifier_fn_t		notifier_call;
	struct notifier_block	*next;
	int			priority;
};

struct atomic_notifier_head {
	spinlock_t		lock;
	struct notifier_block	*head;
};

struct blocking_notifier_head {
	struct rw_semaphore	rwsem;
	struct notifier_block	*head;
};

struct raw_notifier_head {
	struct notifier_block	*head;
};

#define	NOTIFY_DONE		0x0000
#define	NOTIFY_OK		0x0001
#define	NOTIFY_STOP_MASK	0x8000
#define	NOTIFY_BAD		(NOTIFY_STOP_MASK | 0x0002)
#define	NOTIFY_STOP		(NOTIFY_OK | NOTIFY_STOP_MASK)

#define	ATOMIC_NOTIFIER_HEAD(name)					\
	struct atomic_notifier_head name = {				\
		.lock = __SPIN_LOCK_UNLOCKED(name.lock), .head = NULL }
#define	BLOCKING_NOTIFIER_HEAD(name)					\
	struct blocking_notifier_head name = {				\
		.rwsem = __RWSEM_INITIALIZER(name.rwsem), .head = NULL }
#define	RAW_NOTIFIER_HEAD(name)						\
	struct raw_notifier_head name = { .head = NULL }
#define	ATOMIC_INIT_NOTIFIER_HEAD(h) do {				\
	spin_lock_init(&(h)->lock);					\
	(h)->head = NULL;						\
} while (0)
#define	BLOCKING_INIT_NOTIFIER_HEAD(h) do {				\
	init_rwsem(&(h)->rwsem);					\
	(h)->head = NULL;						\
} while (0)
#define	RAW_INIT_NOTIFIER_HEAD(h)	((h)->head = NULL)

static inline int
notifier_from_errno(int err)
{
	return (err != 0 ? (NOTIFY_STOP_MASK | (NOTIFY_OK - err)) : NOTIFY_OK);
}

static inline int
notifier_to_errno(int ret)
{
	ret &= ~NOTIFY_STOP_MASK;
	return (ret > NOTIFY_OK ? NOTIFY_OK - ret : 0);
}

int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
    struct notifier_block *nb);
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
    struct notifier_block *nb);
int atomic_notifier_call_chain(struct atomic_notifier_head *nh,
    unsigned long val, void *v);
int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
    struct notifier_block *nb);
int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
    struct notifier_block *nb);
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
    unsigned long val, void *v);
int raw_notifier_chain_register(struct raw_notifier_head *nh,
    struct notifier_block *nb);
int raw_notifier_chain_unregister(struct raw_notifier_head *nh,
    struct notifier_block *nb);
int raw_notifier_call_chain(struct raw_notifier_head *nh, unsigned long val,
    void *v);

#endif /* _LINUXKPI_INCLUDE_LINUX_NOTIFIER_H_ */
