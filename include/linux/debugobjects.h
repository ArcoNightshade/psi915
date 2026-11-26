/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No debug objects.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_DEBUGOBJECTS_H_
#define	_LINUXKPI_INCLUDE_LINUX_DEBUGOBJECTS_H_

struct debug_obj_descr {
	const char	*name;
};
#define	debug_object_init(a, d)		do { } while (0)
#define	debug_object_activate(a, d)	(0)
#define	debug_object_deactivate(a, d)	do { } while (0)
#define	debug_object_destroy(a, d)	do { } while (0)
#define	debug_object_free(a, d)		do { } while (0)
#define	debug_object_assert_init(a, d)	do { } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_DEBUGOBJECTS_H_ */
