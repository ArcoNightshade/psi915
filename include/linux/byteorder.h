/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Byte order (amd64 is little-endian). Not named asm/byteorder.h:
 * illumos installs that header (docs/03).
 */
#ifndef _LINUXKPI_LINUX_BYTEORDER_H_
#define	_LINUXKPI_LINUX_BYTEORDER_H_

#include <linux/types.h>

#define	__LITTLE_ENDIAN		1234
#define	__LITTLE_ENDIAN_BITFIELD
#define	cpu_to_le16(x)		((__le16)(u16)(x))
#define	cpu_to_le32(x)		((__le32)(u32)(x))
#define	cpu_to_le64(x)		((__le64)(u64)(x))
#define	le16_to_cpu(x)		((u16)(__le16)(x))
#define	le32_to_cpu(x)		((u32)(__le32)(x))
#define	le64_to_cpu(x)		((u64)(__le64)(x))
#define	cpu_to_be16(x)		((__be16)__builtin_bswap16((u16)(x)))
#define	cpu_to_be32(x)		((__be32)__builtin_bswap32((u32)(x)))
#define	cpu_to_be64(x)		((__be64)__builtin_bswap64((u64)(x)))
#define	be16_to_cpu(x)		__builtin_bswap16((u16)(__be16)(x))
#define	be32_to_cpu(x)		__builtin_bswap32((u32)(__be32)(x))
#define	be64_to_cpu(x)		__builtin_bswap64((u64)(__be64)(x))
#define	swab16(x)		__builtin_bswap16(x)
#define	swab32(x)		__builtin_bswap32(x)
#define	swab64(x)		__builtin_bswap64(x)
#define	le16_to_cpup(p)		le16_to_cpu(*(p))
#define	le32_to_cpup(p)		le32_to_cpu(*(p))
#define	le64_to_cpup(p)		le64_to_cpu(*(p))
#define	cpu_to_le32s(p)		do { (void)(p); } while (0)
#define	le32_to_cpus(p)		do { (void)(p); } while (0)
#define	le16_add_cpu(p, v)	(*(p) = cpu_to_le16(le16_to_cpu(*(p)) + (v)))
#define	le32_add_cpu(p, v)	(*(p) = cpu_to_le32(le32_to_cpu(*(p)) + (v)))

#endif /* _LINUXKPI_LINUX_BYTEORDER_H_ */
