/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Dynamic debug is not supported: class maps are discarded and
 * dynamic debug prints compile to nothing.
 */
#ifndef _LINUXKPI_LINUX_DYNAMIC_DEBUG_H_
#define	_LINUXKPI_LINUX_DYNAMIC_DEBUG_H_

#define	DECLARE_DYNDBG_CLASSMAP(...)
#define	DYNDBG_CLASSMAP_DEFINE(...)
#define	DYNDBG_CLASSMAP_USE(...)
#define	DEFINE_DYNAMIC_DEBUG_METADATA(name, fmt)
#define	DYNAMIC_DEBUG_BRANCH(desc)	(0)
#define	dynamic_pr_debug(fmt, ...)	do { } while (0)
#define	dynamic_dev_dbg(dev, fmt, ...)	do { (void)(dev); } while (0)
#define	_dynamic_func_call_cls(cls, fmt, func, ...) do { } while (0)
#define	_dynamic_func_call_cls_no_desc(cls, fmt, func, ...) do { } while (0)
#define	__dynamic_func_call_cls(id, cls, fmt, func, ...) do { } while (0)
enum class_map_type {
	DD_CLASS_TYPE_DISJOINT_BITS,
	DD_CLASS_TYPE_LEVEL_NUM,
	DD_CLASS_TYPE_DISJOINT_NAMES,
	DD_CLASS_TYPE_LEVEL_NAMES
};

#endif /* _LINUXKPI_LINUX_DYNAMIC_DEBUG_H_ */
