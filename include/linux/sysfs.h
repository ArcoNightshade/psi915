/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. sysfs does not exist on illumos: attribute types are
 * provided so drivers compile, and every create/remove call succeeds
 * without effect. sysfs_emit()/sysfs_emit_at() format into a page-sized
 * buffer as in Linux.
 */
#ifndef _LINUXKPI_LINUX_SYSFS_H_
#define	_LINUXKPI_LINUX_SYSFS_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stringify.h>

struct kobject;
struct lkpi_module;

struct attribute {
	const char	*name;
	unsigned short	mode;
};

struct bin_attribute;
struct attribute_group {
	const char		*name;
	unsigned short		(*is_visible)(struct kobject *, struct attribute *,
				    int);
	struct attribute	**attrs;
	struct bin_attribute	**bin_attrs;
};

struct bin_attribute {
	struct attribute	attr;
	size_t			size;
	void			*private;
	ssize_t			(*read)(void *f, struct kobject *, struct bin_attribute *,
				    char *, long long, size_t);
	ssize_t			(*write)(void *f, struct kobject *,
				    struct bin_attribute *, char *, long long, size_t);
};

struct sysfs_ops {
	ssize_t	(*show)(struct kobject *, struct attribute *, char *);
	ssize_t	(*store)(struct kobject *, struct attribute *, const char *,
		    size_t);
};

struct kobj_attribute {
	struct attribute attr;
	ssize_t	(*show)(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf);
	ssize_t	(*store)(struct kobject *kobj, struct kobj_attribute *attr,
		    const char *buf, size_t count);
};

#define	__ATTR(_name, _mode, _show, _store)				\
	{ .attr = { .name = __stringify(_name), .mode = (_mode) },	\
	    .show = (_show), .store = (_store) }
#define	__ATTR_RO(_name)	__ATTR(_name, 0444, _name##_show, NULL)
#define	__ATTR_RO_MODE(_name, _mode) __ATTR(_name, _mode, _name##_show, NULL)
#define	__ATTR_WO(_name)	__ATTR(_name, 0200, NULL, _name##_store)
#define	__ATTR_RW(_name)	__ATTR(_name, 0644, _name##_show, _name##_store)
#define	__ATTR_RW_MODE(_name, _mode) __ATTR(_name, _mode, _name##_show, _name##_store)
#define	__ATTR_NULL		{ .attr = { .name = NULL } }
#define	__BIN_ATTR(_name, _mode, _read, _write, _size)			\
	{ .attr = { .name = __stringify(_name), .mode = (_mode) },	\
	    .read = (_read), .write = (_write), .size = (_size) }
#define	__BIN_ATTR_RO(_name, _size) __BIN_ATTR(_name, 0444, _name##_read, NULL, _size)
#define	BIN_ATTR_RO(_name, _size)					\
	struct bin_attribute bin_attr_##_name = __BIN_ATTR_RO(_name, _size)
#define	ATTRIBUTE_GROUPS(_name)						\
	static const struct attribute_group _name##_group = {		\
		.attrs = _name##_attrs };				\
	static const struct attribute_group *_name##_groups[] = {	\
		&_name##_group, NULL }

#define	sysfs_create_file(k, a)			((void)(k), (void)(a), 0)
#define	sysfs_remove_file(k, a)			do { (void)(k); (void)(a); } while (0)
#define	sysfs_create_files(k, a)		((void)(k), (void)(a), 0)
#define	sysfs_remove_files(k, a)		do { (void)(k); (void)(a); } while (0)
#define	sysfs_create_group(k, g)		((void)(k), (void)(g), 0)
#define	sysfs_remove_group(k, g)		do { (void)(k); (void)(g); } while (0)
#define	sysfs_create_groups(k, g)		((void)(k), (void)(g), 0)
#define	sysfs_remove_groups(k, g)		do { (void)(k); (void)(g); } while (0)
#define	sysfs_merge_group(k, g)			((void)(k), (void)(g), 0)
#define	sysfs_unmerge_group(k, g)		do { (void)(k); (void)(g); } while (0)
#define	sysfs_update_group(k, g)		((void)(k), (void)(g), 0)
#define	sysfs_create_bin_file(k, a)		((void)(k), (void)(a), 0)
#define	sysfs_remove_bin_file(k, a)		do { (void)(k); (void)(a); } while (0)
#define	sysfs_create_link(k, t, n)		((void)(k), (void)(t), 0)
#define	sysfs_remove_link(k, n)			do { (void)(k); } while (0)
#define	sysfs_notify(k, d, a)			do { (void)(k); } while (0)
#define	sysfs_attr_init(a)			do { (void)(a); } while (0)
#define	sysfs_bin_attr_init(a)			do { (void)(a); } while (0)

int sysfs_emit(char *buf, const char *fmt, ...) __printf(2, 3);
int sysfs_emit_at(char *buf, int at, const char *fmt, ...) __printf(3, 4);

#endif /* _LINUXKPI_LINUX_SYSFS_H_ */
