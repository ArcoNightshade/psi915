/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux struct file emulation: lifecycle (alloc, fput/release), and the
 * ioctl, read and poll dispatchers shared by /dev/dri clone opens and
 * anonymous files. See <drmglue/glue.h> and docs/10, section 2.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>

const struct lkpi_glue_backend *lkpi_glue_be;

void
lkpi_glue_set_backend(const struct lkpi_glue_backend *be)
{
	lkpi_glue_be = be;
}

void
lkpi_address_space_init(struct address_space *m, struct inode *host)
{
	m->host = host;
	init_rwsem(&m->i_mmap_rwsem);
	lkpi_itree_init(&m->i_mmap);
}

struct inode *
lkpi_inode_alloc(uint32_t rdev, umode_t mode)
{
	struct inode *ip;

	ip = kzalloc(sizeof (*ip), GFP_KERNEL);
	if (ip == NULL)
		return (NULL);
	ip->i_rdev = rdev;
	ip->i_mode = mode;
	atomic_set(&ip->i_count, 1);
	lkpi_address_space_init(&ip->i_data, ip);
	ip->i_mapping = &ip->i_data;
	return (ip);
}

void
lkpi_inode_put(struct inode *ip)
{
	if (ip == NULL || !atomic_dec_and_test(&ip->i_count))
		return;
	/* A mapping still registered here would be a use-after-free. */
	WARN_ON(!lkpi_itree_empty(&ip->i_data.i_mmap));
	lkpi_rwsem_destroy(&ip->i_data.i_mmap_rwsem);
	kfree(ip);
}

struct lkpi_file *
lkpi_file_alloc(enum lkpi_file_kind kind, const struct file_operations *fops,
    struct inode *ip, unsigned int flags, fmode_t mode)
{
	struct lkpi_file *f;

	f = kzalloc(sizeof (*f), GFP_KERNEL);
	if (f == NULL)
		return (NULL);
	f->f_lkpi_kind = kind;
	f->f_op = fops;
	f->f_inode = ip;
	f->f_mapping = ip != NULL ? ip->i_mapping : NULL;
	f->f_flags = flags;
	f->f_mode = mode | FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
	atomic_long_set(&f->f_count, 1);
	mutex_init(&f->f_lkpi_lock);
	mutex_init(&f->f_lkpi_mmap_lock);
	f->f_lkpi_ph = lkpi_glue_be->ph_alloc(f);
	if (f->f_lkpi_ph == NULL) {
		mutex_destroy(&f->f_lkpi_lock);
		mutex_destroy(&f->f_lkpi_mmap_lock);
		kfree(f);
		return (NULL);
	}
	return (f);
}

static void
lkpi_file_unregister_polls(struct lkpi_file *f)
{
	unsigned int i;

	mutex_lock(&f->f_lkpi_lock);
	for (i = 0; i < f->f_lkpi_npollreg; i++)
		remove_wait_queue(f->f_lkpi_pollreg[i].wq,
		    &f->f_lkpi_pollreg[i].we);
	f->f_lkpi_npollreg = 0;
	mutex_unlock(&f->f_lkpi_lock);
}

void lkpi_anon_file_released(struct lkpi_file *f);	/* anonfile.c */
void dput(struct dentry *d);				/* pseudofs.c */

void
lkpi_file_free(struct lkpi_file *f)
{
	lkpi_file_unregister_polls(f);
	if (f->f_lkpi_kind == LKPI_FILE_ANON)
		lkpi_anon_file_released(f);
	if (lkpi_glue_be->file_fini != NULL)
		lkpi_glue_be->file_fini(f);
	lkpi_glue_be->ph_free(f->f_lkpi_ph);
	lkpi_inode_put(f->f_lkpi_own_inode);
	/* alloc_file_pseudo files: the last dput runs d_op->d_release. */
	dput(f->f_path.dentry);
	mutex_destroy(&f->f_lkpi_lock);
	mutex_destroy(&f->f_lkpi_mmap_lock);
	kfree(f);
}

static void
lkpi_file_final(struct lkpi_file *f)
{
	/* Stop poll forwarding before release frees the wait queues. */
	lkpi_file_unregister_polls(f);
	if (f->f_op != NULL && f->f_op->release != NULL)
		(void) f->f_op->release(f->f_inode, f);
	lkpi_file_free(f);
}

static void
lkpi_file_final_work(struct work_struct *w)
{
	lkpi_file_final(container_of(w, struct lkpi_file, f_lkpi_free_work));
}

/*
 * The final release runs synchronously in the thread dropping the last
 * reference, except in interrupt context (e.g. upstream dma-buf.c's fence
 * callback dma_buf_poll_cb() drops a file reference), where it is queued
 * on the system workqueue. Linux defers every final __fput() to task work
 * or a workqueue; a driver that fput()s while holding a lock its own
 * release takes would deadlock here (docs/10, section 2.6, risk K3).
 */
void
fput(struct lkpi_file *f)
{
	if (f == NULL)
		return;
	if (!atomic_long_dec_and_test(&f->f_count))
		return;
	if (lkpi_plat_in_interrupt()) {
		INIT_WORK(&f->f_lkpi_free_work, lkpi_file_final_work);
		schedule_work(&f->f_lkpi_free_work);
		return;
	}
	lkpi_file_final(f);
}

loff_t
noop_llseek(struct lkpi_file *f, loff_t off, int whence)
{
	return (f->f_pos);
}

loff_t
no_llseek(struct lkpi_file *f, loff_t off, int whence)
{
	return (-ESPIPE);
}

/*
 * ioctl
 */
int
lkpi_file_ioctl(struct lkpi_file *f, uint32_t ucmd, unsigned long arg,
    const struct lkpi_ioctl_ctx *ctx, long *rvalp)
{
	long (*fn)(struct lkpi_file *, unsigned int, unsigned long);
	struct task_struct *t;
	uint32_t kcmd = 0, lcmd;
	bool saved;
	long r;
	int e;

	*rvalp = 0;
	if (ctx->kcmd != NULL)
		kcmd = ctx->kcmd(f, (ucmd >> 8) & 0xffU, ucmd & 0xffU);
	e = lkpi_ioc_translate(ucmd, ctx->abi, kcmd, &lcmd);
	if (e != 0)
		return (lkpi_errno_to_user(e));

	/* Linux: no compat_ioctl means 32-bit callers get ENOTTY. */
	fn = ctx->ilp32 ? f->f_op->compat_ioctl : f->f_op->unlocked_ioctl;
	if (fn == NULL)
		return (ENOTTY);

	if (ctx->nonblock)
		f->f_flags |= O_NONBLOCK;
	else
		f->f_flags &= ~O_NONBLOCK;

	/*
	 * FKIOCTL (an in-kernel ldi_ioctl caller): make copy_{from,to}_user
	 * treat arg as a kernel address for the duration of the call.
	 * in_compat_syscall() is answered by the KPI from the process data
	 * model (requested change R6), so nothing is set for ilp32 here.
	 */
	t = current;
	saved = t->lkpi_uaccess_kernel;
	if (ctx->kernel)
		t->lkpi_uaccess_kernel = true;
	r = fn(f, lcmd, arg);
	t->lkpi_uaccess_kernel = saved;

	if (r < 0)
		return (lkpi_errno_to_user((int)r));
	*rvalp = r;
	return (0);
}

/*
 * read
 */
int
lkpi_file_read(struct lkpi_file *f, char __user *buf, size_t len,
    bool nonblock, size_t *done)
{
	ssize_t r;

	*done = 0;
	if (f->f_op->read == NULL)
		return (EINVAL);
	if (nonblock)
		f->f_flags |= O_NONBLOCK;
	else
		f->f_flags &= ~O_NONBLOCK;
	r = f->f_op->read(f, buf, len, &f->f_pos);
	if (r < 0)
		return (lkpi_errno_to_user((int)r));
	*done = (size_t)r;
	return (0);
}

/*
 * poll
 *
 * f_op->poll registers the wait queues it sleeps on with poll_wait(). A
 * Linux poll table entry lives only for one poll() call; an illumos poll
 * head stays attached to the file and is woken by pollwakeup(). So the
 * first time a queue is seen it gets a persistent custom-function wait
 * entry that forwards every wake_up*() on it to the file's poll head, and
 * it stays queued until the file is released. Wake-ups run with the queue
 * lock held, possibly in interrupt context; ph_wake defers the
 * pollwakeup(9F) (backend.h).
 */
static int
lkpi_file_wake_fn(struct wait_queue_entry *we, unsigned int mode, int flags,
    void *key)
{
	struct lkpi_file_pollreg *pr;
	__poll_t ev;

	pr = container_of(we, struct lkpi_file_pollreg, we);
	ev = key != NULL ? (__poll_t)(uintptr_t)key : ~(__poll_t)0;
	lkpi_glue_be->ph_wake(pr->f->f_lkpi_ph, ev);
	return (0);
}

static void
lkpi_file_qproc(struct lkpi_file *f, struct wait_queue_head *wq,
    poll_table *pt)
{
	struct lkpi_file_pollreg *pr;
	unsigned int i;

	mutex_lock(&f->f_lkpi_lock);
	for (i = 0; i < f->f_lkpi_npollreg; i++) {
		if (f->f_lkpi_pollreg[i].wq == wq) {
			mutex_unlock(&f->f_lkpi_lock);
			return;
		}
	}
	if (f->f_lkpi_npollreg == LKPI_FILE_MAX_POLLREG) {
		mutex_unlock(&f->f_lkpi_lock);
		WARN_ONCE(1, "drmglue: more than %d poll queues per file",
		    LKPI_FILE_MAX_POLLREG);
		return;
	}
	pr = &f->f_lkpi_pollreg[f->f_lkpi_npollreg++];
	pr->wq = wq;
	pr->f = f;
	init_waitqueue_func_entry(&pr->we, lkpi_file_wake_fn);
	add_wait_queue(wq, &pr->we);
	mutex_unlock(&f->f_lkpi_lock);
}

int
lkpi_file_poll(struct lkpi_file *f, __poll_t events, __poll_t *revents,
    void **php)
{
	poll_table pt;

	*php = f->f_lkpi_ph;
	if (f->f_op->poll == NULL) {
		/* Linux DEFAULT_POLLMASK */
		*revents = (LKPI_EPOLLIN | LKPI_EPOLLOUT | LKPI_EPOLLRDNORM |
		    LKPI_EPOLLWRNORM) & events;
		return (0);
	}
	pt._qproc = lkpi_file_qproc;
	pt._key = events | LKPI_EPOLLERR | LKPI_EPOLLHUP;
	*revents = f->f_op->poll(f, &pt) &
	    (events | LKPI_EPOLLERR | LKPI_EPOLLHUP | LKPI_EPOLLNVAL);
	return (0);
}

void
lkpi_file_poll_wake(struct lkpi_file *f, __poll_t events)
{
	lkpi_glue_be->ph_wake(f->f_lkpi_ph, events);
}
