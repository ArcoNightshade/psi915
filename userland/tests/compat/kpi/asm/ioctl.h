/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Kernel-side view for uapi_dump: the Linux "generic" ioctl number layout
 * (x86 uses it), written from the documented ABI:
 *   bits  0..7   number
 *   bits  8..15  type (magic)
 *   bits 16..29  argument size (14 bits)
 *   bits 30..31  direction: 0 none, 1 write (user->kernel), 2 read
 * Header values produced here are the canonical Linux numbers, whatever the
 * host. The illumos encoding is computed separately in uapi_dump.c.
 */
#ifndef _PSI915_TEST_KPI_ASM_IOCTL_H
#define	_PSI915_TEST_KPI_ASM_IOCTL_H

#define	_IOC_NRBITS	8
#define	_IOC_TYPEBITS	8
#define	_IOC_SIZEBITS	14
#define	_IOC_DIRBITS	2

#define	_IOC_NRSHIFT	0
#define	_IOC_TYPESHIFT	(_IOC_NRSHIFT + _IOC_NRBITS)
#define	_IOC_SIZESHIFT	(_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define	_IOC_DIRSHIFT	(_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define	_IOC_NONE	0U
#define	_IOC_WRITE	1U
#define	_IOC_READ	2U

#define	_IOC(dir, type, nr, size)					\
	(((unsigned int)(dir) << _IOC_DIRSHIFT) |			\
	((unsigned int)(type) << _IOC_TYPESHIFT) |			\
	((unsigned int)(nr) << _IOC_NRSHIFT) |				\
	((unsigned int)(size) << _IOC_SIZESHIFT))

#define	_IO(type, nr)		_IOC(_IOC_NONE, (type), (nr), 0)
#define	_IOR(type, nr, t)	_IOC(_IOC_READ, (type), (nr), sizeof (t))
#define	_IOW(type, nr, t)	_IOC(_IOC_WRITE, (type), (nr), sizeof (t))
#define	_IOWR(type, nr, t)	_IOC(_IOC_READ | _IOC_WRITE, (type), (nr), \
				    sizeof (t))

#endif /* _PSI915_TEST_KPI_ASM_IOCTL_H */
