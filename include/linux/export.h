/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Symbol exports: illumos modules export every global symbol, so these
 * are empty (also defined by <linux/module.h>).
 */
#ifndef _LINUXKPI_LINUX_EXPORT_H_
#define	_LINUXKPI_LINUX_EXPORT_H_

#ifndef EXPORT_SYMBOL
#define	EXPORT_SYMBOL(sym)
#define	EXPORT_SYMBOL_GPL(sym)
#define	EXPORT_SYMBOL_NS(sym, ns)
#define	EXPORT_SYMBOL_NS_GPL(sym, ns)
#endif
#ifndef EXPORT_SYMBOL_FOR_TESTS_ONLY
#define	EXPORT_SYMBOL_FOR_TESTS_ONLY(sym)
#endif
#define	EXPORT_SYMBOL_IF_KUNIT(sym)
#define	THIS_MODULE_EXPORT_NS

#endif /* _LINUXKPI_LINUX_EXPORT_H_ */
