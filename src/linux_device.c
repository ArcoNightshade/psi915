/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * kobject, struct device, classes, uevents, sysfs_emit (see
 * <linux/kobject.h>, <linux/device.h>, <linux/sysfs.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/sysctl.h>
#include <linux/relay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linuxkpi/plat_dev.h>

struct kobject *kernel_kobj;

static char *
lkpi_vasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	char tmp[128], *s;
	va_list ap2;
	int n;

	va_copy(ap2, ap);
	n = lkpi_plat_vsnprintf(tmp, sizeof (tmp), fmt, ap2);
	va_end(ap2);
	if (n < 0)
		return (NULL);
	s = kmalloc((size_t)n + 1, gfp);
	if (s == NULL)
		return (NULL);
	if ((size_t)n < sizeof (tmp))
		memcpy(s, tmp, (size_t)n + 1);
	else
		(void) lkpi_plat_vsnprintf(s, (size_t)n + 1, fmt, ap);
	return (s);
}

static int
lkpi_kobject_vset_name(struct kobject *kobj, const char *fmt, va_list ap)
{
	char *s = lkpi_vasprintf(GFP_KERNEL, fmt, ap);

	if (s == NULL)
		return (-ENOMEM);
	kfree(kobj->name);
	kobj->name = s;
	return (0);
}

int
kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = lkpi_kobject_vset_name(kobj, fmt, ap);
	va_end(ap);
	return (r);
}

void
kobject_init(struct kobject *kobj, const struct kobj_type *ktype)
{
	kref_init(&kobj->kref);
	INIT_LIST_HEAD(&kobj->entry);
	kobj->ktype = ktype;
	kobj->state_initialized = 1;
}

static int
lkpi_kobject_vadd(struct kobject *kobj, struct kobject *parent,
    const char *fmt, va_list ap)
{
	int r = 0;

	if (fmt != NULL)
		r = lkpi_kobject_vset_name(kobj, fmt, ap);
	if (r != 0)
		return (r);
	kobj->parent = parent != NULL ? kobject_get(parent) : NULL;
	kobj->state_in_sysfs = 1;
	return (0);
}

int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt,
    ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = lkpi_kobject_vadd(kobj, parent, fmt, ap);
	va_end(ap);
	return (r);
}

int
kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...)
{
	va_list ap;
	int r;

	kobject_init(kobj, ktype);
	va_start(ap, fmt);
	r = lkpi_kobject_vadd(kobj, parent, fmt, ap);
	va_end(ap);
	return (r);
}

void
kobject_del(struct kobject *kobj)
{
	struct kobject *parent;

	if (kobj == NULL || !kobj->state_in_sysfs)
		return;
	kobj->state_in_sysfs = 0;
	parent = kobj->parent;
	kobj->parent = NULL;
	if (parent != NULL)
		kobject_put(parent);
}

struct kobject *
kobject_get(struct kobject *kobj)
{
	if (kobj != NULL)
		kref_get(&kobj->kref);
	return (kobj);
}

struct kobject *
kobject_get_unless_zero(struct kobject *kobj)
{
	if (kobj != NULL && !kref_get_unless_zero(&kobj->kref))
		return (NULL);
	return (kobj);
}

static void
lkpi_kobject_release(struct kref *kref)
{
	struct kobject *kobj = container_of(kref, struct kobject, kref);
	const char *name = kobj->name;

	kobject_del(kobj);
	kobj->name = NULL;
	if (kobj->ktype != NULL && kobj->ktype->release != NULL)
		kobj->ktype->release(kobj);
	kfree(name);
}

void
kobject_put(struct kobject *kobj)
{
	if (kobj != NULL)
		(void) kref_put(&kobj->kref, lkpi_kobject_release);
}

static void
lkpi_dynamic_kobj_release(struct kobject *kobj)
{
	kfree(kobj);
}

static const struct kobj_type lkpi_dynamic_ktype = {
	.release = lkpi_dynamic_kobj_release,
};

struct kobject *
kobject_create_and_add(const char *name, struct kobject *parent)
{
	struct kobject *kobj = kzalloc(sizeof (*kobj), GFP_KERNEL);

	if (kobj == NULL)
		return (NULL);
	if (kobject_init_and_add(kobj, &lkpi_dynamic_ktype, parent, "%s",
	    name) != 0) {
		kobject_put(kobj);
		return (NULL);
	}
	return (kobj);
}

/* "/parent/.../name" */
char *
kobject_get_path(const struct kobject *kobj, gfp_t gfp)
{
	const struct kobject *k;
	size_t len = 1, pos;
	char *path;

	for (k = kobj; k != NULL; k = k->parent)
		len += strlen(k->name != NULL ? k->name : "") + 1;
	path = kzalloc(len, gfp);
	if (path == NULL)
		return (NULL);
	pos = len - 1;
	for (k = kobj; k != NULL; k = k->parent) {
		const char *n = k->name != NULL ? k->name : "";
		size_t l = strlen(n);

		pos -= l;
		memcpy(path + pos, n, l);
		path[--pos] = '/';
	}
	return (path);
}

int
add_uevent_var(struct kobj_uevent_env *env, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (env->envp_idx >= UEVENT_NUM_ENVP)
		return (-ENOMEM);
	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(env->buf + env->buflen,
	    sizeof (env->buf) - (size_t)env->buflen, fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int)sizeof (env->buf) - env->buflen)
		return (-ENOMEM);
	env->envp[env->envp_idx++] = env->buf + env->buflen;
	env->buflen += n + 1;
	return (0);
}

static const char *const lkpi_kobject_actions[] = {
	[KOBJ_ADD] = "add", [KOBJ_REMOVE] = "remove",
	[KOBJ_CHANGE] = "change", [KOBJ_MOVE] = "move",
	[KOBJ_ONLINE] = "online", [KOBJ_OFFLINE] = "offline",
	[KOBJ_BIND] = "bind", [KOBJ_UNBIND] = "unbind",
};

int
kobject_uevent_env(struct kobject *kobj, enum kobject_action action,
    char *envp[])
{
	struct kobj_uevent_env *env;
	struct kobject *k;
	struct device *dev = NULL;
	char *path;
	int i;

	if (kobj->uevent_suppress)
		return (0);
	for (k = kobj; k != NULL; k = k->parent) {
		if (k->lkpi_device != NULL &&
		    lkpi_dev_dip(k->lkpi_device) != NULL) {
			dev = k->lkpi_device;
			break;
		}
	}
	env = kzalloc(sizeof (*env), GFP_KERNEL);
	path = kobject_get_path(kobj, GFP_KERNEL);
	if (env == NULL || path == NULL) {
		kfree(env);
		kfree(path);
		return (-ENOMEM);
	}
	for (i = 0; envp != NULL && envp[i] != NULL; i++)
		(void) add_uevent_var(env, "%s", envp[i]);
	if (kobj->lkpi_device != NULL) {
		struct device *d = kobj->lkpi_device;

		if (d->type != NULL && d->type->uevent != NULL)
			(void) d->type->uevent(d, env);
		if (d->class != NULL && d->class->dev_uevent != NULL)
			(void) d->class->dev_uevent(d, env);
	}
	lkpi_plat_uevent(dev != NULL ? lkpi_dev_dip(dev) : NULL,
	    lkpi_kobject_actions[action], path, env->envp, env->envp_idx);
	kfree(path);
	kfree(env);
	return (0);
}

int
kobject_uevent(struct kobject *kobj, enum kobject_action action)
{
	return (kobject_uevent_env(kobj, action, NULL));
}

/* struct device */

static void
lkpi_device_release(struct kobject *kobj)
{
	struct device *dev = kobj_to_dev(kobj);

	(void) devres_release_all(dev);
	if (dev->release != NULL)
		dev->release(dev);
	else if (dev->type != NULL && dev->type->release != NULL)
		dev->type->release(dev);
	else if (dev->class != NULL && dev->class->dev_release != NULL)
		dev->class->dev_release(dev);
	else
		WARN(1, "linuxkpi: device %p has no release function\n", dev);
}

static const struct kobj_type lkpi_device_ktype = {
	.release = lkpi_device_release,
};

void
device_initialize(struct device *dev)
{
	kobject_init(&dev->kobj, &lkpi_device_ktype);
	dev->kobj.lkpi_device = dev;
	INIT_LIST_HEAD(&dev->lkpi_devres);
	spin_lock_init(&dev->lkpi_devres_lock);
	atomic_set(&dev->power.usage_count, 0);
	dev->numa_node = -1;
	dev->lkpi_initialized = true;
}

int
device_add(struct device *dev)
{
	int r;

	if (dev->init_name != NULL && dev->kobj.name == NULL) {
		r = kobject_set_name(&dev->kobj, "%s", dev->init_name);
		if (r != 0)
			return (r);
		dev->init_name = NULL;
	}
	r = kobject_add(&dev->kobj,
	    dev->parent != NULL ? &dev->parent->kobj : NULL, NULL);
	if (r != 0)
		return (r);
	if (dev->parent != NULL)
		(void) get_device(dev->parent);
	return (kobject_uevent(&dev->kobj, KOBJ_ADD));
}

void
device_del(struct device *dev)
{
	struct device *parent = dev->parent;

	(void) kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	kobject_del(&dev->kobj);
	if (parent != NULL)
		put_device(parent);
}

int
device_register(struct device *dev)
{
	device_initialize(dev);
	return (device_add(dev));
}

void
device_unregister(struct device *dev)
{
	device_del(dev);
	put_device(dev);
}

struct device *
get_device(struct device *dev)
{
	if (dev != NULL)
		(void) kobject_get(&dev->kobj);
	return (dev);
}

void
put_device(struct device *dev)
{
	if (dev != NULL)
		kobject_put(&dev->kobj);
}

int
dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = lkpi_kobject_vset_name(&dev->kobj, fmt, ap);
	va_end(ap);
	return (r);
}

const char *
dev_name(const struct device *dev)
{
	if (dev->init_name != NULL)
		return (dev->init_name);
	return (dev->kobj.name != NULL ? dev->kobj.name : "(unnamed)");
}

const char *
lkpi_device_name(const struct device *dev)
{
	return (dev_name(dev));
}

void *
lkpi_dev_dip(const struct device *dev)
{
	for (; dev != NULL; dev = dev->parent)
		if (dev->lkpi_dip != NULL)
			return (dev->lkpi_dip);
	return (NULL);
}

/* Classes: no sysfs, so just objects with a name. */

struct class *
class_create(const char *name)
{
	struct class *cls = kzalloc(sizeof (*cls), GFP_KERNEL);

	if (cls == NULL)
		return (ERR_PTR(-ENOMEM));
	cls->name = name;
	return (cls);
}

void
class_destroy(const struct class *cls)
{
	if (cls != NULL && !IS_ERR(cls))
		kfree(cls);
}

int
class_register(const struct class *cls)
{
	(void) cls;
	return (0);
}

void
class_unregister(const struct class *cls)
{
	(void) cls;
}

/*
 * device_create() devices are tracked so device_destroy() can find them
 * by (class, devt).
 */
struct lkpi_class_dev {
	struct device		dev;
	struct list_head	entry;
};

static LIST_HEAD(lkpi_class_devs);
static DEFINE_MUTEX(lkpi_class_lock);

static void
lkpi_class_cdev_release(struct device *dev)
{
	struct lkpi_class_dev *cd = container_of(dev, struct lkpi_class_dev,
	    dev);

	kfree(cd);
}

struct device *
device_create(const struct class *cls, struct device *parent, dev_t devt,
    void *drvdata, const char *fmt, ...)
{
	struct lkpi_class_dev *cd;
	va_list ap;
	int r;

	cd = kzalloc(sizeof (*cd), GFP_KERNEL);
	if (cd == NULL)
		return (ERR_PTR(-ENOMEM));
	device_initialize(&cd->dev);
	cd->dev.class = cls;
	cd->dev.parent = parent;
	cd->dev.devt = devt;
	cd->dev.driver_data = drvdata;
	cd->dev.release = lkpi_class_cdev_release;
	va_start(ap, fmt);
	r = lkpi_kobject_vset_name(&cd->dev.kobj, fmt, ap);
	va_end(ap);
	if (r == 0)
		r = device_add(&cd->dev);
	if (r != 0) {
		put_device(&cd->dev);
		return (ERR_PTR(r));
	}
	mutex_lock(&lkpi_class_lock);
	list_add_tail(&cd->entry, &lkpi_class_devs);
	mutex_unlock(&lkpi_class_lock);
	return (&cd->dev);
}

void
device_destroy(const struct class *cls, dev_t devt)
{
	struct lkpi_class_dev *cd, *found = NULL;

	mutex_lock(&lkpi_class_lock);
	list_for_each_entry(cd, &lkpi_class_devs, entry) {
		if (cd->dev.class == cls && cd->dev.devt == devt) {
			found = cd;
			list_del(&cd->entry);
			break;
		}
	}
	mutex_unlock(&lkpi_class_lock);
	if (found != NULL)
		device_unregister(&found->dev);
}

int
bus_register(const struct bus_type *bus)
{
	(void) bus;
	return (0);
}

void
bus_unregister(const struct bus_type *bus)
{
	(void) bus;
}

int
driver_register(struct device_driver *drv)
{
	(void) drv;
	return (0);
}

void
driver_unregister(struct device_driver *drv)
{
	(void) drv;
}

int
sysfs_emit(char *buf, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(buf, 4096, fmt, ap);
	va_end(ap);
	return (n < 4096 ? n : 4095);
}

int
sysfs_emit_at(char *buf, int at, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (at < 0 || at >= 4096)
		return (0);
	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(buf + at, (size_t)(4096 - at), fmt, ap);
	va_end(ap);
	return (n < 4096 - at ? n : 4095 - at);
}

/* seq_file output into a caller buffer (see <linux/seq_file.h>). */
#include <linux/seq_file.h>

void
seq_write(struct seq_file *m, const void *data, size_t len)
{
	size_t room;
	bool over = false;

	if (m->size == 0 || m->count >= m->size)
		return;
	room = m->size - m->count - 1;
	if (len > room) {
		len = room;
		over = true;
	}
	memcpy(m->buf + m->count, data, len);
	m->buf[m->count + len] = '\0';
	m->count = over ? m->size : m->count + len;
}

void
seq_puts(struct seq_file *m, const char *s)
{
	seq_write(m, s, strlen(s));
}

void
seq_putc(struct seq_file *m, char c)
{
	seq_write(m, &c, 1);
}

void
seq_printf(struct seq_file *m, const char *fmt, ...)
{
	char tmp[256];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(tmp, sizeof (tmp), fmt, ap);
	va_end(ap);
	if (n > 0)
		seq_write(m, tmp, (size_t)n < sizeof (tmp) ? (size_t)n :
		    sizeof (tmp) - 1);
}

int
single_open(struct lkpi_file *f, int (*show)(struct seq_file *, void *),
    void *data)
{
	(void) f;
	(void) show;
	(void) data;
	return (-ENODEV);
}

int
single_release(struct inode *inode, struct lkpi_file *f)
{
	(void) inode;
	(void) f;
	return (0);
}

int
seq_release(struct inode *inode, struct lkpi_file *f)
{
	(void) inode;
	(void) f;
	return (0);
}

long
seq_read(struct lkpi_file *f, char __user *buf, size_t size, long long *ppos)
{
	(void) f;
	(void) buf;
	(void) size;
	(void) ppos;
	return (-ENODEV);
}

long long
seq_lseek(struct lkpi_file *f, long long off, int whence)
{
	(void) f;
	(void) off;
	(void) whence;
	return (-ESPIPE);
}

const struct sysfs_ops kobj_sysfs_ops;

/* sysctl: tables are accepted but not published (no /proc/sys). */
const int lkpi_sysctl_vals[] = { 0, 1, 0x7fffffff };

int
proc_dointvec(const struct ctl_table *table, int write, void *buffer,
    size_t *lenp, long long *ppos)
{
	(void) table;
	(void) write;
	(void) buffer;
	(void) lenp;
	(void) ppos;
	return (-EOPNOTSUPP);
}

int
proc_dointvec_minmax(const struct ctl_table *table, int write, void *buffer,
    size_t *lenp, long long *ppos)
{
	return (proc_dointvec(table, write, buffer, lenp, ppos));
}

struct ctl_table_header *
register_sysctl(const char *path, const struct ctl_table *table)
{
	struct ctl_table_header *h;

	(void) path;
	h = kzalloc(sizeof (*h), GFP_KERNEL);
	if (h != NULL)
		h->lkpi_table = table;
	return (h);
}

void
unregister_sysctl_table(struct ctl_table_header *h)
{
	kfree(h);
}

/* relay files are never created (relay_open fails). */
const struct file_operations relay_file_operations;
