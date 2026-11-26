/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; type set cross-checked against FreeBSD's LinuxKPI types.h
 * (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_TYPES_H_
#define	_LINUXKPI_LINUX_TYPES_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;

typedef uint8_t		__u8;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef uint64_t	__u64;
typedef int8_t		__s8;
typedef int16_t		__s16;
typedef int32_t		__s32;
typedef int64_t		__s64;

/* Endian-annotated types; sparse-only distinction, plain integers here. */
typedef uint16_t	__le16;
typedef uint16_t	__be16;
typedef uint32_t	__le32;
typedef uint32_t	__be32;
typedef uint64_t	__le64;
typedef uint64_t	__be64;
typedef uint16_t	__sum16;
typedef uint32_t	__wsum;

typedef uint64_t	__aligned_u64 __attribute__((__aligned__(8)));

typedef unsigned int	gfp_t;
typedef unsigned int	__poll_t;
typedef uint64_t	phys_addr_t;
typedef uint64_t	dma_addr_t;
typedef uint64_t	resource_size_t;
typedef unsigned long	pgoff_t;
typedef unsigned long	irq_hw_number_t;
typedef unsigned long	kernel_ulong_t;
/* Macro over a KPI name: illumos <sys/types.h> may define loff_t too. */
typedef long long	lkpi_loff_t;
#define	loff_t		lkpi_loff_t
typedef unsigned short	umode_t;
typedef unsigned long	__kernel_size_t;
typedef long		__kernel_ssize_t;
typedef long		__kernel_ptrdiff_t;
typedef long		__kernel_long_t;
typedef unsigned long	__kernel_ulong_t;
typedef int		__kernel_pid_t;
typedef unsigned int	__kernel_uid32_t;
typedef unsigned int	__kernel_gid32_t;
typedef long long	__kernel_loff_t;
typedef long long	__kernel_time64_t;
typedef int		__kernel_clockid_t;
typedef unsigned short	__kernel_mode_t;

#if defined(LINUXKPI_ILLUMOS_KERNEL) && !defined(_PTRDIFF_T)
#define	_PTRDIFF_T
typedef __PTRDIFF_TYPE__	ptrdiff_t;
#endif

#define	BITS_PER_LONG		(__SIZEOF_LONG__ * 8)
#define	BITS_PER_LONG_LONG	(__SIZEOF_LONG_LONG__ * 8)

#define	DECLARE_BITMAP(name, bits)					\
	unsigned long name[((bits) + BITS_PER_LONG - 1) / BITS_PER_LONG]

/* Doubly linked list node/head; operations in <linux/list.h>. */
struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/* Hash list with a single-pointer head; operations in <linux/list.h>. */
struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next;
	struct hlist_node **pprev;
};

/* RCU callback head; operations in <linux/rcupdate.h>. */
struct callback_head {
	struct callback_head	*next;
	void			(*func)(struct callback_head *head);
} __attribute__((__aligned__(sizeof (void *))));
#define	rcu_head	callback_head

typedef void (*rcu_callback_t)(struct rcu_head *head);

#ifndef U64_C
#define	S8_C(x)		x
#define	U8_C(x)		x ## U
#define	S16_C(x)	x
#define	U16_C(x)	x ## U
#define	S32_C(x)	x
#define	U32_C(x)	x ## U
#define	S64_C(x)	x ## LL
#define	U64_C(x)	x ## ULL
#endif

#endif /* _LINUXKPI_LINUX_TYPES_H_ */
