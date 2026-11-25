/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Sleepable RCU (see <linux/srcu.h>). Clean-room design.
 *
 * Correctness: reader "inc ctr[i]; mb; load idx" and updater "store idx;
 * mb; load ctr[i]" form a store-buffering pair, so either the reader sees
 * the flipped index (and retries on the new one) or the updater sees the
 * reader's increment (and waits for it). A reader that passes the
 * re-check therefore either is waited for or started after the flip, by
 * which time the updater's earlier stores are visible to it.
 */

#include <linux/kernel.h>
#include <linux/srcu.h>
#include <linux/bug.h>

int
init_srcu_struct(struct srcu_struct *ssp)
{
	atomic_long_set(&ssp->ctr[0], 0);
	atomic_long_set(&ssp->ctr[1], 0);
	ssp->idx = 0;
	mutex_init(&ssp->lock);
	return (0);
}

void
cleanup_srcu_struct(struct srcu_struct *ssp)
{
	WARN_ON(atomic_long_read(&ssp->ctr[0]) != 0 ||
	    atomic_long_read(&ssp->ctr[1]) != 0);
	mutex_destroy(&ssp->lock);
}

int
srcu_read_lock(struct srcu_struct *ssp)
{
	int i;

	for (;;) {
		i = (int)(READ_ONCE(ssp->idx) & 1);
		atomic_long_inc(&ssp->ctr[i]);
		smp_mb();
		if ((int)(READ_ONCE(ssp->idx) & 1) == i)
			return (i);
		atomic_long_dec(&ssp->ctr[i]);
	}
}

void
srcu_read_unlock(struct srcu_struct *ssp, int idx)
{
	smp_mb();
	atomic_long_dec(&ssp->ctr[idx & 1]);
}

bool
srcu_read_lock_held(const struct srcu_struct *ssp)
{
	(void) ssp;
	return (true);
}

void
synchronize_srcu(struct srcu_struct *ssp)
{
	unsigned long old;
	uint64_t wait = 1000;	/* ns, doubling up to 1 ms */

	mutex_lock(&ssp->lock);
	smp_mb();
	old = READ_ONCE(ssp->idx) & 1;
	WRITE_ONCE(ssp->idx, ssp->idx + 1);
	smp_mb();
	while (atomic_long_read(&ssp->ctr[old]) != 0) {
		lkpi_plat_sleep_ns(wait);
		if (wait < 1000000)
			wait *= 2;
	}
	smp_mb();
	mutex_unlock(&ssp->lock);
}
