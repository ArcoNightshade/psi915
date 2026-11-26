/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Linux file-layer types: struct file (renamed lkpi_file by
 * <linuxkpi/platform.h>, illumos has its own), inode, address_space,
 * path, file_operations, poll_table, kdev_t encoding. Member names follow
 * upstream Linux 6.14; only the members DRM core, i915, dma-buf, sync_file
 * and drm_syncobj use exist. The f_lkpi_* members are DRM glue state
 * (drmglue, docs/10), which also implements fput(), the anonymous-inode
 * files and the pseudo file system; moved here from drmglue (docs/10 R1)
 * so there is one definition.
 *
 * illumos <sys/file.h> must be included before any linux/ header by code
 * that needs file_t (see platform.h).
 */
#ifndef _LINUXKPI_LINUX_FS_H_
#define	_LINUXKPI_LINUX_FS_H_

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/stat.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linuxkpi/itree.h>

#include <linux/fcntl.h>
#include <linux/capability.h>
#include <linux/uidgid.h>

typedef unsigned int	fmode_t;
typedef void		*fl_owner_t;

/* Linux kdev_t: 12-bit major, 20-bit minor (in-kernel encoding). */
#define	LKPI_MINORBITS		20
#define	LKPI_MINORMASK		((1U << LKPI_MINORBITS) - 1)
#define	LKPI_MKDEV(ma, mi)	(((uint32_t)(ma) << LKPI_MINORBITS) | (mi))
#define	LKPI_MAJOR(d)		((uint32_t)(d) >> LKPI_MINORBITS)
#define	LKPI_MINOR(d)		((uint32_t)(d) & LKPI_MINORMASK)

#define	FMODE_READ		0x1U
#define	FMODE_WRITE		0x2U
#define	FMODE_LSEEK		0x4U
#define	FMODE_PREAD		0x8U
#define	FMODE_PWRITE		0x10U
#define	FMODE_UNSIGNED_OFFSET	0x2000U

struct lkpi_file;
struct vm_area_struct;
struct seq_file;
struct lkpi_module;
struct poll_table_struct;

/*
 * The Linux mapping object of an inode. Only the mmap registry is
 * emulated: i_mmap holds every live user mapping (struct lkpi_vma) of the
 * file, keyed by page offset, so unmap_mapping_range() can find them
 * (docs/10, section 3.5).
 */
struct writeback_control;
struct lkpi_page;
struct folio;
struct address_space;

/* Page-cache operations (shmem backing store; see docs/04). */
struct address_space_operations {
	int	(*writepage)(struct lkpi_page *page,
		    struct writeback_control *wbc);
	bool	(*dirty_folio)(struct address_space *mapping, void *folio);
	int	(*write_begin)(struct lkpi_file *file,
		    struct address_space *mapping, long long pos,
		    unsigned int len, struct folio **foliop, void **fsdata);
	int	(*write_end)(struct lkpi_file *file,
		    struct address_space *mapping, long long pos,
		    unsigned int len, unsigned int copied, struct folio *folio,
		    void *fsdata);
};

struct address_space {
	struct inode		*host;
	const struct address_space_operations *a_ops;
	struct rw_semaphore	i_mmap_rwsem;
	struct lkpi_itree	i_mmap;
};

struct super_block;
struct dentry;
struct vfsmount;

/*
 * Member names follow upstream Linux 6.14 so the GPL dma-buf.c,
 * sync_file.c and the MIT drm_syncobj.c compile unchanged; the layout
 * (order, padding) is the glue's own and is never relied on by them.
 */
struct inode {
	unsigned long		i_ino;
	uint32_t		i_rdev;		/* LKPI_MKDEV encoding */
	umode_t			i_mode;
	loff_t			i_size;
	loff_t			i_lkpi_bytes;	/* inode_set_bytes() */
	atomic_t		i_count;
	void			*i_private;
	struct super_block	*i_sb;
	struct address_space	*i_mapping;
	struct address_space	i_data;
};

struct path {
	struct vfsmount		*mnt;
	struct dentry		*dentry;
};

typedef unsigned int fop_flags_t;
#define	FOP_BUFFER_RASYNC	((fop_flags_t)1 << 0)
#define	FOP_BUFFER_WASYNC	((fop_flags_t)1 << 1)
#define	FOP_MMAP_SYNC		((fop_flags_t)1 << 2)
#define	FOP_DIO_PARALLEL_WRITE	((fop_flags_t)1 << 3)
#define	FOP_HUGE_PAGES		((fop_flags_t)1 << 4)
#define	FOP_UNSIGNED_OFFSET	((fop_flags_t)1 << 5)

struct file_operations {
	struct lkpi_module	*owner;
	fop_flags_t	fop_flags;
	loff_t		(*llseek)(struct lkpi_file *, loff_t, int);
	ssize_t		(*read)(struct lkpi_file *, char __user *, size_t,
			    loff_t *);
	ssize_t		(*write)(struct lkpi_file *, const char __user *,
			    size_t, loff_t *);
	__poll_t	(*poll)(struct lkpi_file *, struct poll_table_struct *);
	long		(*unlocked_ioctl)(struct lkpi_file *, unsigned int,
			    unsigned long);
	long		(*compat_ioctl)(struct lkpi_file *, unsigned int,
			    unsigned long);
	int		(*mmap)(struct lkpi_file *, struct vm_area_struct *);
	int		(*open)(struct inode *, struct lkpi_file *);
	int		(*flush)(struct lkpi_file *, fl_owner_t);
	int		(*release)(struct inode *, struct lkpi_file *);
	int		(*fasync)(int, struct lkpi_file *, int);
	void		(*show_fdinfo)(struct seq_file *, struct lkpi_file *);
};

/*
 * Wait queues a file's f_op->poll passed to poll_wait(). Each gets one
 * custom-function entry (KPI <linux/wait.h>) that forwards wake_up*() to
 * the file's poll head; entries stay queued until the file is released
 * (illumos poll heads are persistent, unlike Linux poll tables). See
 * lkpi_file_poll() in src/file.c.
 */
#define	LKPI_FILE_MAX_POLLREG	4

struct lkpi_file_pollreg {
	struct wait_queue_head	*wq;
	struct wait_queue_entry	we;
	struct lkpi_file	*f;
};

/* Pending mmap(2) parameters, valid during segmap -> devmap (docs/10 3.3). */
struct lkpi_mmap_req {
	unsigned int		prot;		/* PROT_* of the request */
	unsigned int		maxprot;
	unsigned int		flags;		/* MAP_* */
	struct lkpi_vma		*lv;		/* created by devmap(9E) */
};

enum lkpi_file_kind {
	LKPI_FILE_CDEV = 1,	/* clone open of a /dev/dri node */
	LKPI_FILE_ANON = 2	/* anon_inode_getfile(): dma-buf, sync_file, ... */
};

struct lkpi_file {
	const struct file_operations	*f_op;
	void				*private_data;
	struct path			f_path;		/* alloc_file_pseudo */
	struct inode			*f_inode;
	struct address_space		*f_mapping;
	unsigned int			f_flags;	/* O_* */
	fmode_t				f_mode;
	loff_t				f_pos;
	atomic_long_t			f_count;

	/* psi915 glue state; never touched by Linux code. */
	enum lkpi_file_kind		f_lkpi_kind;
	struct mutex			f_lkpi_lock;	/* vnode, pollreg */
	struct mutex			f_lkpi_mmap_lock; /* one mmap at a time */
	struct lkpi_mmap_req		*f_lkpi_mmap;
	void				*f_lkpi_vnode;	/* anon: backend vnode */
	void				*f_lkpi_ph;	/* backend pollhead */
	unsigned int			f_lkpi_npollreg;
	struct lkpi_file_pollreg	f_lkpi_pollreg[LKPI_FILE_MAX_POLLREG];
	uint32_t			f_lkpi_mmap_id;	/* anon mmap host id */
	void				*f_lkpi_mmap_vp; /* backend: host snode */
	struct inode			*f_lkpi_own_inode; /* freed with file */
	struct work_struct		f_lkpi_free_work; /* deferred release */
};

static inline long
file_count(struct lkpi_file *f)
{
	return (atomic_long_read(&f->f_count));
}

typedef struct poll_table_struct {
	void		(*_qproc)(struct lkpi_file *, struct wait_queue_head *,
			    struct poll_table_struct *);
	__poll_t	_key;
} poll_table;

static inline void
poll_wait(struct lkpi_file *f, struct wait_queue_head *wq, poll_table *p)
{
	if (p != NULL && p->_qproc != NULL && wq != NULL)
		p->_qproc(f, wq, p);
}

static inline bool
poll_does_not_wait(const poll_table *p)
{
	return (p == NULL || p->_qproc == NULL);
}

static inline __poll_t
poll_requested_events(const poll_table *p)
{
	return (p != NULL ? p->_key : ~(__poll_t)0);
}

static inline struct inode *
file_inode(const struct lkpi_file *f)
{
	return (f->f_inode);
}

static inline unsigned int
iminor(const struct inode *ip)
{
	return (LKPI_MINOR(ip->i_rdev));
}

static inline unsigned int
imajor(const struct inode *ip)
{
	return (LKPI_MAJOR(ip->i_rdev));
}

static inline loff_t
i_size_read(const struct inode *ip)
{
	return (ip->i_size);
}

static inline void
i_size_write(struct inode *ip, loff_t sz)
{
	ip->i_size = sz;
}

static inline struct lkpi_file *
get_file(struct lkpi_file *f)
{
	atomic_long_inc(&f->f_count);
	return (f);
}

/* Takes a reference unless the count already dropped to zero. */
static inline struct lkpi_file *
get_file_active(struct lkpi_file **fp)
{
	struct lkpi_file *f = *fp;

	if (f != NULL && !atomic_long_inc_not_zero(&f->f_count))
		f = NULL;
	return (f);
}

/* Drops a reference; the last one calls f_op->release (file.c). */
void fput(struct lkpi_file *f);

static inline int
nonseekable_open(struct inode *ip, struct lkpi_file *f)
{
	f->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return (0);
}

static inline int
stream_open(struct inode *ip, struct lkpi_file *f)
{
	return (nonseekable_open(ip, f));
}

/* No 32-bit compat ioctls (docs/09 5.9). */
#define	compat_ptr_ioctl	NULL

/*
 * Simple attribute files (debugfs/sysfs u64 get/set): there is no
 * debugfs, so opening one fails with -ENODEV; get, set and fmt are only
 * referenced.
 */
#define	DEFINE_SIMPLE_ATTRIBUTE(__fops, __get, __set, __fmt)		\
static int								\
__fops ## _lkpi_open(struct inode *__i, struct lkpi_file *__f)		\
{									\
	(void) __i;							\
	(void) __f;							\
	(void) (__get);							\
	(void) (__set);							\
	(void) (__fmt);							\
	return (-ENODEV);						\
}									\
static const struct file_operations __fops = {				\
	.open = __fops ## _lkpi_open,					\
}
#define	DEFINE_SIMPLE_ATTRIBUTE_SIGNED(f, g, s, fmt)			\
	DEFINE_SIMPLE_ATTRIBUTE(f, g, s, fmt)

/*
 * Character device majors: the illumos character device of DRM is
 * drmglue's (cb_ops), so Linux's DRM_MAJOR registration succeeds without
 * effect.
 */
#define	register_chrdev(maj, name, fops)	\
	((void)(maj), (void)(name), (void)(fops), 0)
#define	unregister_chrdev(maj, name)	do { (void)(maj); } while (0)

/* Calls the file's mmap (dma-buf mmap forwarding). */
#define	call_mmap(file, vma)	((file)->f_op->mmap((file), (vma)))

/* f_op reference helpers (no module reference counts). */
#define	fops_get(f)		(f)
#define	fops_put(f)		do { (void)(f); } while (0)
#define	replace_fops(file, fops)	((file)->f_op = (fops))

/*
 * simple_pin_fs/simple_release_fs: a counted kern_mount() of a pseudo
 * file system (DRM's anonymous inodes); src/linux_device.c.
 */
struct file_system_type;
int simple_pin_fs(struct file_system_type *type, struct vfsmount **mount,
    int *count);
void simple_release_fs(struct vfsmount **mount, int *count);

#define	MAX_NON_LFS		((1UL << 31) - 1)
#define	MAX_LFS_FILESIZE	((loff_t)LLONG_MAX)

#ifndef SEEK_SET
#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2
#endif

loff_t noop_llseek(struct lkpi_file *f, loff_t off, int whence);
loff_t no_llseek(struct lkpi_file *f, loff_t off, int whence);

/*
 * Pseudo file systems (anonymous inodes and files of dma-buf, sync_file
 * and DRM); moved from drmglue (docs/10, R1). Implemented in
 * drmglue/src/pseudofs.c.
 */
struct fs_context;
struct super_operations;

struct super_block {
	unsigned long		s_magic;
	const struct dentry_operations *s_d_op;
	const struct super_operations *s_op;
	struct file_system_type	*s_type;
	atomic_long_t		s_lkpi_next_ino;
};

#define	FS_REQUIRES_DEV		1
#define	FS_USERNS_MOUNT		8

struct file_system_type {
	const char	*name;
	int		fs_flags;
	int		(*init_fs_context)(struct fs_context *);
	void		(*kill_sb)(struct super_block *);
	struct lkpi_module *owner;
};

void kill_anon_super(struct super_block *sb);
struct inode *alloc_anon_inode(struct super_block *sb);
void iput(struct inode *ip);
void ihold(struct inode *ip);
void inode_set_bytes(struct inode *ip, loff_t bytes);
struct lkpi_file *alloc_file_pseudo(struct inode *ip, struct vfsmount *mnt,
    const char *name, int flags, const struct file_operations *fops);

/* As in Linux, fs.h provides the dentry types. */
#include <linux/dcache.h>

#endif /* _LINUXKPI_LINUX_FS_H_ */
