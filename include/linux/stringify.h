/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room.
 */
#ifndef _LINUXKPI_LINUX_STRINGIFY_H_
#define	_LINUXKPI_LINUX_STRINGIFY_H_

#define	__stringify_1(x...)	#x
#define	__stringify(x...)	__stringify_1(x)
#define	FILE_LINE		__FILE__ ":" __stringify(__LINE__)

#endif /* _LINUXKPI_LINUX_STRINGIFY_H_ */
