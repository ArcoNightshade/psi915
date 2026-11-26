/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No PWM framework on illumos (i915 uses it only for DSI/LPSS backlights).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PWM_H_
#define	_LINUXKPI_INCLUDE_LINUX_PWM_H_

#include <linux/types.h>
#include <linux/err.h>

struct pwm_device;
struct pwm_state {
	u64	period;
	u64	duty_cycle;
	int	polarity;
	bool	enabled;
};
#define	pwm_get(d, n)			((struct pwm_device *)ERR_PTR(-ENODEV))
#define	pwm_put(p)			do { (void)(p); } while (0)
#define	pwm_apply_might_sleep(p, s)	(-ENODEV)
#define	pwm_get_state(p, s)		do { (void)(p); (void)(s); } while (0)
#define	pwm_get_relative_duty_cycle(s, scale) (0)
#define	pwm_set_relative_duty_cycle(s, d, scale) (0)
#define	pwm_init_state(p, s)		do { } while (0)
#define	pwm_lookup_init(l, n)		do { } while (0)
#define	pwm_remove_table(l, n)		do { } while (0)
#define	PWM_LOOKUP(chip, idx, dev, con, per, pol) { 0 }
struct pwm_lookup {
	int	dummy;
};

#define	pwm_is_enabled(p)		((void)(p), false)

#endif /* _LINUXKPI_INCLUDE_LINUX_PWM_H_ */
