/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * /dev/dri node registry and clone opens (src/cdev.c) and the ioctl, read
 * and poll dispatchers (src/file.c) with a fake DRM-like driver: clone id
 * allocation, release timing, open failure, ioctl translation incl. the
 * 32-bit and kernel-caller paths and errno mapping, O_NONBLOCK on reads,
 * and poll forwarding from a KPI wait queue to the file's poll head.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/glue.h>
#include "sim_backend.h"

#include "harness.h"

#define	NODE_CARD	0
#define	NODE_RENDER	128

struct fpriv {
	wait_queue_head_t	evq;
	int			events;
};

static int opens, releases, fail_open, compat_calls;
static uint32_t last_cmd;
static unsigned long last_arg;
static bool last_kernel, last_nonblock;
static long ioctl_ret;

static int
f_open(struct inode *ip, struct lkpi_file *f)
{
	struct fpriv *p;

	if (fail_open)
		return (-EACCES);
	p = kzalloc(sizeof (*p), GFP_KERNEL);
	init_waitqueue_head(&p->evq);
	f->private_data = p;
	opens++;
	CHECK(iminor(ip) == NODE_CARD || iminor(ip) == NODE_RENDER);
	CHECK_EQ(imajor(ip), LKPI_DRM_MAJOR);
	return (0);
}

static int
f_release(struct inode *ip, struct lkpi_file *f)
{
	kfree(f->private_data);
	releases++;
	return (0);
}

static long
f_ioctl(struct lkpi_file *f, unsigned int cmd, unsigned long arg)
{
	last_cmd = cmd;
	last_arg = arg;
	last_kernel = current->lkpi_uaccess_kernel;
	last_nonblock = (f->f_flags & O_NONBLOCK) != 0;
	return (ioctl_ret);
}

static long
f_compat_ioctl(struct lkpi_file *f, unsigned int cmd, unsigned long arg)
{
	compat_calls++;
	return (f_ioctl(f, cmd, arg));
}

static ssize_t
f_read(struct lkpi_file *f, char __user *buf, size_t len, loff_t *pos)
{
	struct fpriv *p = f->private_data;

	if (p->events == 0)
		return ((f->f_flags & O_NONBLOCK) ? -EAGAIN : -ERESTARTSYS);
	p->events--;
	memset(buf, 'e', len < 8 ? len : 8);
	return (len < 8 ? (ssize_t)len : 8);
}

static __poll_t
f_poll(struct lkpi_file *f, poll_table *pt)
{
	struct fpriv *p = f->private_data;

	poll_wait(f, &p->evq, pt);
	return (p->events > 0 ? LKPI_EPOLLIN | LKPI_EPOLLRDNORM : 0);
}

static const struct file_operations card_fops = {
	.open = f_open,
	.release = f_release,
	.unlocked_ioctl = f_ioctl,
	.compat_ioctl = f_compat_ioctl,
	.read = f_read,
	.poll = f_poll,
};

static const struct file_operations render_fops = {
	.open = f_open,
	.release = f_release,
	.unlocked_ioctl = f_ioctl,	/* no compat_ioctl */
};

/* Kernel table: nr 0x41 is a 264-byte struct. */
static uint32_t
card_kcmd(struct lkpi_file *f, unsigned int type, unsigned int nr)
{
	if (type == 'd' && nr == 0x41)
		return (LKPI_IOC(LKPI_IOC_READ | LKPI_IOC_WRITE, 'd', 0x41, 264));
	return (0);
}

TEST(nodes_and_clones)
{
	struct lkpi_node_desc card = { NODE_CARD, "card0", &card_fops,
	    card_kcmd, (void *)0x1 };
	struct lkpi_node_desc render = { NODE_RENDER, "renderD128",
	    &render_fops, NULL, (void *)0x2 };
	uint32_t c1, c2, c3;
	struct lkpi_file *f;

	opens = releases = 0;
	CHECK_EQ(lkpi_node_register(&card), 0);
	CHECK_EQ(lkpi_node_register(&card), -EEXIST);
	CHECK_EQ(lkpi_node_register(&render), 0);
	CHECK(lkpi_node_plat(NODE_RENDER) == (void *)0x2);

	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ | FMODE_WRITE,
	    &c1), 0);
	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ | FMODE_WRITE,
	    &c2), 0);
	CHECK_EQ(c1, 1);
	CHECK_EQ(c2, 2);
	CHECK_EQ(lkpi_cdev_open(NODE_RENDER, O_RDWR, FMODE_READ, &c3), 0);
	CHECK_EQ(c3, 1);
	CHECK_EQ(lkpi_cdev_open(5, O_RDWR, FMODE_READ, &c3), ENXIO);
	CHECK_EQ(lkpi_node_open_count(NODE_CARD), 2);
	CHECK_EQ(lkpi_node_unregister(NODE_CARD), -EBUSY);

	/* Minor encoding round trip and 32-bit safety of low clone ids. */
	CHECK_EQ(lkpi_minor_node(lkpi_minor_make(NODE_RENDER, 7)), NODE_RENDER);
	CHECK_EQ(lkpi_minor_clone(lkpi_minor_make(NODE_RENDER, 7)), 7);
	CHECK(lkpi_minor_make(NODE_RENDER, LKPI_CLONE_MAX32) <= 0x3ffffU);

	/* Lowest id is reused after close. */
	f = lkpi_cdev_file(NODE_CARD, c1);
	CHECK(f != NULL);
	CHECK(lkpi_cdev_file(NODE_CARD, 0) == NULL);
	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c1), 0);
	CHECK_EQ(releases, 0);		/* our lookup reference */
	fput(f);
	CHECK_EQ(releases, 1);
	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c1), EBADF);
	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ, &c1), 0);
	CHECK_EQ(c1, 1);

	/* Failed open: no release, nothing left behind. */
	fail_open = 1;
	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ, &c3), EACCES);
	fail_open = 0;
	CHECK_EQ(lkpi_node_open_count(NODE_CARD), 2);

	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c1), 0);
	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c2), 0);
	CHECK_EQ(lkpi_cdev_close(NODE_RENDER, 1), 0);
	CHECK_EQ(releases, 4);
	CHECK_EQ(opens, 4);
}

TEST(ioctl_dispatch)
{
	struct lkpi_ioctl_ctx ctx = { .abi = LKPI_IOC_ABI_ILLUMOS,
	    .kcmd = card_kcmd };
	struct lkpi_file *f;
	uint32_t c;
	long rv;

	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ, &c), 0);
	f = lkpi_cdev_file(NODE_CARD, c);

	ioctl_ret = 5;
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_OUT, 'd', 0x2, 16),
	    0x1234, &ctx, &rv), 0);
	CHECK_EQ(last_cmd, LKPI_IOC(LKPI_IOC_READ, 'd', 0x2, 16));
	CHECK_EQ(last_arg, 0x1234);
	CHECK_EQ(rv, 5);
	CHECK(!last_kernel);

	/* Large struct: size from the kernel table. */
	ioctl_ret = 0;
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_IN |
	    LKPI_SUN_IOC_OUT, 'd', 0x41, 264), 0, &ctx, &rv), 0);
	CHECK_EQ(LKPI_IOC_SIZE(last_cmd), 264);

	/* Linux-internal errors become illumos errno. */
	ioctl_ret = -ERESTARTSYS;
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_VOID, 'd', 0x3,
	    0), 0, &ctx, &rv), EINTR);
	ioctl_ret = -ENOTSUPP;
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_VOID, 'd', 0x3,
	    0), 0, &ctx, &rv), ENOTSUP);
	CHECK_EQ(lkpi_file_ioctl(f, 0x1f000000U, 0, &ctx, &rv), ENOTTY);

	/* 32-bit caller goes to compat_ioctl; FKIOCTL and FNONBLOCK seen. */
	ioctl_ret = 0;
	ctx.ilp32 = true;
	ctx.kernel = true;
	ctx.nonblock = true;
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_VOID, 'd', 0x3,
	    0), 0, &ctx, &rv), 0);
	CHECK_EQ(compat_calls, 1);
	CHECK(last_kernel);
	CHECK(last_nonblock);
	CHECK(!current->lkpi_uaccess_kernel);	/* restored */
	fput(f);
	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c), 0);

	/* No compat_ioctl: 32-bit callers get ENOTTY (Linux behaviour). */
	CHECK_EQ(lkpi_cdev_open(NODE_RENDER, O_RDWR, FMODE_READ, &c), 0);
	f = lkpi_cdev_file(NODE_RENDER, c);
	CHECK_EQ(lkpi_file_ioctl(f, LKPI_SUN_IOC(LKPI_SUN_IOC_VOID, 'd', 0x3,
	    0), 0, &ctx, &rv), ENOTTY);
	fput(f);
	CHECK_EQ(lkpi_cdev_close(NODE_RENDER, c), 0);
}

TEST(read_and_poll)
{
	struct lkpi_file *f;
	struct fpriv *p;
	char buf[32];
	size_t done;
	__poll_t rev;
	void *ph;
	long wakes;
	uint32_t c;

	CHECK_EQ(lkpi_cdev_open(NODE_CARD, O_RDWR, FMODE_READ, &c), 0);
	f = lkpi_cdev_file(NODE_CARD, c);
	p = f->private_data;

	CHECK_EQ(lkpi_file_read(f, buf, sizeof (buf), true, &done), EAGAIN);
	CHECK_EQ(lkpi_file_read(f, buf, sizeof (buf), false, &done), EINTR);

	CHECK_EQ(lkpi_file_poll(f, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK_EQ(rev, 0);
	CHECK(ph == f->f_lkpi_ph);
	CHECK_EQ(f->f_lkpi_npollreg, 1);
	CHECK_EQ(lkpi_file_poll(f, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK_EQ(f->f_lkpi_npollreg, 1);	/* registered once */

	/* An event: the driver wakes its queue, the poll head is woken. */
	wakes = sim_ph_wakes;
	p->events = 1;
	wake_up_all(&p->evq);
	CHECK_EQ(sim_ph_wakes, wakes + 1);
	CHECK_EQ(lkpi_file_poll(f, LKPI_EPOLLIN | LKPI_EPOLLOUT, &rev, &ph),
	    0);
	CHECK_EQ(rev, LKPI_EPOLLIN);	/* RDNORM not requested */
	CHECK_EQ(lkpi_file_read(f, buf, sizeof (buf), false, &done), 0);
	CHECK_EQ(done, 8);
	CHECK(buf[0] == 'e');

	fput(f);
	CHECK_EQ(lkpi_cdev_close(NODE_CARD, c), 0);
}

TEST(unregister)
{
	CHECK_EQ(lkpi_node_unregister(NODE_CARD), 0);
	CHECK_EQ(lkpi_node_unregister(NODE_RENDER), 0);
	CHECK_EQ(lkpi_node_unregister(NODE_RENDER), -ENOENT);
	CHECK(lkpi_node_plat(NODE_CARD) == NULL);
}

LKPI_SUITE(drmglue_cdev)
{
	sim_init();
	RUN(nodes_and_clones);
	RUN(ioctl_dispatch);
	RUN(read_and_poll);
	RUN(unregister);
	CHECK_EQ(sim_live_phs, 0);
}
