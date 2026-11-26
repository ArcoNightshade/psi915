/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * sync_file: a dma_fence exported as a file descriptor (docs/10, section
 * 4.4). Clean-room from the documented API and uAPI.
 *
 * Gaps (docs/10): SYNC_IOC_MERGE only for the trivial cases (same fence,
 * or one side signaled); fence timestamps are reported as 0;
 * SYNC_IOC_SET_DEADLINE is ENOTTY.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/sync_file.h>
#include <drmglue/anonfile.h>
#include <drmglue/glue.h>
#include <drmglue/xlate.h>

/* uAPI (include/uapi/linux/sync_file.h layouts and numbers). */
struct lkpi_sync_merge_data {
	char		name[32];
	int32_t		fd2;
	int32_t		fence;
	uint32_t	flags;
	uint32_t	pad;
};

struct lkpi_sync_fence_info {
	char		obj_name[32];
	char		driver_name[32];
	int32_t		status;
	uint32_t	flags;
	uint64_t	timestamp_ns;
};

struct lkpi_sync_file_info {
	char		name[32];
	int32_t		status;
	uint32_t	flags;
	uint32_t	num_fences;
	uint32_t	pad;
	uint64_t	sync_fence_info;
};

#define	LKPI_SYNC_IOC_MERGE	LKPI_IOC(LKPI_IOC_READ | LKPI_IOC_WRITE, '>', \
				    3, sizeof (struct lkpi_sync_merge_data))
#define	LKPI_SYNC_IOC_FILE_INFO	LKPI_IOC(LKPI_IOC_READ | LKPI_IOC_WRITE, '>', \
				    4, sizeof (struct lkpi_sync_file_info))

static const struct file_operations sync_file_fops;

static int
lkpi_sf_copyin(void *k, unsigned long u, size_t len)
{
	return (lkpi_plat_copyin((const void *)u, k, len,
	    current->lkpi_uaccess_kernel) == 0 ? 0 : -EFAULT);
}

static int
lkpi_sf_copyout(unsigned long u, const void *k, size_t len)
{
	return (lkpi_plat_copyout(k, (void *)u, len,
	    current->lkpi_uaccess_kernel) == 0 ? 0 : -EFAULT);
}

static struct sync_file *
sync_file_alloc(void)
{
	struct sync_file *sf = kzalloc(sizeof (*sf), GFP_KERNEL);

	if (sf == NULL)
		return (NULL);
	sf->file = anon_inode_getfile("sync_file", &sync_file_fops, sf, 0);
	if (IS_ERR(sf->file)) {
		kfree(sf);
		return (NULL);
	}
	return (sf);
}

struct sync_file *
sync_file_create(struct dma_fence *fence)
{
	struct sync_file *sf = sync_file_alloc();

	if (sf == NULL)
		return (NULL);
	sf->fence = dma_fence_get(fence);
	return (sf);
}

static struct sync_file *
sync_file_fdget(int fd)
{
	struct lkpi_file *f;

	if (fd < 0)
		return (NULL);
	f = fget((unsigned int)fd);
	if (f == NULL)
		return (NULL);
	if (f->f_op != &sync_file_fops) {
		fput(f);
		return (NULL);
	}
	return (f->private_data);
}

struct dma_fence *
sync_file_get_fence(int fd)
{
	struct sync_file *sf = sync_file_fdget(fd);
	struct dma_fence *fence;

	if (sf == NULL)
		return (NULL);
	fence = dma_fence_get(sf->fence);
	fput(sf->file);
	return (fence);
}

char *
sync_file_get_name(struct sync_file *sf, char *buf, int len)
{
	if (sf->user_name[0] != '\0')
		strscpy(buf, sf->user_name, len);
	else
		snprintf(buf, len, "%s-%s-%llu",
		    sf->fence->ops->get_driver_name(sf->fence),
		    sf->fence->ops->get_timeline_name(sf->fence),
		    (unsigned long long)sf->fence->seqno);
	return (buf);
}

static int
sync_file_release(struct inode *ip, struct lkpi_file *f)
{
	struct sync_file *sf = f->private_data;

	if (test_bit(LKPI_SYNC_FILE_POLL_ENABLED, &sf->flags))
		(void) dma_fence_remove_callback(sf->fence, &sf->cb);
	dma_fence_put(sf->fence);
	kfree(sf);
	return (0);
}

static void
sync_file_fence_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct sync_file *sf = container_of(cb, struct sync_file, cb);

	/* Fence-signal context (may be an interrupt handler). */
	lkpi_file_poll_wake(sf->file, LKPI_EPOLLIN | LKPI_EPOLLRDNORM);
}

static __poll_t
sync_file_poll(struct lkpi_file *f, poll_table *pt)
{
	struct sync_file *sf = f->private_data;

	/*
	 * The file's poll head is woken from the fence callback, so no wait
	 * queue is registered with poll_wait(); the callback is armed once
	 * and stays armed until release.
	 */
	if (!poll_does_not_wait(pt) &&
	    !test_and_set_bit(LKPI_SYNC_FILE_POLL_ENABLED, &sf->flags)) {
		if (dma_fence_add_callback(sf->fence, &sf->cb,
		    sync_file_fence_cb) < 0)
			lkpi_file_poll_wake(f, LKPI_EPOLLIN);
	}
	return (dma_fence_is_signaled(sf->fence) ? LKPI_EPOLLIN : 0);
}

static long
sync_file_ioctl_merge(struct sync_file *sf, unsigned long arg)
{
	struct lkpi_sync_merge_data d;
	struct sync_file *sf2, *out;
	struct dma_fence *pick;
	int fd, r;

	r = lkpi_sf_copyin(&d, arg, sizeof (d));
	if (r != 0)
		return (r);
	if (d.flags != 0 || d.pad != 0)
		return (-EINVAL);
	sf2 = sync_file_fdget(d.fd2);
	if (sf2 == NULL)
		return (-ENOENT);
	if (sf2->fence == sf->fence || dma_fence_is_signaled(sf2->fence))
		pick = sf->fence;
	else if (dma_fence_is_signaled(sf->fence))
		pick = sf2->fence;
	else
		pick = NULL;	/* needs dma_fence_unwrap_merge (R10) */
	if (pick == NULL) {
		fput(sf2->file);
		return (-ENOTSUPP);
	}
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		fput(sf2->file);
		return (fd);
	}
	out = sync_file_create(pick);
	fput(sf2->file);
	if (out == NULL) {
		put_unused_fd((unsigned int)fd);
		return (-ENOMEM);
	}
	d.name[sizeof (d.name) - 1] = '\0';
	strscpy(out->user_name, d.name, sizeof (out->user_name));
	d.fence = fd;
	r = lkpi_sf_copyout(arg, &d, sizeof (d));
	if (r != 0) {
		put_unused_fd((unsigned int)fd);
		fput(out->file);
		return (r);
	}
	fd_install((unsigned int)fd, out->file);
	return (0);
}

static long
sync_file_ioctl_info(struct sync_file *sf, unsigned long arg)
{
	struct lkpi_sync_file_info info;
	struct lkpi_sync_fence_info fi;
	int r;

	r = lkpi_sf_copyin(&info, arg, sizeof (info));
	if (r != 0)
		return (r);
	if (info.flags != 0 || info.pad != 0)
		return (-EINVAL);
	if (info.num_fences != 0) {
		memset(&fi, 0, sizeof (fi));
		strscpy(fi.obj_name,
		    sf->fence->ops->get_timeline_name(sf->fence),
		    sizeof (fi.obj_name));
		strscpy(fi.driver_name,
		    sf->fence->ops->get_driver_name(sf->fence),
		    sizeof (fi.driver_name));
		fi.status = dma_fence_get_status(sf->fence);
		r = lkpi_sf_copyout((unsigned long)info.sync_fence_info, &fi,
		    sizeof (fi));
		if (r != 0)
			return (r);
	}
	(void) sync_file_get_name(sf, info.name, sizeof (info.name));
	info.status = dma_fence_get_status(sf->fence);
	info.num_fences = 1;
	return (lkpi_sf_copyout(arg, &info, sizeof (info)));
}

static long
sync_file_ioctl(struct lkpi_file *f, unsigned int cmd, unsigned long arg)
{
	struct sync_file *sf = f->private_data;

	switch (cmd) {
	case LKPI_SYNC_IOC_MERGE:
		return (sync_file_ioctl_merge(sf, arg));
	case LKPI_SYNC_IOC_FILE_INFO:
		return (sync_file_ioctl_info(sf, arg));
	default:
		return (-ENOTTY);
	}
}

static const struct file_operations sync_file_fops = {
	.release = sync_file_release,
	.poll = sync_file_poll,
	.unlocked_ioctl = sync_file_ioctl,
	.compat_ioctl = sync_file_ioctl,
};
