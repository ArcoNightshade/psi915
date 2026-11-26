/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_TYPES_H
#define	_STUB_SYS_TYPES_H

#include <sys/ccompile.h>

typedef signed char		int8_t;
typedef short			int16_t;
typedef int			int32_t;
typedef long			int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long		uint64_t;
typedef long			intptr_t;
typedef unsigned long		uintptr_t;
typedef unsigned long		size_t;
typedef long			ssize_t;
typedef long			off_t;
typedef unsigned char		uchar_t;
typedef unsigned short		ushort_t;
typedef unsigned int		uint_t;
typedef unsigned long		ulong_t;
typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef enum { B_FALSE = 0, B_TRUE = 1 } boolean_t;
typedef long			clock_t;
typedef char			*caddr_t;
typedef short			pri_t;
typedef unsigned long		uintmax_t;
typedef long			intmax_t;
typedef long long		hrtime_t;
typedef int			pid_t;
typedef unsigned long		dev_t;
typedef unsigned long		pfn_t;
typedef long long		offset_t;
typedef unsigned long long	paddr_t;

#ifndef NULL
#define	NULL	((void *)0)
#endif

#endif
