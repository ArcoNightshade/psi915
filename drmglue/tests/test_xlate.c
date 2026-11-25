/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <drmglue/xlate.h>: illumos and Linux ioctl encodings,
 * size rules for >= 256 byte structures, errno and poll-bit mapping.
 */

#include <linux/kernel.h>
#include <drmglue/xlate.h>

#include "harness.h"

/* illumos <sys/ioccom.h> macros, restated from the header's definition. */
#define	S_IO(x, y)		LKPI_SUN_IOC(LKPI_SUN_IOC_VOID, x, y, 0)
#define	S_IOR(x, y, n)		LKPI_SUN_IOC(LKPI_SUN_IOC_OUT, x, y, n)
#define	S_IOW(x, y, n)		LKPI_SUN_IOC(LKPI_SUN_IOC_IN, x, y, n)
#define	S_IOWR(x, y, n)		LKPI_SUN_IOC(LKPI_SUN_IOC_IN | \
				    LKPI_SUN_IOC_OUT, x, y, n)
#define	L_IOR(x, y, n)		LKPI_IOC(LKPI_IOC_READ, x, y, n)
#define	L_IOW(x, y, n)		LKPI_IOC(LKPI_IOC_WRITE, x, y, n)
#define	L_IOWR(x, y, n)		LKPI_IOC(LKPI_IOC_READ | LKPI_IOC_WRITE, x, y, n)

TEST(linux_layout)
{
	/* DRM_IOCTL_VERSION on Linux x86-64: _IOWR('d', 0x00, 64 bytes). */
	CHECK_EQ(L_IOWR('d', 0, 64), 0xc0406400U);
	/* DRM_IOCTL_GEM_CLOSE: _IOW('d', 0x09, 8). */
	CHECK_EQ(L_IOW('d', 9, 8), 0x40086409U);
	CHECK_EQ(LKPI_IOC_NR(0xc0406400U), 0);
	CHECK_EQ(LKPI_IOC_TYPE(0xc0406400U), 'd');
	CHECK_EQ(LKPI_IOC_SIZE(0xc0406400U), 64);
	CHECK_EQ(LKPI_IOC_DIR(0xc0406400U), 3);
}

TEST(illumos_to_linux)
{
	uint32_t l;

	/* The direction bits swap: illumos _IOR == Linux _IOR semantics. */
	CHECK_EQ(lkpi_ioc_translate(S_IOR('d', 0x2a, 16),
	    LKPI_IOC_ABI_ILLUMOS, 0, &l), 0);
	CHECK_EQ(l, L_IOR('d', 0x2a, 16));
	CHECK_EQ(lkpi_ioc_translate(S_IOW('d', 0x09, 8),
	    LKPI_IOC_ABI_ILLUMOS, 0, &l), 0);
	CHECK_EQ(l, L_IOW('d', 9, 8));
	CHECK_EQ(lkpi_ioc_translate(S_IOWR('d', 0x40, 64),
	    LKPI_IOC_ABI_ILLUMOS, 0, &l), 0);
	CHECK_EQ(l, L_IOWR('d', 0x40, 64));
	CHECK_EQ(lkpi_ioc_translate(S_IO('d', 0x1e), LKPI_IOC_ABI_ILLUMOS, 0,
	    &l), 0);
	CHECK_EQ(l, LKPI_IOC(LKPI_IOC_NONE, 'd', 0x1e, 0));
	/* Bit patterns collide across ABIs: same word, opposite meaning. */
	CHECK_EQ(S_IOR('d', 1, 8), L_IOW('d', 1, 8));
}

TEST(illumos_sizes)
{
	uint32_t l;

	/* User struct smaller than kernel's (old userland): exact size kept. */
	CHECK_EQ(lkpi_ioc_translate(S_IOWR('d', 0x40, 48),
	    LKPI_IOC_ABI_ILLUMOS, L_IOWR('d', 0x40, 64), &l), 0);
	CHECK_EQ(LKPI_IOC_SIZE(l), 48);
	/* Kernel struct of 264 bytes: illumos encodes 264 & 0xff = 8. */
	CHECK_EQ(lkpi_ioc_translate(S_IOWR('d', 0x41, 264),
	    LKPI_IOC_ABI_ILLUMOS, L_IOWR('d', 0x41, 264), &l), 0);
	CHECK_EQ(LKPI_IOC_SIZE(l), 264);
	/* Inconsistent low byte for a large kernel struct: rejected. */
	CHECK_EQ(lkpi_ioc_translate(S_IOWR('d', 0x41, 260),
	    LKPI_IOC_ABI_ILLUMOS, L_IOWR('d', 0x41, 264), &l), -EINVAL);
	/* kcmd for another nr is ignored. */
	CHECK_EQ(lkpi_ioc_translate(S_IOWR('d', 0x42, 8),
	    LKPI_IOC_ABI_ILLUMOS, L_IOWR('d', 0x41, 264), &l), 0);
	CHECK_EQ(LKPI_IOC_SIZE(l), 8);
}

TEST(malformed)
{
	uint32_t l;

	CHECK_EQ(lkpi_ioc_translate(0x01000000U | S_IOW('d', 1, 8),
	    LKPI_IOC_ABI_ILLUMOS, 0, &l), -ENOTTY);
	CHECK_EQ(lkpi_ioc_translate(LKPI_SUN_IOC_VOID | LKPI_SUN_IOC_IN |
	    ('d' << 8), LKPI_IOC_ABI_ILLUMOS, 0, &l), -ENOTTY);
	CHECK_EQ(lkpi_ioc_translate(LKPI_SUN_IOC_VOID | (4 << 16) | ('d' << 8),
	    LKPI_IOC_ABI_ILLUMOS, 0, &l), -ENOTTY);
	/* Linux ABI passes through untouched. */
	CHECK_EQ(lkpi_ioc_translate(0xdeadbeefU, LKPI_IOC_ABI_LINUX, 0, &l), 0);
	CHECK_EQ(l, 0xdeadbeefU);
}

TEST(errno_map)
{
	CHECK_EQ(lkpi_errno_to_user(0), 0);
	CHECK_EQ(lkpi_errno_to_user(-EINVAL), EINVAL);
	CHECK_EQ(lkpi_errno_to_user(-ENOMEM), ENOMEM);
	CHECK_EQ(lkpi_errno_to_user(-ERESTARTSYS), EINTR);
	CHECK_EQ(lkpi_errno_to_user(-ERESTARTNOINTR), EINTR);
	CHECK_EQ(lkpi_errno_to_user(-ENOIOCTLCMD), ENOTTY);
	CHECK_EQ(lkpi_errno_to_user(-ENOTSUPP), ENOTSUP);
	CHECK_EQ(lkpi_errno_to_user(-EJUKEBOX), EAGAIN);
	CHECK_EQ(lkpi_errno_to_user(-EBADTYPE), EINVAL);
	CHECK(lkpi_errno_to_user(-EREMOTEIO) < 512);
}

TEST(poll_bits)
{
	CHECK_EQ(lkpi_poll_to_sun(LKPI_EPOLLIN | LKPI_EPOLLRDNORM),
	    LKPI_SUN_POLLIN | LKPI_SUN_POLLRDNORM);
	CHECK_EQ(lkpi_poll_to_sun(LKPI_EPOLLWRBAND), LKPI_SUN_POLLWRBAND);
	CHECK_EQ(lkpi_poll_to_sun(LKPI_EPOLLRDHUP), LKPI_SUN_POLLRDHUP);
	CHECK_EQ(lkpi_poll_to_sun(LKPI_EPOLLMSG), 0);
	CHECK_EQ(lkpi_poll_from_sun(LKPI_SUN_POLLIN), LKPI_EPOLLIN);
	CHECK_EQ(lkpi_poll_from_sun(LKPI_SUN_POLLOUT),
	    LKPI_EPOLLOUT | LKPI_EPOLLWRNORM);
	CHECK_EQ(lkpi_poll_from_sun(LKPI_SUN_POLLWRBAND), LKPI_EPOLLWRBAND);
}

LKPI_SUITE(drmglue_xlate)
{
	RUN(linux_layout);
	RUN(illumos_to_linux);
	RUN(illumos_sizes);
	RUN(malformed);
	RUN(errno_map);
	RUN(poll_bits);
}
