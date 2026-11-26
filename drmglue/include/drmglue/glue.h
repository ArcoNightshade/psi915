/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Portable entry-point dispatch of the DRM glue: what an illumos cb_ops
 * routine or anonymous-file vnode operation does after decoding its DDI
 * arguments (src/file.c, src/cdev.c). Return values are positive illumos
 * errno values (0 = success); Linux return values are translated here.
 */
#ifndef _DRMGLUE_GLUE_H_
#define	_DRMGLUE_GLUE_H_

#include <linux/types.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/xlate.h>

/* Per-call ioctl context. */
struct lkpi_ioctl_ctx {
	enum lkpi_ioc_abi	abi;
	bool			ilp32;		/* caller is a 32-bit process */
	bool			refuse_ilp32;	/* policy: EINVAL for ilp32 */
	bool			kernel;		/* FKIOCTL: arg is a kernel address */
	bool			nonblock;	/* FNONBLOCK|FNDELAY on the file */
	/* Kernel definition of (type, nr), or 0 (see lkpi_ioc_translate). */
	uint32_t		(*kcmd)(struct lkpi_file *f, unsigned int type,
				    unsigned int nr);
};

int lkpi_file_ioctl(struct lkpi_file *f, uint32_t ucmd, unsigned long arg,
    const struct lkpi_ioctl_ctx *ctx, long *rvalp);

/* One user buffer of a read(2); *done gets the byte count. */
int lkpi_file_read(struct lkpi_file *f, char __user *buf, size_t len,
    bool nonblock, size_t *done);

/*
 * chpoll / VOP_POLL. events and *revents are Linux EPOLL* bits; *php gets
 * the backend poll head of the file (always; the caller applies the
 * illumos rules about when to return it).
 */
int lkpi_file_poll(struct lkpi_file *f, __poll_t events, __poll_t *revents,
    void **php);

/* Wake pollers of f (for fops that keep their own state, e.g. sync_file). */
void lkpi_file_poll_wake(struct lkpi_file *f, __poll_t events);

/*
 * /dev/dri nodes and clone opens (src/cdev.c).
 *
 * dev_t minor layout (docs/10, section 2.1):
 *   bits 0-7   node: the Linux DRM minor (0-63 card, 128-191 renderD)
 *   bits 8-31  clone id; 0 is the node itself, opens get 1, 2, ...
 * Clone ids up to LKPI_CLONE_MAX32 keep the minor within the 18 bits a
 * 32-bit process's stat(2) can represent; they are handed out lowest
 * first, so only more than 1023 simultaneous opens of one node produce
 * minors that make fstat() fail with EOVERFLOW in 32-bit processes.
 * Node 0xff is reserved for the anonymous-file mmap host (anonfile.c).
 */
#define	LKPI_NODE_BITS		8
#define	LKPI_NODE_MASK		0xffU
#define	LKPI_CLONE_SHIFT	8
#define	LKPI_CLONE_MAX		0xffffffU	/* 24 bits */
#define	LKPI_CLONE_MAX32	0x3ffU		/* (L_MAXMIN32 >> 8) */
#define	LKPI_NODE_ANON		0xffU
#define	LKPI_NODE_MAX		0xfeU
#define	LKPI_DRM_MAJOR		226		/* Linux DRM_MAJOR, for i_rdev */

static inline uint32_t
lkpi_minor_make(unsigned int node, uint32_t clone)
{
	return ((clone << LKPI_CLONE_SHIFT) | (node & LKPI_NODE_MASK));
}

static inline unsigned int
lkpi_minor_node(uint32_t minor)
{
	return (minor & LKPI_NODE_MASK);
}

static inline uint32_t
lkpi_minor_clone(uint32_t minor)
{
	return (minor >> LKPI_CLONE_SHIFT);
}

struct lkpi_node_desc {
	unsigned int			node;	/* Linux DRM minor */
	const char			*name;	/* "card0", "renderD128" */
	const struct file_operations	*fops;	/* drm_stub_fops or driver's */
	uint32_t			(*kcmd)(struct lkpi_file *, unsigned int,
					    unsigned int);
	void				*plat;	/* backend: dev_info_t * */
};

int lkpi_node_register(const struct lkpi_node_desc *d);
/* -EBUSY while clones are open. */
int lkpi_node_unregister(unsigned int node);
void *lkpi_node_plat(unsigned int node);
uint32_t (*lkpi_node_kcmd(unsigned int node))(struct lkpi_file *,
    unsigned int, unsigned int);
unsigned int lkpi_node_open_count(unsigned int node);

/*
 * open(9E): new clone of node with Linux open flags and fmode; returns the
 * clone id in *clone. The file's f_op->open runs here.
 */
int lkpi_cdev_open(unsigned int node, unsigned int oflags, fmode_t mode,
    uint32_t *clone);
/* close(9E): drop the open reference (release runs on the last one). */
int lkpi_cdev_close(unsigned int node, uint32_t clone);
/* Referenced file of an open clone, or NULL. Drop with fput(). */
struct lkpi_file *lkpi_cdev_file(unsigned int node, uint32_t clone);

/*
 * The anonymous-file mmap host (anonfile.c): anonymous files are mmapped
 * through segdev using dev_t (host major, LKPI_NODE_ANON, id). The id is
 * assigned on first mmap and released with the file.
 */
int lkpi_anon_mmap_id(struct lkpi_file *f, uint32_t *id);
struct lkpi_file *lkpi_anon_mmap_file(uint32_t id);

#endif /* _DRMGLUE_GLUE_H_ */
