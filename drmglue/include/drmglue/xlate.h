/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Pure translation helpers between the illumos user ABI and the Linux
 * kernel ABI the imported DRM code expects (docs/10-mmap-dmabuf-design.md,
 * section 2):
 *
 *   - ioctl command words: illumos <sys/ioccom.h> encoding (what libdrm and
 *     Mesa's drm-uapi headers produce when built on illumos) or the Linux
 *     asm-generic _IOC encoding, decoded and re-encoded as a Linux command
 *     for file_operations.unlocked_ioctl/compat_ioctl;
 *   - Linux-internal errno values (ERESTARTSYS, ENOTSUPP, ...) to illumos
 *     errno values for userland;
 *   - Linux EPOLL* bits to illumos POLL* bits and back.
 *
 * No environment dependencies beyond <linux/types.h>; unit-tested in
 * drmglue/tests/test_xlate.c.
 */
#ifndef _DRMGLUE_XLATE_H_
#define	_DRMGLUE_XLATE_H_

#include <linux/types.h>
#include <linux/errno.h>

/*
 * Linux asm-generic ioctl layout (x86 uses it unchanged):
 *   bits  0- 7 nr, 8-15 type, 16-29 size (14 bits), 30-31 direction.
 * Direction bits are from the user's point of view: WRITE means the
 * kernel copies in, READ means the kernel copies out.
 */
#define	LKPI_IOC_NRBITS		8
#define	LKPI_IOC_TYPEBITS	8
#define	LKPI_IOC_SIZEBITS	14
#define	LKPI_IOC_NRSHIFT	0
#define	LKPI_IOC_TYPESHIFT	8
#define	LKPI_IOC_SIZESHIFT	16
#define	LKPI_IOC_DIRSHIFT	30
#define	LKPI_IOC_NONE		0U
#define	LKPI_IOC_WRITE		1U
#define	LKPI_IOC_READ		2U
#define	LKPI_IOC_SIZEMASK	((1U << LKPI_IOC_SIZEBITS) - 1)

#define	LKPI_IOC(dir, type, nr, size)					\
	((uint32_t)(((uint32_t)(dir) << LKPI_IOC_DIRSHIFT) |		\
	(((uint32_t)(size) & LKPI_IOC_SIZEMASK) << LKPI_IOC_SIZESHIFT) |	\
	(((uint32_t)(type) & 0xffU) << LKPI_IOC_TYPESHIFT) |		\
	(((uint32_t)(nr) & 0xffU) << LKPI_IOC_NRSHIFT)))
#define	LKPI_IOC_DIR(c)		(((uint32_t)(c) >> LKPI_IOC_DIRSHIFT) & 3U)
#define	LKPI_IOC_TYPE(c)	(((uint32_t)(c) >> LKPI_IOC_TYPESHIFT) & 0xffU)
#define	LKPI_IOC_NR(c)		(((uint32_t)(c) >> LKPI_IOC_NRSHIFT) & 0xffU)
#define	LKPI_IOC_SIZE(c)	(((uint32_t)(c) >> LKPI_IOC_SIZESHIFT) & \
				    LKPI_IOC_SIZEMASK)

/*
 * illumos <sys/ioccom.h> layout (BSD heritage, but with an 8-bit size):
 *   IOC_VOID 0x20000000 (no argument), IOC_OUT 0x40000000 (kernel copies
 *   out), IOC_IN 0x80000000 (kernel copies in), size = sizeof & 0xff in
 *   bits 16-23, bits 24-28 always zero.
 * Note the direction bits are swapped relative to Linux: an illumos _IOR
 * has the bit pattern of a Linux _IOW and vice versa, so the encoding of an
 * incoming command cannot be inferred from its bits; it is configured
 * (enum lkpi_ioc_abi). Sizes of 256 bytes and more are truncated by the
 * illumos macros, so for those the kernel's own definition is authoritative.
 */
#define	LKPI_SUN_IOC_VOID	0x20000000U
#define	LKPI_SUN_IOC_OUT	0x40000000U
#define	LKPI_SUN_IOC_IN		0x80000000U
#define	LKPI_SUN_IOCPARM_MASK	0xffU
#define	LKPI_SUN_IOC_RESERVED	0x1f000000U	/* must be zero */

#define	LKPI_SUN_IOC(inout, type, nr, size)				\
	((uint32_t)((inout) | (((uint32_t)(size) & LKPI_SUN_IOCPARM_MASK) \
	<< 16) | (((uint32_t)(type) & 0xffU) << 8) | ((uint32_t)(nr) & 0xffU)))

enum lkpi_ioc_abi {
	LKPI_IOC_ABI_ILLUMOS = 0,	/* <sys/ioccom.h>; the default */
	LKPI_IOC_ABI_LINUX = 1		/* asm-generic _IOC */
};

/*
 * Translate a user command word to the Linux command handed to the driver.
 *
 *   ucmd  the command word from ioctl(2)
 *   abi   encoding of ucmd
 *   kcmd  the kernel's own (Linux-encoded) definition of the ioctl with the
 *         same type and nr, or 0 if unknown; used for the size of an illumos
 *         command whose real size does not fit the 8-bit field
 *
 * Returns 0 and the Linux command in *lcmd, or a negative Linux errno:
 *   -ENOTTY  malformed command word (reserved bits, impossible direction)
 *   -EINVAL  illumos size field inconsistent with a >= 256 byte kcmd
 *
 * Size rules for the illumos ABI (Linux drm_ioctl accepts user structs that
 * are smaller or larger than the kernel's and copies min/zero-fills, so the
 * user size must be preserved whenever it is known exactly):
 *   kcmd unknown or its size < 256: size = encoded size (exact)
 *   kcmd size >= 256: the encoded size must equal kcmd size & 0xff, and the
 *         kcmd size is used (user and kernel struct sizes then must agree
 *         modulo 256; a mismatch is rejected rather than guessed)
 */
int lkpi_ioc_translate(uint32_t ucmd, enum lkpi_ioc_abi abi, uint32_t kcmd,
    uint32_t *lcmd);

/*
 * Map a Linux return value (negative errno) to a positive illumos errno for
 * userland. Kernel-internal codes: ERESTART* -> EINTR (libdrm's drmIoctl()
 * retries on EINTR, which matches Linux's transparent restart closely
 * enough), ENOIOCTLCMD -> ENOTTY, ENOTSUPP -> ENOTSUP, EJUKEBOX -> EAGAIN,
 * other Linux-only codes -> EINVAL/EIO. Standard names already carry the
 * environment's values (include/linux/errno.h) and pass through.
 * err must be negative or zero; zero returns zero.
 */
int lkpi_errno_to_user(int err);

/*
 * Linux EPOLL* event bits (uapi/linux/eventpoll.h values; these are ABI
 * constants, identical on all Linux architectures we care about).
 */
#define	LKPI_EPOLLIN		0x0001U
#define	LKPI_EPOLLPRI		0x0002U
#define	LKPI_EPOLLOUT		0x0004U
#define	LKPI_EPOLLERR		0x0008U
#define	LKPI_EPOLLHUP		0x0010U
#define	LKPI_EPOLLNVAL		0x0020U
#define	LKPI_EPOLLRDNORM	0x0040U
#define	LKPI_EPOLLRDBAND	0x0080U
#define	LKPI_EPOLLWRNORM	0x0100U
#define	LKPI_EPOLLWRBAND	0x0200U
#define	LKPI_EPOLLMSG		0x0400U
#define	LKPI_EPOLLRDHUP		0x2000U

/*
 * illumos <sys/poll.h> values (ABI constants of illumos).
 */
#define	LKPI_SUN_POLLIN		0x0001
#define	LKPI_SUN_POLLPRI	0x0002
#define	LKPI_SUN_POLLOUT	0x0004
#define	LKPI_SUN_POLLRDNORM	0x0040
#define	LKPI_SUN_POLLWRNORM	LKPI_SUN_POLLOUT
#define	LKPI_SUN_POLLRDBAND	0x0080
#define	LKPI_SUN_POLLWRBAND	0x0100
#define	LKPI_SUN_POLLERR	0x0008
#define	LKPI_SUN_POLLHUP	0x0010
#define	LKPI_SUN_POLLNVAL	0x0020
#define	LKPI_SUN_POLLRDHUP	0x4000

short lkpi_poll_to_sun(__poll_t lev);
__poll_t lkpi_poll_from_sun(short sev);

#endif /* _DRMGLUE_XLATE_H_ */
