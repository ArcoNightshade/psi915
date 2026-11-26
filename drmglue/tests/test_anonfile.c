/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Anonymous files over the simulated fd/vnode layer (src/anonfile.c):
 * release timing across dup, fork-style sharing, SCM_RIGHTS transfer,
 * repeated fd_install of one file, O_CLOEXEC, fget type checks, a
 * fd_install/close race; then dma-buf (src/dma_buf.c) export/import, size,
 * ioctls, attachments, mmap; and sync_file (src/sync_file.c) poll and
 * ioctls with a test fence.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#include <drmglue/anonfile.h>
#include <drmglue/glue.h>
#include "sim_backend.h"

#include "harness.h"

static int anon_released;

static int
anon_release(struct inode *ip, struct lkpi_file *f)
{
	anon_released++;
	return (0);
}

static const struct file_operations anon_fops = {
	.release = anon_release,
};

static int
install(struct lkpi_file *f, unsigned int flags)
{
	int fd = get_unused_fd_flags(flags);

	if (fd >= 0)
		fd_install((unsigned int)fd, f);
	return (fd);
}

TEST(release_after_last_close)
{
	struct lkpi_file *f;
	int fd, fd2;
	long vn = sim_live_vnodes;

	anon_released = 0;
	f = anon_inode_getfile("t", &anon_fops, NULL, O_RDWR);
	CHECK(!IS_ERR(f));
	CHECK(f->f_mode & FMODE_WRITE);
	fd = install(f, 0);
	CHECK(fd >= 0);
	CHECK_EQ(sim_live_vnodes, vn + 1);
	fd2 = sim_dup(fd);
	CHECK_EQ(sim_close(fd), 0);
	CHECK_EQ(anon_released, 0);
	CHECK_EQ(sim_close(fd2), 0);
	CHECK_EQ(anon_released, 1);
	CHECK_EQ(sim_live_vnodes, vn);
}

TEST(kernel_ref_outlives_fd)
{
	struct lkpi_file *f, *g;
	int fd;

	anon_released = 0;
	f = anon_inode_getfile("t", &anon_fops, NULL, O_RDWR);
	fd = install(f, 0);
	g = fget((unsigned int)fd);
	CHECK(g == f);
	CHECK(fget(63) == NULL);
	CHECK(fget(1000) == NULL);
	CHECK_EQ(sim_close(fd), 0);
	CHECK_EQ(anon_released, 0);
	fput(g);
	CHECK_EQ(anon_released, 1);
}

TEST(scm_rights_transfer)
{
	struct sim_proc *a = sim_proc_new(), *b = sim_proc_new();
	struct lkpi_file *f;
	int fd, rfd;

	anon_released = 0;
	sim_proc_set(a);
	f = anon_inode_getfile("t", &anon_fops, NULL, O_RDWR);
	fd = install(f, O_CLOEXEC);
	CHECK(sim_fd_cloexec(fd));
	rfd = sim_send_fd(a, fd, b);
	CHECK(rfd >= 0);
	sim_proc_exit(a);			/* sender exits */
	CHECK_EQ(anon_released, 0);
	sim_proc_set(b);
	CHECK(fget((unsigned int)rfd) == f);
	fput(f);
	sim_proc_exit(b);
	sim_proc_set(NULL);
	CHECK_EQ(anon_released, 1);
}

TEST(install_twice_one_vnode)
{
	struct lkpi_file *f;
	long vn = sim_live_vnodes;
	int fd1, fd2;

	anon_released = 0;
	f = anon_inode_getfile("t", &anon_fops, NULL, O_RDWR);
	(void) get_file(f);		/* like drm_prime: get_dma_buf, export */
	fd1 = install(f, 0);
	fd2 = install(f, 0);		/* consumes the second reference */
	CHECK(fd1 >= 0 && fd2 >= 0 && fd1 != fd2);
	CHECK_EQ(sim_live_vnodes, vn + 1);
	CHECK_EQ(atomic_long_read(&f->f_count), 1);
	CHECK_EQ(sim_close(fd1), 0);
	CHECK_EQ(anon_released, 0);
	CHECK_EQ(sim_close(fd2), 0);
	CHECK_EQ(anon_released, 1);
	CHECK_EQ(sim_live_vnodes, vn);
}

TEST(reserve_cancel)
{
	int fd = get_unused_fd_flags(O_CLOEXEC), fd2;

	CHECK(fd >= 0);
	put_unused_fd((unsigned int)fd);
	fd2 = get_unused_fd_flags(0);
	CHECK_EQ(fd2, fd);
	put_unused_fd((unsigned int)fd2);
}

/* fd_install of an existing file racing with close of its last fd. */
static struct lkpi_file *race_f;
static atomic_t race_tid;

static void
race_body(void *arg)
{
	int i, fd;

	if (atomic_inc_return(&race_tid) % 2 == 0) {
		for (i = 0; i < 4000; i++) {
			(void) get_file(race_f);
			fd = install(race_f, 0);
			if (fd >= 0)
				(void) sim_close(fd);
			else
				fput(race_f);
		}
	} else {
		for (i = 0; i < 4000; i++) {
			struct lkpi_file *g = fget((unsigned int)(i % 8));

			if (g != NULL)
				fput(g);
		}
	}
}

TEST(install_close_race)
{
	long vn = sim_live_vnodes;

	anon_released = 0;
	race_f = anon_inode_getfile("t", &anon_fops, NULL, O_RDWR);
	atomic_set(&race_tid, 0);
	harness_run_threads(4, race_body, NULL);
	CHECK_EQ(anon_released, 0);
	CHECK_EQ(sim_live_vnodes, vn);
	CHECK_EQ(atomic_long_read(&race_f->f_count), 1);
	fput(race_f);
	CHECK_EQ(anon_released, 1);
}

/*
 * dma-buf
 */
static int db_released, db_begin, db_end, db_attach, db_map, db_unmap;
static enum dma_data_direction db_dir;
static char db_sgt;	/* fake sg_table address */

static struct sg_table *
db_map_fn(struct dma_buf_attachment *a, enum dma_data_direction d)
{
	db_map++;
	return ((struct sg_table *)&db_sgt);
}

static void
db_unmap_fn(struct dma_buf_attachment *a, struct sg_table *s,
    enum dma_data_direction d)
{
	db_unmap++;
}

static void
db_release_fn(struct dma_buf *db)
{
	db_released++;
}

static int
db_attach_fn(struct dma_buf *db, struct dma_buf_attachment *a)
{
	db_attach++;
	return (0);
}

static int
db_begin_fn(struct dma_buf *db, enum dma_data_direction d)
{
	db_begin++;
	db_dir = d;
	return (0);
}

static int
db_end_fn(struct dma_buf *db, enum dma_data_direction d)
{
	db_end++;
	db_dir = d;
	return (0);
}

static int
db_mmap_fn(struct dma_buf *db, struct vm_area_struct *vma)
{
	return (remap_pfn_range(vma, vma->vm_start, 500 + vma->vm_pgoff,
	    vma->vm_end - vma->vm_start, vma->vm_page_prot));
}

static const struct dma_buf_ops db_ops = {
	.cache_sgt_mapping = true,
	.attach = db_attach_fn,
	.map_dma_buf = db_map_fn,
	.unmap_dma_buf = db_unmap_fn,
	.release = db_release_fn,
	.begin_cpu_access = db_begin_fn,
	.end_cpu_access = db_end_fn,
	.mmap = db_mmap_fn,
};

/* illumos-encoded DMA_BUF_IOCTL_SYNC: _IOW('b', 0, 8) */
#define	S_DMA_BUF_IOCTL_SYNC	LKPI_SUN_IOC(LKPI_SUN_IOC_IN, 'b', 0, 8)

static int
db_ioctl(struct dma_buf *db, uint32_t cmd, void *arg)
{
	struct lkpi_ioctl_ctx ctx = { .abi = LKPI_IOC_ABI_ILLUMOS };
	long rv;

	return (lkpi_file_ioctl(db->file, cmd, (unsigned long)arg, &ctx,
	    &rv));
}

TEST(dma_buf_basics)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp);
	struct dma_buf *db, *db2;
	struct dma_buf_attachment *a;
	struct sg_table *sgt;
	struct sim_seg *s, *s2;
	uint64_t flags;
	int fd, sfd;
	long resv = sim_live_resv;

	db_released = db_begin = db_end = db_attach = db_map = db_unmap = 0;
	exp.ops = &db_ops;
	exp.size = 3 * PAGE_SIZE + 100;
	exp.flags = O_RDWR;
	CHECK(IS_ERR(dma_buf_export(NULL)));
	db = dma_buf_export(&exp);
	CHECK(!IS_ERR(db));
	CHECK_EQ(sim_live_resv, resv + 1);	/* exporter gave no resv */
	fd = dma_buf_fd(db, O_CLOEXEC);
	CHECK(fd >= 0);
	CHECK(sim_fd_cloexec(fd));
	CHECK_EQ(sim_fd_size(fd), 3 * PAGE_SIZE + 100);	/* lseek(SEEK_END) */

	db2 = dma_buf_get(fd);
	CHECK(db2 == db);

	/* attach / map (cached) / detach */
	a = dma_buf_attach(db, (struct device *)&db_sgt);
	CHECK(!IS_ERR(a));
	sgt = dma_buf_map_attachment(a, DMA_BIDIRECTIONAL);
	CHECK(sgt == (struct sg_table *)&db_sgt);
	CHECK(dma_buf_map_attachment(a, DMA_BIDIRECTIONAL) == sgt);
	CHECK_EQ(db_map, 1);
	dma_buf_unmap_attachment(a, sgt, DMA_BIDIRECTIONAL);
	CHECK_EQ(db_unmap, 0);
	dma_buf_detach(db, a);
	CHECK_EQ(db_unmap, 1);
	CHECK_EQ(db_attach, 1);

	/* DMA_BUF_IOCTL_SYNC through the generic ioctl path */
	flags = 1 | 0;		/* READ | START */
	CHECK_EQ(db_ioctl(db, S_DMA_BUF_IOCTL_SYNC, &flags), 0);
	CHECK_EQ(db_begin, 1);
	CHECK_EQ(db_dir, DMA_FROM_DEVICE);
	flags = 3 | 4;		/* RW | END */
	CHECK_EQ(db_ioctl(db, S_DMA_BUF_IOCTL_SYNC, &flags), 0);
	CHECK_EQ(db_end, 1);
	CHECK_EQ(db_dir, DMA_BIDIRECTIONAL);
	flags = 0x80;
	CHECK_EQ(db_ioctl(db, S_DMA_BUF_IOCTL_SYNC, &flags), EINVAL);
	flags = 4;		/* END without direction */
	CHECK_EQ(db_ioctl(db, S_DMA_BUF_IOCTL_SYNC, &flags), EINVAL);
	CHECK_EQ(db_ioctl(db, S_DMA_BUF_IOCTL_SYNC, (void *)8), EFAULT);
	CHECK_EQ(db_ioctl(db, LKPI_SUN_IOC(LKPI_SUN_IOC_IN, 'b', 9, 8),
	    &flags), ENOTTY);

	/* mmap: in range works (pages 1..3), past the end is refused */
	CHECK_EQ(sim_mmap(db->file, PAGE_SIZE, 3 * PAGE_SIZE, LKPI_PROT_READ,
	    &s), 0);
	CHECK_EQ(sim_access(s, 2, false), 503);
	CHECK_EQ(sim_mmap(db->file, 2 * PAGE_SIZE, 3 * PAGE_SIZE,
	    LKPI_PROT_READ, &s2), EINVAL);

	/* Not a dma-buf: EINVAL; bad fd: EBADF. */
	{
		struct lkpi_file *o = anon_inode_getfile("x", &anon_fops,
		    NULL, 0);

		sfd = install(o, 0);
		CHECK_EQ(PTR_ERR(dma_buf_get(sfd)), -EINVAL);
		CHECK_EQ(PTR_ERR(dma_buf_get(-1)), -EBADF);
		(void) sim_close(sfd);
	}

	/* Release needs: fd closed, dma_buf_get ref, and the mapping. */
	CHECK_EQ(sim_close(fd), 0);
	dma_buf_put(db2);
	CHECK_EQ(db_released, 0);
	sim_munmap(s, 0, 3, NULL, NULL);
	CHECK_EQ(db_released, 1);
	CHECK_EQ(sim_live_resv, resv);
}

/*
 * sync_file
 */
static const char *
tf_name(struct dma_fence *f)
{
	return ("test");
}

static const struct dma_fence_ops tf_ops = {
	.get_driver_name = tf_name,
	.get_timeline_name = tf_name,
};

struct t_merge {
	char		name[32];
	int32_t		fd2;
	int32_t		fence;
	uint32_t	flags;
	uint32_t	pad;
};

struct t_info {
	char		name[32];
	int32_t		status;
	uint32_t	flags;
	uint32_t	num_fences;
	uint32_t	pad;
	uint64_t	sync_fence_info;
};

TEST(sync_file_poll_info)
{
	struct dma_fence *fence = kzalloc(sizeof (*fence), GFP_KERNEL);
	struct dma_fence *f2 = kzalloc(sizeof (*f2), GFP_KERNEL);
	struct lkpi_ioctl_ctx ctx = { .abi = LKPI_IOC_ABI_ILLUMOS };
	struct sync_file *sf, *sf2;
	struct t_merge m;
	struct t_info info;
	__poll_t rev;
	void *ph;
	long wakes, rv;
	int fd, fd2;

	dma_fence_init(fence, &tf_ops, 7);
	dma_fence_init(f2, &tf_ops, 8);
	sf = sync_file_create(fence);
	CHECK(sf != NULL);
	fd = install(sf->file, O_CLOEXEC);

	CHECK_EQ(lkpi_file_poll(sf->file, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK_EQ(rev, 0);
	wakes = sim_ph_wakes;
	dma_fence_signal(fence);
	CHECK_EQ(sim_ph_wakes, wakes + 1);
	CHECK_EQ(lkpi_file_poll(sf->file, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK_EQ(rev, LKPI_EPOLLIN);

	CHECK(sync_file_get_fence(fd) == fence);
	dma_fence_put(fence);

	/* SYNC_IOC_FILE_INFO = _IOWR('>', 4, 56) in illumos encoding */
	memset(&info, 0, sizeof (info));
	CHECK_EQ(lkpi_file_ioctl(sf->file, LKPI_SUN_IOC(LKPI_SUN_IOC_IN |
	    LKPI_SUN_IOC_OUT, '>', 4, sizeof (info)), (unsigned long)&info,
	    &ctx, &rv), 0);
	CHECK_EQ(info.status, 1);
	CHECK_EQ(info.num_fences, 1);
	CHECK(strcmp(info.name, "test-test-7") == 0);

	/* Merge with an unsignaled second fence: the signaled one drops. */
	sf2 = sync_file_create(f2);
	fd2 = install(sf2->file, 0);
	memset(&m, 0, sizeof (m));
	m.fd2 = fd2;
	strscpy(m.name, "merged", sizeof (m.name));
	CHECK_EQ(lkpi_file_ioctl(sf->file, LKPI_SUN_IOC(LKPI_SUN_IOC_IN |
	    LKPI_SUN_IOC_OUT, '>', 3, sizeof (m)), (unsigned long)&m, &ctx,
	    &rv), 0);
	CHECK(m.fence >= 0);
	CHECK(sync_file_get_fence(m.fence) == f2);
	dma_fence_put(f2);
	CHECK(sim_fd_cloexec(m.fence));
	(void) sim_close(m.fence);

	/* Armed callback on an unsignaled fence is removed at release. */
	CHECK_EQ(lkpi_file_poll(sf2->file, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK(!list_empty(&f2->cb_list));
	(void) sim_close(fd2);
	CHECK(list_empty(&f2->cb_list));
	CHECK(sync_file_get_fence(fd2) == NULL);
	(void) sim_close(fd);
	dma_fence_put(f2);	/* test's own references */
	dma_fence_put(fence);
}

LKPI_SUITE(drmglue_anonfile)
{
	long vn, ph;

	sim_init();
	vn = sim_live_vnodes;
	ph = sim_live_phs;
	RUN(release_after_last_close);
	RUN(kernel_ref_outlives_fd);
	RUN(scm_rights_transfer);
	RUN(install_twice_one_vnode);
	RUN(reserve_cancel);
	RUN(install_close_race);
	RUN(dma_buf_basics);
	RUN(sync_file_poll_info);
	CHECK_EQ(sim_live_vnodes, vn);
	CHECK_EQ(sim_live_phs, ph);
	CHECK_EQ(sim_live_wins, 0);
}
