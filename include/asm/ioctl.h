/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Linux ioctl command encoding (x86, asm-generic layout): dir << 30 |
 * size << 16 (14 bits) | type << 8 | nr, dir 1 = write (to the kernel),
 * 2 = read. Kernel drivers see Linux-encoded commands; the DRM glue
 * translates illumos <sys/ioccom.h> commands from userland (docs/09 5.1,
 * docs/10 2.4). illumos installs no <asm/ioctl.h>.
 */
#ifndef _LINUXKPI_ASM_IOCTL_H_
#define	_LINUXKPI_ASM_IOCTL_H_

#define	_IOC_NRBITS	8
#define	_IOC_TYPEBITS	8
#define	_IOC_SIZEBITS	14
#define	_IOC_DIRBITS	2
#define	_IOC_NRMASK	((1U << _IOC_NRBITS) - 1)
#define	_IOC_TYPEMASK	((1U << _IOC_TYPEBITS) - 1)
#define	_IOC_SIZEMASK	((1U << _IOC_SIZEBITS) - 1)
#define	_IOC_DIRMASK	((1U << _IOC_DIRBITS) - 1)
#define	_IOC_NRSHIFT	0
#define	_IOC_TYPESHIFT	(_IOC_NRSHIFT + _IOC_NRBITS)
#define	_IOC_SIZESHIFT	(_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define	_IOC_DIRSHIFT	(_IOC_SIZESHIFT + _IOC_SIZEBITS)
#define	_IOC_NONE	0U
#define	_IOC_WRITE	1U
#define	_IOC_READ	2U

#define	_IOC(dir, type, nr, size)					\
	(((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) |	\
	    ((nr) << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))
#define	_IOC_TYPECHECK(t)	(sizeof (t))
/*
 * illumos <sys/ioccom.h> defines these with its own encoding; the KPI
 * uses Linux's (drmglue translates illumos ioctl numbers).
 */
#undef	_IO
#undef	_IOR
#undef	_IOW
#undef	_IOWR
#undef	IOC_IN
#undef	IOC_OUT
#undef	IOC_INOUT
#define	_IO(type, nr)		_IOC(_IOC_NONE, (type), (nr), 0)
#define	_IOR(type, nr, t)	_IOC(_IOC_READ, (type), (nr), _IOC_TYPECHECK(t))
#define	_IOW(type, nr, t)	_IOC(_IOC_WRITE, (type), (nr), _IOC_TYPECHECK(t))
#define	_IOWR(type, nr, t)						\
	_IOC(_IOC_READ | _IOC_WRITE, (type), (nr), _IOC_TYPECHECK(t))
#define	IOC_IN		(_IOC_WRITE << _IOC_DIRSHIFT)
#define	IOC_OUT		(_IOC_READ << _IOC_DIRSHIFT)
#define	IOC_INOUT	((_IOC_WRITE | _IOC_READ) << _IOC_DIRSHIFT)
#define	IOCSIZE_MASK	(_IOC_SIZEMASK << _IOC_SIZESHIFT)
#define	IOCSIZE_SHIFT	(_IOC_SIZESHIFT)
#define	_IOC_DIR(nr)		(((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define	_IOC_TYPE(nr)		(((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define	_IOC_NR(nr)		(((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define	_IOC_SIZE(nr)		(((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

#endif /* _LINUXKPI_ASM_IOCTL_H_ */
