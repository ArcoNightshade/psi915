/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kref API; function set cross-checked against FreeBSD's LinuxKPI kref.h
 * (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_KREF_H_
#define	_LINUXKPI_LINUX_KREF_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

struct kref {
	refcount_t refcount;
};

#define	KREF_INIT(n)	{ .refcount = REFCOUNT_INIT(n) }

static inline void
kref_init(struct kref *kref)
{
	refcount_set(&kref->refcount, 1);
}

static inline unsigned int
kref_read(const struct kref *kref)
{
	return (refcount_read(&kref->refcount));
}

static inline void
kref_get(struct kref *kref)
{
	refcount_inc(&kref->refcount);
}

/*
 * Drop a reference; if it was the last one, call release (which normally
 * frees the containing object) and return 1. Otherwise return 0.
 */
static inline int
kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
	if (refcount_dec_and_test(&kref->refcount)) {
		release(kref);
		return (1);
	}
	return (0);
}

/*
 * As kref_put(), but if this is the last reference, take lock first and
 * call release with it held; release must drop the lock. Lets lookups
 * under lock race safely with the final put.
 */
static inline int
kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref),
    struct mutex *lock)
{
	if (refcount_dec_and_mutex_lock(&kref->refcount, lock)) {
		release(kref);
		return (1);
	}
	return (0);
}

static inline int
kref_put_lock(struct kref *kref, void (*release)(struct kref *kref),
    spinlock_t *lock)
{
	if (refcount_dec_and_lock(&kref->refcount, lock)) {
		release(kref);
		return (1);
	}
	return (0);
}

/*
 * Take a reference only if the object is still live. For lookups under a
 * lock that does not keep the object alive by itself.
 */
static inline int __must_check
kref_get_unless_zero(struct kref *kref)
{
	return (refcount_inc_not_zero(&kref->refcount));
}

#endif /* _LINUXKPI_LINUX_KREF_H_ */
