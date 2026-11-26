/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_CONF_H
#define	_STUB_SYS_CONF_H
#include <sys/types.h>
struct uio;
struct pollhead;
struct as;
struct cb_ops {
	int	(*cb_open)(dev_t *, int, int, cred_t *);
	int	(*cb_close)(dev_t, int, int, cred_t *);
	int	(*cb_strategy)();
	int	(*cb_print)();
	int	(*cb_dump)();
	int	(*cb_read)(dev_t, struct uio *, cred_t *);
	int	(*cb_write)();
	int	(*cb_ioctl)(dev_t, int, intptr_t, int, cred_t *, int *);
	int	(*cb_devmap)(dev_t, void *, offset_t, size_t, size_t *, uint_t);
	int	(*cb_mmap)();
	int	(*cb_segmap)(dev_t, off_t, struct as *, caddr_t *, off_t,
		    uint_t, uint_t, uint_t, cred_t *);
	int	(*cb_chpoll)(dev_t, short, int, short *, struct pollhead **);
	int	(*cb_prop_op)();
	void	*cb_str;
	int	cb_flag;
	int	cb_rev;
	int	(*cb_aread)();
	int	(*cb_awrite)();
};
#define	D_NEW		0x00
#define	D_MP		0x20
#define	D_DEVMAP	0x100
#define	D_64BIT		0x200
#define	CB_REV		1
#endif
