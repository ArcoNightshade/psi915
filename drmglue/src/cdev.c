/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * /dev/dri node registry and clone-open table (docs/10, section 2.1).
 *
 * Every open(9E) of a node gets a fresh clone id and its own dev_t, so
 * illumos calls close(9E) once per open file description (after the last
 * dup/fork copy is closed and the last mapping is gone, specfs defers it),
 * which is exactly when Linux calls f_op->release. The clone table maps
 * (node, clone id) back to the Linux struct file for the other entry
 * points. Precedent: illumos/gfx-drm drm_sunmod.c (drm_sun_open) uses the
 * same clone scheme with a 9-bit minor; the layout here differs (glue.h).
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/glue.h>

struct lkpi_node {
	struct lkpi_node_desc	d;
	char			name[32];
	struct inode		*inode;
	struct mutex		lock;		/* clones, nopen */
	struct idr		clones;
	unsigned int		nopen;
};

static DEFINE_MUTEX(lkpi_nodes_lock);
static struct lkpi_node *lkpi_nodes[LKPI_NODE_MAX + 1];

static struct lkpi_node *
lkpi_node_get(unsigned int node)
{
	struct lkpi_node *n;

	if (node > LKPI_NODE_MAX)
		return (NULL);
	mutex_lock(&lkpi_nodes_lock);
	n = lkpi_nodes[node];
	mutex_unlock(&lkpi_nodes_lock);
	return (n);
}

int
lkpi_node_register(const struct lkpi_node_desc *d)
{
	struct lkpi_node *n;

	if (d->node > LKPI_NODE_MAX || d->fops == NULL)
		return (-EINVAL);
	n = kzalloc(sizeof (*n), GFP_KERNEL);
	if (n == NULL)
		return (-ENOMEM);
	n->inode = lkpi_inode_alloc(LKPI_MKDEV(LKPI_DRM_MAJOR, d->node),
	    0020666 /* S_IFCHR */);
	if (n->inode == NULL) {
		kfree(n);
		return (-ENOMEM);
	}
	n->d = *d;
	strscpy(n->name, d->name != NULL ? d->name : "", sizeof (n->name));
	n->d.name = n->name;
	mutex_init(&n->lock);
	idr_init_base(&n->clones, 1);

	mutex_lock(&lkpi_nodes_lock);
	if (lkpi_nodes[d->node] != NULL) {
		mutex_unlock(&lkpi_nodes_lock);
		lkpi_inode_put(n->inode);
		idr_destroy(&n->clones);
		mutex_destroy(&n->lock);
		kfree(n);
		return (-EEXIST);
	}
	lkpi_nodes[d->node] = n;
	mutex_unlock(&lkpi_nodes_lock);
	return (0);
}

int
lkpi_node_unregister(unsigned int node)
{
	struct lkpi_node *n;

	if (node > LKPI_NODE_MAX)
		return (-EINVAL);
	mutex_lock(&lkpi_nodes_lock);
	n = lkpi_nodes[node];
	if (n == NULL) {
		mutex_unlock(&lkpi_nodes_lock);
		return (-ENOENT);
	}
	mutex_lock(&n->lock);
	if (n->nopen != 0) {
		mutex_unlock(&n->lock);
		mutex_unlock(&lkpi_nodes_lock);
		return (-EBUSY);
	}
	mutex_unlock(&n->lock);
	lkpi_nodes[node] = NULL;
	mutex_unlock(&lkpi_nodes_lock);

	/* Files still alive (mapped after close) keep their inode ref. */
	lkpi_inode_put(n->inode);
	idr_destroy(&n->clones);
	mutex_destroy(&n->lock);
	kfree(n);
	return (0);
}

void *
lkpi_node_plat(unsigned int node)
{
	struct lkpi_node *n = lkpi_node_get(node);

	return (n != NULL ? n->d.plat : NULL);
}

uint32_t
(*lkpi_node_kcmd(unsigned int node))(struct lkpi_file *, unsigned int,
    unsigned int)
{
	struct lkpi_node *n = lkpi_node_get(node);

	return (n != NULL ? n->d.kcmd : NULL);
}

unsigned int
lkpi_node_open_count(unsigned int node)
{
	struct lkpi_node *n = lkpi_node_get(node);
	unsigned int c = 0;

	if (n != NULL) {
		mutex_lock(&n->lock);
		c = n->nopen;
		mutex_unlock(&n->lock);
	}
	return (c);
}

int
lkpi_cdev_open(unsigned int node, unsigned int oflags, fmode_t mode,
    uint32_t *clone)
{
	struct lkpi_node *n;
	struct lkpi_file *f;
	int r;

	n = lkpi_node_get(node);
	if (n == NULL)
		return (ENXIO);
	f = lkpi_file_alloc(LKPI_FILE_CDEV, n->d.fops, n->inode, oflags, mode);
	if (f == NULL)
		return (ENOMEM);
	atomic_inc(&n->inode->i_count);
	f->f_lkpi_own_inode = n->inode;

	if (f->f_op->open != NULL) {
		r = f->f_op->open(n->inode, f);
		if (r != 0) {
			/* Linux: no release after a failed open. */
			lkpi_file_free(f);
			return (lkpi_errno_to_user(r));
		}
	}

	mutex_lock(&n->lock);
	r = idr_alloc(&n->clones, f, 1, (int)LKPI_CLONE_MAX + 1, GFP_KERNEL);
	if (r < 0) {
		mutex_unlock(&n->lock);
		fput(f);
		return (r == -ENOSPC ? EMFILE : lkpi_errno_to_user(r));
	}
	n->nopen++;
	mutex_unlock(&n->lock);
	*clone = (uint32_t)r;
	return (0);
}

int
lkpi_cdev_close(unsigned int node, uint32_t clone)
{
	struct lkpi_node *n;
	struct lkpi_file *f;

	n = lkpi_node_get(node);
	if (n == NULL)
		return (ENXIO);
	mutex_lock(&n->lock);
	f = idr_remove(&n->clones, clone);
	if (f != NULL)
		n->nopen--;
	mutex_unlock(&n->lock);
	if (f == NULL)
		return (EBADF);
	fput(f);
	return (0);
}

struct lkpi_file *
lkpi_cdev_file(unsigned int node, uint32_t clone)
{
	struct lkpi_node *n;
	struct lkpi_file *f;

	n = lkpi_node_get(node);
	if (n == NULL || clone == 0)
		return (NULL);
	mutex_lock(&n->lock);
	f = idr_find(&n->clones, clone);
	f = get_file_active(&f);
	mutex_unlock(&n->lock);
	return (f);
}
