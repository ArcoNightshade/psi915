/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Module metadata and entry points.
 *
 * MODULE_*() metadata is discarded (illumos modinfo shows the modlinkage
 * string). THIS_MODULE is NULL and try_module_get() always succeeds:
 * illumos keeps a module loaded while its dev_ops/cb_ops are in use, and
 * the driver glue holds dependent modules via _depends_on.
 *
 * module_init(fn)/module_exit(fn) define global wrappers
 * lkpi_initcall_<fn>() and lkpi_exitcall_<fn>(). A kmod may contain
 * several Linux modules (e.g. drm and drm_kms_helper), so the kmod's
 * illumos glue (_init/_fini, see <linuxkpi/ddi.h> once available) names
 * the calls to make, in order, instead of relying on a linker section.
 */
#ifndef _LINUXKPI_LINUX_MODULE_H_
#define	_LINUXKPI_LINUX_MODULE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/stringify.h>
#include <linux/kernel.h>

struct module {
	const char	*name;
};

#define	THIS_MODULE		((struct module *)NULL)
#define	KBUILD_MODNAME_DEFAULT	"linuxkpi"
#ifndef KBUILD_MODNAME
#define	KBUILD_MODNAME		KBUILD_MODNAME_DEFAULT
#endif

#define	MODULE_LICENSE(s)
#define	MODULE_AUTHOR(s)
#define	MODULE_DESCRIPTION(s)
#define	MODULE_VERSION(s)
#define	MODULE_ALIAS(s)
#define	MODULE_FIRMWARE(s)
#define	MODULE_SOFTDEP(s)
#define	MODULE_IMPORT_NS(ns)
#define	MODULE_INFO(tag, info)
#define	MODULE_DEVICE_TABLE(type, name)
#define	EXPORT_SYMBOL(sym)
#define	EXPORT_SYMBOL_GPL(sym)
#define	EXPORT_SYMBOL_NS(sym, ns)
#define	EXPORT_SYMBOL_NS_GPL(sym, ns)
#define	EXPORT_SYMBOL_FOR_TESTS_ONLY(sym)

#define	module_init(fn)							\
	int lkpi_initcall_##fn(void);					\
	int lkpi_initcall_##fn(void) { return (fn()); }
#define	module_exit(fn)							\
	void lkpi_exitcall_##fn(void);					\
	void lkpi_exitcall_##fn(void) { fn(); }
/*
 * Every initcall level maps to the same wrapper: the module linkage
 * file calls lkpi_initcall_<fn>() explicitly, in the order it needs
 * (docs/11). __exitcall(fn) likewise defines lkpi_exitcall_<fn>().
 */
#define	core_initcall(fn)	module_init(fn)
#define	postcore_initcall(fn)	module_init(fn)
#define	arch_initcall(fn)	module_init(fn)
#define	fs_initcall(fn)		module_init(fn)
#define	rootfs_initcall(fn)	module_init(fn)
#define	__exitcall(fn)		module_exit(fn)
#define	late_initcall(fn)	module_init(fn)
#define	subsys_initcall(fn)	module_init(fn)
#define	device_initcall(fn)	module_init(fn)

static inline bool
try_module_get(struct module *m)
{
	(void) m;
	return (true);
}

#define	module_put(m)		do { (void)(m); } while (0)
#define	__module_get(m)		do { (void)(m); } while (0)
#define	module_name(m)		("linuxkpi")
#define	request_module(...)	(-ENOENT)
#define	symbol_get(x)		(&(x))
#define	symbol_put(x)		do { } while (0)
#define	module_is_live(m)	(true)

#endif /* _LINUXKPI_LINUX_MODULE_H_ */
