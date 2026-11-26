/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. User and group ids.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_UIDGID_H_
#define	_LINUXKPI_INCLUDE_LINUX_UIDGID_H_

#include <linux/types.h>

typedef struct {
	unsigned int	val;
} kuid_t;
typedef struct {
	unsigned int	val;
} kgid_t;
#define	overflowuid		65534
#define	overflowgid		65534
#define	GLOBAL_ROOT_UID		((kuid_t){ 0 })
#define	GLOBAL_ROOT_GID		((kgid_t){ 0 })
#define	uid_eq(a, b)		((a).val == (b).val)
#define	from_kuid(ns, u)	((u).val)
#define	from_kuid_munged(ns, u)	((u).val)

#endif /* _LINUXKPI_INCLUDE_LINUX_UIDGID_H_ */
