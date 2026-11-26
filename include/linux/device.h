/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux driver-model basics (struct
 * device, bus_type, device_driver, class) from their documented
 * behaviour; member set cross-checked against FreeBSD's LinuxKPI
 * device.h (BSD-2-Clause).
 *
 * A struct device is refcounted through its kobject; the last
 * put_device() calls dev->release (or the class/type release). There is
 * no driver core matching: bus drivers (the PCI glue) bind devices to
 * drivers themselves. lkpi_dip is the illumos dev_info_t backing the
 * device (inherited from the parent when NULL), used for DMA, interrupts,
 * properties and sysevents.
 *
 * devres (managed resources): devm_*() allocations and actions are
 * released in reverse order by devres_release_all(), which the PCI glue
 * calls after remove() and when probe() fails (src/linux_devres.c).
 */
#ifndef _LINUXKPI_LINUX_DEVICE_H_
#define	_LINUXKPI_LINUX_DEVICE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/pm.h>
#include <linux/sysfs.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>

struct device;
struct device_driver;
struct bus_type;
struct class;
struct device_type;
struct fwnode_handle;
struct device_node;
struct dma_map_ops;

/* dev_t is the environment's (illumos <sys/types.h>, 64-bit). */

struct device_dma_parameters {
	unsigned int	max_segment_size;
	unsigned int	min_align_mask;
	unsigned long	segment_boundary_mask;
};

struct bus_type {
	const char		*name;
	int			(*match)(struct device *dev,
				    const struct device_driver *drv);
	int			(*probe)(struct device *dev);
	void			(*remove)(struct device *dev);
	void			(*shutdown)(struct device *dev);
	const struct dev_pm_ops	*pm;
};

enum probe_type {
	PROBE_DEFAULT_STRATEGY,
	PROBE_PREFER_ASYNCHRONOUS,
	PROBE_FORCE_SYNCHRONOUS
};

struct device_driver {
	const char		*name;
	const struct bus_type	*bus;
	struct lkpi_module	*owner;
	const char		*mod_name;
	bool			suppress_bind_attrs;
	enum probe_type		probe_type;
	int			(*probe)(struct device *dev);
	int			(*remove)(struct device *dev);
	void			(*shutdown)(struct device *dev);
	const struct dev_pm_ops	*pm;
	const struct attribute_group **groups;
	const struct attribute_group **dev_groups;
};

struct class {
	const char		*name;
	const struct attribute_group **class_groups;
	const struct attribute_group **dev_groups;
	int			(*dev_uevent)(const struct device *dev,
				    struct kobj_uevent_env *env);
	char			*(*devnode)(const struct device *dev,
				    unsigned short *mode);
	void			(*class_release)(const struct class *class);
	void			(*dev_release)(struct device *dev);
	const struct dev_pm_ops	*pm;
};

struct device_type {
	const char		*name;
	const struct attribute_group **groups;
	int			(*uevent)(const struct device *dev,
				    struct kobj_uevent_env *env);
	char			*(*devnode)(const struct device *dev,
				    unsigned short *mode);
	void			(*release)(struct device *dev);
	const struct dev_pm_ops	*pm;
};

struct device {
	struct kobject		kobj;
	struct device		*parent;
	const char		*init_name;
	const struct device_type *type;
	const struct bus_type	*bus;
	struct device_driver	*driver;
	void			*platform_data;
	void			*driver_data;
	struct dev_pm_info	power;
	u64			*dma_mask;
	u64			coherent_dma_mask;
	u64			bus_dma_limit;
	struct device_dma_parameters *dma_parms;
	const struct dma_map_ops *dma_ops;
	struct device_node	*of_node;
	struct fwnode_handle	*fwnode;
	int			numa_node;
	dev_t			devt;
	u32			id;
	const struct class	*class;
	const struct attribute_group **groups;
	void			(*release)(struct device *dev);
	bool			offline_disabled:1;
	bool			offline:1;
	bool			dma_coherent:1;
	/* KPI private */
	void			*lkpi_dip;	/* dev_info_t * */
	struct list_head	lkpi_devres;
	spinlock_t		lkpi_devres_lock;
	bool			lkpi_initialized;
};

static inline struct device *
kobj_to_dev(struct kobject *kobj)
{
	return (container_of(kobj, struct device, kobj));
}

void device_initialize(struct device *dev);
int device_add(struct device *dev);
void device_del(struct device *dev);
int device_register(struct device *dev);
void device_unregister(struct device *dev);
struct device *get_device(struct device *dev);
void put_device(struct device *dev);
int dev_set_name(struct device *dev, const char *fmt, ...) __printf(2, 3);
const char *dev_name(const struct device *dev);
/* dev_info_t of dev or of its nearest ancestor that has one. */
void *lkpi_dev_dip(const struct device *dev);

static inline void *
dev_get_drvdata(const struct device *dev)
{
	return (dev->driver_data);
}

static inline void
dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

static inline int
dev_to_node(const struct device *dev)
{
	(void) dev;
	return (-1);
}

#define	set_dev_node(dev, node)		do { (void)(dev); } while (0)
#define	dev_is_pci(dev)			((dev) != NULL && (dev)->bus == &pci_bus_type)
#define	dev_driver_string(dev)						\
	((dev)->driver != NULL ? (dev)->driver->name : "")
#define	device_lock(dev)		do { (void)(dev); } while (0)
#define	device_unlock(dev)		do { (void)(dev); } while (0)
#define	device_lock_assert(dev)		do { (void)(dev); } while (0)
#define	device_is_registered(dev)	((dev)->kobj.state_in_sysfs)
#define	device_enable_async_suspend(dev) do { (void)(dev); } while (0)
#define	device_set_pm_not_required(dev)	do { (void)(dev); } while (0)
#define	device_link_add(c, s, f)	((void *)NULL)
#define	device_link_del(l)		do { (void)(l); } while (0)
#define	dev_pm_set_driver_flags(dev, f)	do { (void)(dev); } while (0)
#define	device_create_file(dev, a)	((void)(dev), (void)(a), 0)
#define	device_remove_file(dev, a)	do { (void)(dev); (void)(a); } while (0)
#define	device_create_bin_file(dev, a)	((void)(dev), (void)(a), 0)
#define	device_remove_bin_file(dev, a)	do { (void)(dev); (void)(a); } while (0)
#define	device_add_groups(dev, g)	((void)(dev), (void)(g), 0)
#define	device_remove_groups(dev, g)	do { (void)(dev); (void)(g); } while (0)
#define	device_add_group(dev, g)	((void)(dev), (void)(g), 0)
#define	device_remove_group(dev, g)	do { (void)(dev); (void)(g); } while (0)
#define	devm_device_add_group(dev, g)	((void)(dev), (void)(g), 0)
#define	sysfs_dev_char_kobj		((struct kobject *)NULL)

struct device_attribute {
	struct attribute	attr;
	ssize_t			(*show)(struct device *dev,
				    struct device_attribute *attr, char *buf);
	ssize_t			(*store)(struct device *dev,
				    struct device_attribute *attr, const char *buf,
				    size_t count);
};

#define	DEVICE_ATTR(_name, _mode, _show, _store)			\
	struct device_attribute dev_attr_##_name =			\
	    __ATTR(_name, _mode, _show, _store)
#define	DEVICE_ATTR_RO(_name)						\
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define	DEVICE_ATTR_RW(_name)						\
	struct device_attribute dev_attr_##_name = __ATTR_RW(_name)
#define	DEVICE_ATTR_WO(_name)						\
	struct device_attribute dev_attr_##_name = __ATTR_WO(_name)
#define	DEVICE_ATTR_ADMIN_RO(_name)					\
	struct device_attribute dev_attr_##_name = __ATTR_RO_MODE(_name, 0400)
#define	DEVICE_ATTR_ADMIN_RW(_name)					\
	struct device_attribute dev_attr_##_name = __ATTR_RW_MODE(_name, 0600)

/* Classes and class devices (no /sys/class; device_create allocates). */
struct class *class_create(const char *name);
void class_destroy(const struct class *cls);
int class_register(const struct class *cls);
void class_unregister(const struct class *cls);
struct device *device_create(const struct class *cls, struct device *parent,
    dev_t devt, void *drvdata, const char *fmt, ...) __printf(5, 6);
void device_destroy(const struct class *cls, dev_t devt);
#define	class_create_file(c, a)		((void)(c), (void)(a), 0)
#define	class_remove_file(c, a)		do { (void)(c); (void)(a); } while (0)

int bus_register(const struct bus_type *bus);
void bus_unregister(const struct bus_type *bus);
int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);

/* devres */
typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void *__devres_alloc(dr_release_t release, size_t size, gfp_t gfp,
    const char *name) __malloc;
#define	devres_alloc(release, size, gfp)				\
	__devres_alloc(release, size, gfp, #release)
void devres_free(void *res);
void devres_add(struct device *dev, void *res);
void *devres_find(struct device *dev, dr_release_t release,
    dr_match_t match, void *match_data);
void *devres_remove(struct device *dev, dr_release_t release,
    dr_match_t match, void *match_data);
int devres_destroy(struct device *dev, dr_release_t release,
    dr_match_t match, void *match_data);
int devres_release(struct device *dev, dr_release_t release,
    dr_match_t match, void *match_data);
/* Release every managed resource of dev, newest first. Returns count. */
int devres_release_all(struct device *dev);
void *devres_open_group(struct device *dev, void *id, gfp_t gfp);
void devres_close_group(struct device *dev, void *id);
void devres_remove_group(struct device *dev, void *id);
int devres_release_group(struct device *dev, void *id);

void *devm_kmalloc(struct device *dev, size_t size, gfp_t gfp) __malloc;
void devm_kfree(struct device *dev, const void *p);
char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp) __malloc;
char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
    __printf(3, 4) __malloc;
void *devm_kmemdup(struct device *dev, const void *src, size_t len,
    gfp_t gfp);
int devm_add_action(struct device *dev, void (*action)(void *), void *data);
void devm_remove_action(struct device *dev, void (*action)(void *),
    void *data);
void devm_release_action(struct device *dev, void (*action)(void *),
    void *data);

static inline void *
devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return (devm_kmalloc(dev, size, gfp | __GFP_ZERO));
}

static inline void *
devm_kmalloc_array(struct device *dev, size_t n, size_t size, gfp_t gfp)
{
	size_t bytes;

	if (__builtin_mul_overflow(n, size, &bytes))
		return (NULL);
	return (devm_kmalloc(dev, bytes, gfp));
}

static inline void *
devm_kcalloc(struct device *dev, size_t n, size_t size, gfp_t gfp)
{
	return (devm_kmalloc_array(dev, n, size, gfp | __GFP_ZERO));
}

static inline int
devm_add_action_or_reset(struct device *dev, void (*action)(void *),
    void *data)
{
	int r = devm_add_action(dev, action, data);

	if (r != 0)
		action(data);
	return (r);
}

extern const struct bus_type pci_bus_type;

#endif /* _LINUXKPI_LINUX_DEVICE_H_ */
