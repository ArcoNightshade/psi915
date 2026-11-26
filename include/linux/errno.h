/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; cross-checked against FreeBSD's LinuxKPI errno.h
 * (BSD-2-Clause).
 *
 * Standard errno names keep the environment's values (illumos
 * <sys/errno.h> in the kernel, which differ from Linux for many codes,
 * e.g. EDEADLK, ETIMEDOUT), so values returned to illumos userland are
 * native. Kernel-internal Linux codes get the Linux values (512 and up),
 * above every illumos errno; they must never leave the kernel (the ioctl
 * layer maps ERESTARTSYS to EINTR, ENOTSUPP and ENOIOCTLCMD to ENOTSUP or
 * ENOTTY). Codes Linux has and illumos lacks get values from 600 up.
 */
/*
 * On a Linux userland host this file shadows the kernel UAPI header of the
 * same name, and glibc <errno.h> includes <linux/errno.h> for the errno values; chain to the host header first.
 */
#if !defined(_KERNEL) && defined(__linux__)
#include_next <linux/errno.h>
#endif

#ifndef _LINUXKPI_LINUX_ERRNO_H_
#define	_LINUXKPI_LINUX_ERRNO_H_

#include <linuxkpi/platform.h>

#define	ERESTARTSYS		512
#define	ERESTARTNOINTR		513
#define	ERESTARTNOHAND		514
#define	ENOIOCTLCMD		515
#define	ERESTART_RESTARTBLOCK	516
#define	EPROBE_DEFER		517
#define	EOPENSTALE		518
#define	ENOPARAM		519
#define	EBADHANDLE		521
#define	ENOTSYNC		522
#define	EBADCOOKIE		523
#define	ENOTSUPP		524
#define	ETOOSMALL		525
#define	ESERVERFAULT		526
#define	EBADTYPE		527
#define	EJUKEBOX		528
#define	EIOCBQUEUED		529
#define	ERECALLCONFLICT		530

#ifndef EREMOTEIO
#define	EREMOTEIO		600
#endif
#ifndef ENOMEDIUM
#define	ENOMEDIUM		601
#endif
#ifndef EMEDIUMTYPE
#define	EMEDIUMTYPE		602
#endif
#ifndef EKEYEXPIRED
#define	EKEYEXPIRED		603
#endif
#ifndef EHWPOISON
#define	EHWPOISON		604
#endif
#ifndef EBADE
#define	EBADE			605
#endif
#ifndef EMULTIHOP
#define	EMULTIHOP		607
#endif
#ifndef ESTALE
#define	ESTALE			608
#endif
#ifndef ENOBUFS
#define	ENOBUFS			609
#endif
#ifndef ENOKEY
#define	ENOKEY			606
#endif

#endif /* _LINUXKPI_LINUX_ERRNO_H_ */
