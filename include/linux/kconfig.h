/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Kconfig option tests. The build includes this header first (as kbuild
 * does); it pulls in drm/include/generated/autoconf.h when present.
 * IS_BUILTIN(CONFIG_X) is 1 when CONFIG_X is defined to 1, IS_MODULE when
 * CONFIG_X_MODULE is; expandable in #if and in C expressions.
 */
#ifndef _LINUXKPI_LINUX_KCONFIG_H_
#define	_LINUXKPI_LINUX_KCONFIG_H_

#if defined(__has_include)
#if __has_include(<generated/autoconf.h>)
#include <generated/autoconf.h>
#endif
#endif

#define	__lkpi_is_one_1			0,
#define	__lkpi_kc_second(a, b, ...)	b
#define	__lkpi_kc_test(y)		__lkpi_kc_second(y 1, 0)
#define	__lkpi_kc_isone(x)		__lkpi_kc_test(__lkpi_is_one_##x)
#define	__lkpi_kc_def(x)		__lkpi_kc_isone(x)

#define	IS_BUILTIN(option)		__lkpi_kc_def(option)
#define	IS_MODULE(option)		__lkpi_kc_def(option##_MODULE)
#define	IS_ENABLED(option)		(IS_BUILTIN(option) || IS_MODULE(option))
#define	IS_REACHABLE(option)		IS_ENABLED(option)
#define	__is_defined(x)			__lkpi_kc_def(x)

#endif /* _LINUXKPI_LINUX_KCONFIG_H_ */
