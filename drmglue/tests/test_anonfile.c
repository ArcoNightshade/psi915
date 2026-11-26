/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Anonymous files over the simulated fd/vnode layer (src/anonfile.c):
 * release timing across dup, fork-style sharing, SCM_RIGHTS transfer,
 * repeated fd_install of one file, O_CLOEXEC, fget type checks, a
 * fd_install/close race; then the pseudo-filesystem file lifecycle that
 * upstream dma-buf.c uses (src/pseudofs.c), modelled by a minimal
 * exporter written for this test: alloc_anon_inode + alloc_file_pseudo,
 * d_fsdata, d_release after f_op->release, size via getattr, mmap
 * through the anonymous mmap path, poll through a wait queue.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <drmglue/pseudofs.h>
#include <drmglue/lkpi_mm.h>
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
 * Upstream dma-buf.c file pattern (written from the documented API; no
 * upstream code). The object is freed in d_release, after f_op->release.
 */
struct pbuf {
	size_t			size;
	wait_queue_head_t	poll;
	int			ready;
};

static int pb_file_released, pb_d_released, pb_order_ok;

static void
pb_d_release(struct dentry *d)
{
	struct pbuf *b = d->d_fsdata;

	pb_order_ok = pb_file_released == 1;
	pb_d_released++;
	kfree(b);
}

static char *
pb_d_dname(struct dentry *d, char *buf, int len)
{
	return (dynamic_dname(buf, len, "/%s:%s", d->d_name.name, "test"));
}

static const struct dentry_operations pb_dops = {
	.d_dname = pb_d_dname,
	.d_release = pb_d_release,
};

static int
pb_init_fs_context(struct fs_context *fc)
{
	struct pseudo_fs_context *ctx = init_pseudo(fc, 0x444d4142);

	if (ctx == NULL)
		return (-ENOMEM);
	ctx->dops = &pb_dops;
	return (0);
}

static struct file_system_type pb_fs_type = {
	.name = "pbuf",
	.init_fs_context = pb_init_fs_context,
	.kill_sb = kill_anon_super,
};

static int
pb_release(struct inode *ip, struct lkpi_file *f)
{
	pb_file_released++;
	return (0);
}

static int
pb_mmap(struct lkpi_file *f, struct vm_area_struct *vma)
{
	return (remap_pfn_range(vma, vma->vm_start, 900 + vma->vm_pgoff,
	    vma->vm_end - vma->vm_start, vma->vm_page_prot));
}

static __poll_t
pb_poll(struct lkpi_file *f, poll_table *pt)
{
	struct pbuf *b = f->private_data;

	poll_wait(f, &b->poll, pt);
	return (b->ready ? LKPI_EPOLLIN : 0);
}

static const struct file_operations pb_fops = {
	.release = pb_release,
	.mmap = pb_mmap,
	.poll = pb_poll,
};

TEST(pseudo_file_lifecycle)
{
	struct vfsmount *mnt = kern_mount(&pb_fs_type);
	struct lkpi_file *f, *g;
	struct inode *ip;
	struct pbuf *b;
	struct sim_seg *s;
	char nbuf[64], *nm;
	__poll_t rev;
	void *ph;
	long wakes;
	int fd;

	pb_file_released = pb_d_released = pb_order_ok = 0;
	CHECK(!IS_ERR(mnt));
	CHECK_EQ(mnt->mnt_sb->s_magic, 0x444d4142);
	b = kzalloc(sizeof (*b), GFP_KERNEL);
	b->size = 2 * PAGE_SIZE + 10;
	init_waitqueue_head(&b->poll);

	ip = alloc_anon_inode(mnt->mnt_sb);
	CHECK(!IS_ERR(ip));
	CHECK(ip->i_ino != 0);
	ip->i_size = (loff_t)b->size;
	inode_set_bytes(ip, (loff_t)b->size);
	f = alloc_file_pseudo(ip, mnt, "dmabuf", O_RDWR, &pb_fops);
	CHECK(!IS_ERR(f));
	f->private_data = b;
	f->f_path.dentry->d_fsdata = b;
	CHECK(f->f_path.dentry->d_op == &pb_dops);
	nm = f->f_path.dentry->d_op->d_dname(f->f_path.dentry, nbuf,
	    sizeof (nbuf));
	CHECK(!IS_ERR(nm) && strcmp(nm, "/dmabuf:test") == 0);

	fd = get_unused_fd_flags(O_CLOEXEC);
	fd_install((unsigned int)fd, f);
	CHECK(sim_fd_cloexec(fd));
	CHECK_EQ(sim_fd_size(fd), 2 * PAGE_SIZE + 10);

	/* Import path: fget + f_op check, as dma_buf_get does. */
	g = fget((unsigned int)fd);
	CHECK(g == f && g->f_op == &pb_fops);
	CHECK_EQ(file_count(f), 2);

	/* Poll through the wait queue. */
	CHECK_EQ(lkpi_file_poll(f, LKPI_EPOLLIN, &rev, &ph), 0);
	CHECK_EQ(rev, 0);
	wakes = sim_ph_wakes;
	b->ready = 1;
	wake_up_all(&b->poll);
	CHECK_EQ(sim_ph_wakes, wakes + 1);

	/* mmap through the anonymous-file path. */
	CHECK_EQ(sim_mmap(f, PAGE_SIZE, 2 * PAGE_SIZE, LKPI_PROT_READ, &s), 0);
	CHECK_EQ(sim_access(s, 1, false), 902);

	/* Freed only after fd, kernel ref and mapping are all gone. */
	CHECK_EQ(sim_close(fd), 0);
	fput(g);
	CHECK_EQ(pb_file_released, 0);
	CHECK_EQ(pb_d_released, 0);
	sim_munmap(s, 0, 2, NULL, NULL);
	CHECK_EQ(pb_file_released, 1);
	CHECK_EQ(pb_d_released, 1);
	CHECK(pb_order_ok);
	kern_unmount(mnt);
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
	RUN(pseudo_file_lifecycle);
	CHECK_EQ(sim_live_vnodes, vn);
	CHECK_EQ(sim_live_phs, ph);
	CHECK_EQ(sim_live_wins, 0);
}
