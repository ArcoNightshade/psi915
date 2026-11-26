/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux kobject basics from their
 * documented behaviour (Documentation/core-api/kobject.rst); function set
 * cross-checked against FreeBSD's LinuxKPI kobject.h (BSD-2-Clause).
 *
 * A kobject is a name, a parent pointer and a kref whose release calls
 * ktype->release. There is no sysfs: kobject_add() only records the name
 * and parent, attributes are accepted and ignored (<linux/sysfs.h>).
 * kobject_uevent() is delivered as an illumos sysevent (ddi_log_sysevent)
 * on the dev_info_t of the nearest struct device ancestor, class
 * "EC_linuxkpi", subclass "ESC_uevent", with string attributes ACTION,
 * DEVPATH and every KEY=VALUE of the environment (src/linux_device.c).
 */
#ifndef _LINUXKPI_LINUX_KOBJECT_H_
#define	_LINUXKPI_LINUX_KOBJECT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/sysfs.h>

struct kobject;
struct kset;

struct kobj_type {
	void			(*release)(struct kobject *kobj);
	const struct sysfs_ops	*sysfs_ops;
	const struct attribute_group **default_groups;
};

struct kobject {
	const char		*name;
	struct kobject		*parent;
	struct kset		*kset;
	const struct kobj_type	*ktype;
	struct kref		kref;
	struct list_head	entry;
	unsigned int		state_initialized:1;
	unsigned int		state_in_sysfs:1;
	unsigned int		uevent_suppress:1;
	/* KPI private: struct device owning this kobject, if any */
	void			*lkpi_device;
};

struct kset {
	struct list_head	list;
	struct kobject		kobj;
};

enum kobject_action {
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
	KOBJ_MOVE,
	KOBJ_ONLINE,
	KOBJ_OFFLINE,
	KOBJ_BIND,
	KOBJ_UNBIND
};

#define	UEVENT_NUM_ENVP		64
#define	UEVENT_BUFFER_SIZE	2048

struct kobj_uevent_env {
	char	*envp[UEVENT_NUM_ENVP];
	int	envp_idx;
	char	buf[UEVENT_BUFFER_SIZE];
	int	buflen;
};

void kobject_init(struct kobject *kobj, const struct kobj_type *ktype);
int kobject_set_name(struct kobject *kobj, const char *fmt, ...) __printf(2, 3);
int kobject_add(struct kobject *kobj, struct kobject *parent,
    const char *fmt, ...) __printf(3, 4);
int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...) __printf(4, 5);
void kobject_del(struct kobject *kobj);
struct kobject *kobject_get(struct kobject *kobj);
struct kobject *kobject_get_unless_zero(struct kobject *kobj);
void kobject_put(struct kobject *kobj);
struct kobject *kobject_create_and_add(const char *name,
    struct kobject *parent);
char *kobject_get_path(const struct kobject *kobj, gfp_t gfp);
int kobject_uevent(struct kobject *kobj, enum kobject_action action);
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,
    char *envp[]);
int add_uevent_var(struct kobj_uevent_env *env, const char *fmt, ...)
    __printf(2, 3);

static inline const char *
kobject_name(const struct kobject *kobj)
{
	return (kobj->name);
}

extern struct kobject *kernel_kobj;

#endif /* _LINUXKPI_LINUX_KOBJECT_H_ */
