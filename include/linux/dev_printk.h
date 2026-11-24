/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * dev_printk API; cross-checked against FreeBSD's LinuxKPI device.h
 * (BSD-2-Clause).
 *
 * Messages are prefixed with "<device name>: ". struct device arrives in
 * P2; until then the name comes from lkpi_dev_name_hook (set by the device
 * layer), and is "(device)" by default or "(NULL device)" for NULL.
 */
#ifndef _LINUXKPI_LINUX_DEV_PRINTK_H_
#define	_LINUXKPI_LINUX_DEV_PRINTK_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/printk.h>
#include <linux/bug.h>

struct device;

extern const char *(*lkpi_dev_name_hook)(const struct device *dev);

void lkpi_dev_printk(const char *level, const struct device *dev,
    const char *fmt, ...) __printf(3, 4);

#define	dev_printk(level, dev, fmt, ...)				\
	lkpi_dev_printk(level, dev, fmt, ##__VA_ARGS__)

#define	dev_emerg(dev, fmt, ...)	dev_printk(KERN_EMERG, dev, fmt, ##__VA_ARGS__)
#define	dev_alert(dev, fmt, ...)	dev_printk(KERN_ALERT, dev, fmt, ##__VA_ARGS__)
#define	dev_crit(dev, fmt, ...)		dev_printk(KERN_CRIT, dev, fmt, ##__VA_ARGS__)
#define	dev_err(dev, fmt, ...)		dev_printk(KERN_ERR, dev, fmt, ##__VA_ARGS__)
#define	dev_warn(dev, fmt, ...)		dev_printk(KERN_WARNING, dev, fmt, ##__VA_ARGS__)
#define	dev_notice(dev, fmt, ...)	dev_printk(KERN_NOTICE, dev, fmt, ##__VA_ARGS__)
#define	dev_info(dev, fmt, ...)		dev_printk(KERN_INFO, dev, fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define	dev_dbg(dev, fmt, ...)		dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__)
#else
#define	dev_dbg(dev, fmt, ...) ({					\
	if (0)								\
		dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__);	\
})
#endif
#define	dev_vdbg(dev, fmt, ...)		dev_dbg(dev, fmt, ##__VA_ARGS__)

#define	dev_level_once(fn, dev, fmt, ...) ({				\
	static bool __lkpi_dev_once;					\
									\
	if (!__lkpi_dev_once) {						\
		__lkpi_dev_once = true;					\
		fn(dev, fmt, ##__VA_ARGS__);				\
	}								\
})

#define	dev_emerg_once(dev, fmt, ...)	dev_level_once(dev_emerg, dev, fmt, ##__VA_ARGS__)
#define	dev_alert_once(dev, fmt, ...)	dev_level_once(dev_alert, dev, fmt, ##__VA_ARGS__)
#define	dev_crit_once(dev, fmt, ...)	dev_level_once(dev_crit, dev, fmt, ##__VA_ARGS__)
#define	dev_err_once(dev, fmt, ...)	dev_level_once(dev_err, dev, fmt, ##__VA_ARGS__)
#define	dev_warn_once(dev, fmt, ...)	dev_level_once(dev_warn, dev, fmt, ##__VA_ARGS__)
#define	dev_notice_once(dev, fmt, ...)	dev_level_once(dev_notice, dev, fmt, ##__VA_ARGS__)
#define	dev_info_once(dev, fmt, ...)	dev_level_once(dev_info, dev, fmt, ##__VA_ARGS__)
#define	dev_dbg_once(dev, fmt, ...)	dev_level_once(dev_dbg, dev, fmt, ##__VA_ARGS__)

#define	dev_level_ratelimited(fn, dev, fmt, ...) ({			\
	static struct ratelimit_state __lkpi_dev_rs =			\
	    RATELIMIT_STATE_INIT(__lkpi_dev_rs, 0, DEFAULT_RATELIMIT_BURST); \
									\
	if (__ratelimit(&__lkpi_dev_rs))				\
		fn(dev, fmt, ##__VA_ARGS__);				\
})

#define	dev_err_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_err, dev, fmt, ##__VA_ARGS__)
#define	dev_warn_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_warn, dev, fmt, ##__VA_ARGS__)
#define	dev_notice_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_notice, dev, fmt, ##__VA_ARGS__)
#define	dev_info_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_info, dev, fmt, ##__VA_ARGS__)
#define	dev_dbg_ratelimited(dev, fmt, ...)				\
	dev_level_ratelimited(dev_dbg, dev, fmt, ##__VA_ARGS__)

#define	dev_WARN(dev, fmt, ...)						\
	WARN(1, "%s: " fmt, lkpi_dev_name_hook(dev), ##__VA_ARGS__)
#define	dev_WARN_ONCE(dev, cond, fmt, ...)				\
	WARN_ONCE(cond, "%s: " fmt, lkpi_dev_name_hook(dev), ##__VA_ARGS__)

/*
 * Log a probe failure unless it is -EPROBE_DEFER (logged at debug level)
 * and return err.
 */
int lkpi_dev_err_probe(const struct device *dev, int err, const char *fmt,
    ...) __printf(3, 4);
#define	dev_err_probe(dev, err, fmt, ...)				\
	lkpi_dev_err_probe(dev, err, fmt, ##__VA_ARGS__)

#endif /* _LINUXKPI_LINUX_DEV_PRINTK_H_ */
