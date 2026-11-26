/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Backlight class: no backlight devices are registered on illumos yet
 * (backlight_device_register() returns -ENODEV, which i915 tolerates);
 * the types let drivers compile. A kstat/ioctl brightness control is P4
 * work.
 */
#ifndef _LINUXKPI_LINUX_BACKLIGHT_H_
#define	_LINUXKPI_LINUX_BACKLIGHT_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mutex.h>

enum backlight_type {
	BACKLIGHT_RAW = 1,
	BACKLIGHT_PLATFORM,
	BACKLIGHT_FIRMWARE,
	BACKLIGHT_TYPE_MAX
};

enum backlight_scale {
	BACKLIGHT_SCALE_UNKNOWN = 0,
	BACKLIGHT_SCALE_LINEAR,
	BACKLIGHT_SCALE_NON_LINEAR
};

#define	BACKLIGHT_POWER_ON	0
#define	BACKLIGHT_POWER_OFF	4
#define	FB_BLANK_UNBLANK	0
#define	FB_BLANK_POWERDOWN	4

struct backlight_properties {
	int			brightness;
	int			max_brightness;
	int			power;
	enum backlight_type	type;
	unsigned int		state;
	enum backlight_scale	scale;
};

struct backlight_device;
struct backlight_ops {
	unsigned int	options;
	int		(*update_status)(struct backlight_device *);
	int		(*get_brightness)(struct backlight_device *);
};

struct backlight_device {
	struct backlight_properties	props;
	struct mutex			update_lock;
	struct mutex			ops_lock;
	const struct backlight_ops	*ops;
	struct device			dev;
	int				use_count;
};

#define	BL_CORE_SUSPENDED	(1 << 0)
#define	BL_CORE_FBBLANK		(1 << 1)

static inline struct backlight_device *
backlight_device_register(const char *name, struct device *parent, void *data,
    const struct backlight_ops *ops, const struct backlight_properties *props)
{
	(void) name; (void) parent; (void) data; (void) ops; (void) props;
	return (ERR_PTR(-ENODEV));
}

#define	devm_backlight_device_register(d, n, p, dd, o, pr)		\
	backlight_device_register(n, p, dd, o, pr)
#define	backlight_device_unregister(bd)	do { (void)(bd); } while (0)
#define	backlight_device_get_by_name(n)	((struct backlight_device *)NULL)
#define	backlight_device_get_by_type(t)	((struct backlight_device *)NULL)
#define	put_device_backlight(bd)	do { } while (0)
#define	backlight_force_update(bd, r)	do { (void)(bd); } while (0)
#define	bl_get_data(bd)			dev_get_drvdata(&(bd)->dev)

static inline int
backlight_update_status(struct backlight_device *bd)
{
	return (bd->ops != NULL && bd->ops->update_status != NULL ?
	    bd->ops->update_status(bd) : 0);
}

static inline int
backlight_get_brightness(const struct backlight_device *bd)
{
	return (bd->props.power != BACKLIGHT_POWER_ON ? 0 :
	    bd->props.brightness);
}

static inline bool
backlight_is_blank(const struct backlight_device *bd)
{
	return (bd->props.power != BACKLIGHT_POWER_ON);
}

#define	backlight_enable(bd)		((void)(bd), 0)
#define	backlight_disable(bd)		((void)(bd), 0)

#endif /* _LINUXKPI_LINUX_BACKLIGHT_H_ */
