/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux hrtimer API from its documented
 * behaviour; function set cross-checked against FreeBSD's LinuxKPI
 * hrtimer.h (BSD-2-Clause).
 *
 * hrtimers share the timer thread with timer_list (<linux/timer.h>,
 * <linuxkpi/timer_engine.h>): callbacks run in thread context, one at a
 * time. Clocks: CLOCK_MONOTONIC and CLOCK_BOOTTIME use the monotonic
 * clock (ktime_get()); CLOCK_REALTIME/CLOCK_TAI absolute times are
 * converted to a monotonic deadline when the timer is started (a later
 * wall-clock step does not move the timer). _SOFT/_HARD/_PINNED mode bits
 * are accepted and ignored. Slack (hrtimer_start_range_ns) is ignored.
 */
#ifndef _LINUXKPI_LINUX_HRTIMER_H_
#define	_LINUXKPI_LINUX_HRTIMER_H_

#include <linuxkpi/platform.h>
#include <linuxkpi/timer_engine.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/seqlock.h>
#include <linux/timer.h>

#ifndef CLOCK_REALTIME
#define	CLOCK_REALTIME		3
#endif
#ifndef CLOCK_MONOTONIC
#define	CLOCK_MONOTONIC		4
#endif
#ifndef CLOCK_BOOTTIME
#define	CLOCK_BOOTTIME		7
#endif
#ifndef CLOCK_TAI
#define	CLOCK_TAI		11
#endif

enum hrtimer_mode {
	HRTIMER_MODE_ABS = 0x00,
	HRTIMER_MODE_REL = 0x01,
	HRTIMER_MODE_PINNED = 0x02,
	HRTIMER_MODE_SOFT = 0x04,
	HRTIMER_MODE_HARD = 0x08,
	HRTIMER_MODE_ABS_PINNED = 0x02,
	HRTIMER_MODE_REL_PINNED = 0x03,
	HRTIMER_MODE_ABS_SOFT = 0x04,
	HRTIMER_MODE_REL_SOFT = 0x05,
	HRTIMER_MODE_ABS_HARD = 0x08,
	HRTIMER_MODE_REL_HARD = 0x09,
	HRTIMER_MODE_REL_PINNED_HARD = 0x0b
};

enum hrtimer_restart {
	HRTIMER_NORESTART,
	HRTIMER_RESTART
};

struct hrtimer {
	struct lkpi_timer_node	lkpi_node;
	ktime_t			lkpi_expires;	/* in the timer's clock */
	int			lkpi_clock;
	enum hrtimer_restart	(*function)(struct hrtimer *);
};

bool lkpi_hrtimer_run(struct lkpi_timer_node *n, u64 *deadline);

static inline void
hrtimer_setup(struct hrtimer *t, enum hrtimer_restart (*fn)(struct hrtimer *),
    int clock, enum hrtimer_mode mode)
{
	(void) mode;
	memset(t, 0, sizeof (*t));
	t->lkpi_node.run = lkpi_hrtimer_run;
	t->lkpi_clock = clock;
	t->function = fn;
}

static inline void
hrtimer_init(struct hrtimer *t, int clock, enum hrtimer_mode mode)
{
	hrtimer_setup(t, NULL, clock, mode);
}

#define	hrtimer_init_on_stack(t, c, m)		hrtimer_init(t, c, m)
#define	hrtimer_setup_on_stack(t, f, c, m)	hrtimer_setup(t, f, c, m)
#define	destroy_hrtimer_on_stack(t)		do { (void)(t); } while (0)

void hrtimer_start_range_ns(struct hrtimer *t, ktime_t tim, u64 slack,
    enum hrtimer_mode mode);
int hrtimer_cancel(struct hrtimer *t);
int hrtimer_try_to_cancel(struct hrtimer *t);
u64 hrtimer_forward(struct hrtimer *t, ktime_t now, ktime_t interval);

static inline void
hrtimer_start(struct hrtimer *t, ktime_t tim, enum hrtimer_mode mode)
{
	hrtimer_start_range_ns(t, tim, 0, mode);
}

static inline ktime_t
lkpi_hrtimer_clock_now(const struct hrtimer *t)
{
	if (t->lkpi_clock == CLOCK_REALTIME || t->lkpi_clock == CLOCK_TAI)
		return (ktime_get_real());
	return (ktime_get());
}

static inline u64
hrtimer_forward_now(struct hrtimer *t, ktime_t interval)
{
	return (hrtimer_forward(t, lkpi_hrtimer_clock_now(t), interval));
}

static inline void
hrtimer_restart(struct hrtimer *t)
{
	hrtimer_start(t, t->lkpi_expires, HRTIMER_MODE_ABS);
}

static inline void hrtimer_set_expires(struct hrtimer *t, ktime_t k) { t->lkpi_expires = k; }
static inline void hrtimer_set_expires_range_ns(struct hrtimer *t, ktime_t k, u64 d) { (void) d; t->lkpi_expires = k; }
static inline void hrtimer_add_expires_ns(struct hrtimer *t, u64 ns) { t->lkpi_expires += (ktime_t)ns; }
static inline void hrtimer_add_expires(struct hrtimer *t, ktime_t k) { t->lkpi_expires += k; }
static inline ktime_t hrtimer_get_expires(const struct hrtimer *t) { return (t->lkpi_expires); }
static inline ktime_t hrtimer_get_softexpires(const struct hrtimer *t) { return (t->lkpi_expires); }
static inline s64 hrtimer_get_expires_ns(const struct hrtimer *t) { return (t->lkpi_expires); }
static inline ktime_t hrtimer_expires_remaining(const struct hrtimer *t) { return (t->lkpi_expires - lkpi_hrtimer_clock_now(t)); }
#define	hrtimer_get_remaining(t)	hrtimer_expires_remaining(t)
static inline int hrtimer_is_queued(const struct hrtimer *t) { return (READ_ONCE(t->lkpi_node.queued)); }
static inline bool hrtimer_callback_running(const struct hrtimer *t) { return (lkpi_timer_running(&t->lkpi_node)); }

static inline bool
hrtimer_active(const struct hrtimer *t)
{
	return (hrtimer_is_queued(t) || hrtimer_callback_running(t));
}

#endif /* _LINUXKPI_LINUX_HRTIMER_H_ */
