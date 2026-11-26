/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Minimal <linux/types.h> for building Linux-uAPI-consuming userland
 * (Mesa's include/drm-uapi copies, igt, the uapi_dump user build) on illumos.
 * Only the fixed-width uAPI types; no kernel types. Untested on illumos.
 */
#ifndef _PSI915_COMPAT_LINUX_TYPES_H
#define	_PSI915_COMPAT_LINUX_TYPES_H

#if defined(__linux__)
#error "psi915 compat <linux/types.h> is for non-Linux hosts only"
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef int8_t		__s8;
typedef uint8_t		__u8;
typedef int16_t		__s16;
typedef uint16_t	__u16;
typedef int32_t		__s32;
typedef uint32_t	__u32;
typedef int64_t		__s64;
typedef uint64_t	__u64;
typedef uint16_t	__le16;
typedef uint32_t	__le32;
typedef uint64_t	__le64;
typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;
typedef size_t		__kernel_size_t;

#ifndef __user
#define	__user
#endif

#endif /* _PSI915_COMPAT_LINUX_TYPES_H */
