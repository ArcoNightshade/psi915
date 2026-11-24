/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Environment selection for the LinuxKPI headers.
 *
 * The pure-logic headers (types, list, kref, ...) are compiled in two
 * environments:
 *
 *   LINUXKPI_ILLUMOS_KERNEL  illumos kernel module (_KERNEL and __sun)
 *   LINUXKPI_USERLAND        hosted userland (Linux or illumos), used by the
 *                            unit-test harness in tests/
 *
 * Only this file may include environment headers on behalf of the
 * linux/ headers; everything else includes <linuxkpi/platform.h>.
 */
#ifndef _LINUXKPI_PLATFORM_H_
#define	_LINUXKPI_PLATFORM_H_

#if defined(_KERNEL) && (defined(__sun) || defined(__illumos__))

#define	LINUXKPI_ILLUMOS_KERNEL	1

#include <sys/types.h>
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/stdbool.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>

#define	lkpi_assert(x)		ASSERT(x)

#ifndef lkpi_warn
#define	lkpi_warn(msg)		cmn_err(CE_WARN, "linuxkpi: %s", (msg))
#endif

#elif !defined(_KERNEL)

#define	LINUXKPI_USERLAND	1

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#define	lkpi_assert(x)		assert(x)

/*
 * Test programs may define lkpi_warn before including any linux/ header
 * to count or capture warnings.
 */
#ifndef lkpi_warn
#include <stdio.h>
#define	lkpi_warn(msg)		fprintf(stderr, "linuxkpi: %s\n", (msg))
#endif

#else
#error "linuxkpi: unsupported environment (expected illumos _KERNEL or userland)"
#endif

#endif /* _LINUXKPI_PLATFORM_H_ */
