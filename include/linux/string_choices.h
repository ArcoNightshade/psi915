/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Boolean-to-string helpers.
 */
#ifndef _LINUXKPI_LINUX_STRING_CHOICES_H_
#define	_LINUXKPI_LINUX_STRING_CHOICES_H_

#include <linux/types.h>

static inline const char *str_yes_no(bool v) { return (v ? "yes" : "no"); }
static inline const char *str_on_off(bool v) { return (v ? "on" : "off"); }
static inline const char *str_true_false(bool v) { return (v ? "true" : "false"); }
static inline const char *str_enable_disable(bool v) { return (v ? "enable" : "disable"); }
static inline const char *str_enabled_disabled(bool v) { return (v ? "enabled" : "disabled"); }
static inline const char *str_read_write(bool v) { return (v ? "read" : "write"); }
static inline const char *str_high_low(bool v) { return (v ? "high" : "low"); }
static inline const char *str_up_down(bool v) { return (v ? "up" : "down"); }
static inline const char *str_plural(size_t n) { return (n == 1 ? "" : "s"); }
#define	str_disable_enable(v)	str_enable_disable(!(v))
#define	str_disabled_enabled(v)	str_enabled_disabled(!(v))
#define	str_no_yes(v)		str_yes_no(!(v))
#define	str_off_on(v)		str_on_off(!(v))
#define	str_low_high(v)		str_high_low(!(v))
#define	str_down_up(v)		str_up_down(!(v))
#define	str_write_read(v)	str_read_write(!(v))

#endif /* _LINUXKPI_LINUX_STRING_CHOICES_H_ */
