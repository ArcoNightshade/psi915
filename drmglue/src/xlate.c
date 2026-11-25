/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * ioctl command, errno and poll-bit translation; see <drmglue/xlate.h>.
 * Written from the documented encodings (illumos <sys/ioccom.h>, Linux
 * asm-generic/ioctl.h semantics, eventpoll and poll(2) bit values).
 */

#include <linux/kernel.h>
#include <drmglue/xlate.h>

int
lkpi_ioc_translate(uint32_t ucmd, enum lkpi_ioc_abi abi, uint32_t kcmd,
    uint32_t *lcmd)
{
	uint32_t type, nr, esize, size, dir, inout;

	if (abi == LKPI_IOC_ABI_LINUX) {
		*lcmd = ucmd;
		return (0);
	}

	if ((ucmd & LKPI_SUN_IOC_RESERVED) != 0)
		return (-ENOTTY);
	inout = ucmd & (LKPI_SUN_IOC_VOID | LKPI_SUN_IOC_IN | LKPI_SUN_IOC_OUT);
	type = (ucmd >> 8) & 0xffU;
	nr = ucmd & 0xffU;
	esize = (ucmd >> 16) & LKPI_SUN_IOCPARM_MASK;

	switch (inout) {
	case LKPI_SUN_IOC_VOID:
	case 0:		/* _IO without IOC_VOID is not produced, but harmless */
		if (esize != 0)
			return (-ENOTTY);
		dir = LKPI_IOC_NONE;
		break;
	case LKPI_SUN_IOC_IN:
		dir = LKPI_IOC_WRITE;
		break;
	case LKPI_SUN_IOC_OUT:
		dir = LKPI_IOC_READ;
		break;
	case LKPI_SUN_IOC_IN | LKPI_SUN_IOC_OUT:
		dir = LKPI_IOC_READ | LKPI_IOC_WRITE;
		break;
	default:	/* VOID combined with a data direction */
		return (-ENOTTY);
	}

	size = esize;
	if (kcmd != 0 && LKPI_IOC_TYPE(kcmd) == type &&
	    LKPI_IOC_NR(kcmd) == nr) {
		uint32_t ksize = LKPI_IOC_SIZE(kcmd);

		if (ksize > LKPI_SUN_IOCPARM_MASK) {
			if (esize != (ksize & LKPI_SUN_IOCPARM_MASK))
				return (-EINVAL);
			size = ksize;
		}
	}
	*lcmd = LKPI_IOC(dir, type, nr, size);
	return (0);
}

int
lkpi_errno_to_user(int err)
{
	if (err >= 0)
		return (0);
	err = -err;
	if (err < 512)
		return (err);
	switch (err) {
	case ERESTARTSYS:
	case ERESTARTNOINTR:
	case ERESTARTNOHAND:
	case ERESTART_RESTARTBLOCK:
		return (EINTR);
	case ENOIOCTLCMD:
		return (ENOTTY);
	case ENOTSUPP:
		return (ENOTSUP);
	case EJUKEBOX:
		return (EAGAIN);
	case EPROBE_DEFER:
		return (ENXIO);
	case EIOCBQUEUED:
	case EOPENSTALE:
	case ESERVERFAULT:
		return (EIO);
	default:
		/*
		 * Codes illumos lacks (EREMOTEIO, ENOMEDIUM, ... at 600+ in
		 * include/linux/errno.h) and the remaining NFS-internal
		 * ones. EREMOTEIO is the only one DRM uses (DP AUX); EIO
		 * is the closest illumos meaning for all of them.
		 */
		if (err >= 600)
			return (EIO);
		return (EINVAL);
	}
}

static const struct {
	__poll_t	lbit;
	short		sbit;
} lkpi_poll_map[] = {
	{ LKPI_EPOLLIN,		LKPI_SUN_POLLIN },
	{ LKPI_EPOLLPRI,	LKPI_SUN_POLLPRI },
	{ LKPI_EPOLLOUT,	LKPI_SUN_POLLOUT },
	{ LKPI_EPOLLERR,	LKPI_SUN_POLLERR },
	{ LKPI_EPOLLHUP,	LKPI_SUN_POLLHUP },
	{ LKPI_EPOLLNVAL,	LKPI_SUN_POLLNVAL },
	{ LKPI_EPOLLRDNORM,	LKPI_SUN_POLLRDNORM },
	{ LKPI_EPOLLRDBAND,	LKPI_SUN_POLLRDBAND },
	{ LKPI_EPOLLWRNORM,	LKPI_SUN_POLLWRNORM },
	{ LKPI_EPOLLWRBAND,	LKPI_SUN_POLLWRBAND },
	{ LKPI_EPOLLRDHUP,	LKPI_SUN_POLLRDHUP },
};

short
lkpi_poll_to_sun(__poll_t lev)
{
	short s = 0;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lkpi_poll_map); i++)
		if (lev & lkpi_poll_map[i].lbit)
			s |= lkpi_poll_map[i].sbit;
	return (s);
}

__poll_t
lkpi_poll_from_sun(short sev)
{
	__poll_t l = 0;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lkpi_poll_map); i++)
		if ((sev & lkpi_poll_map[i].sbit) == lkpi_poll_map[i].sbit)
			l |= lkpi_poll_map[i].lbit;
	/* illumos POLLWRNORM == POLLOUT: report both Linux bits. */
	return (l);
}
