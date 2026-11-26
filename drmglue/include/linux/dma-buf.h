/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux dma-buf kernel API (6.x), clean-room from the documented
 * behaviour (Documentation/driver-api/dma-buf.rst and the kernel-doc of
 * the API). Linux's own include/linux/dma-buf.h and drivers/dma-buf/
 * dma-buf.c are GPL-2.0-only and are not used; drm-kmod's BSD dma-buf.c
 * is built on FreeBSD file objects and is replaced by drmglue/src/dma_buf.c
 * (docs/10, section 4 and requested change R9).
 *
 * The dma-buf file is a glue anonymous file (anon_inode_getfile), so a
 * dma-buf fd is an ordinary illumos descriptor: passable with SCM_RIGHTS,
 * mmap-able through the anon mmap host, lseek(SEEK_END) gives the size.
 */
#ifndef _LINUXKPI_LINUX_DMA_BUF_H_
#define	_LINUXKPI_LINUX_DMA_BUF_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>

struct device;
struct sg_table;
struct dma_resv;
struct iosys_map;
struct module;
struct dma_buf;
struct dma_buf_attachment;

#ifndef _LINUXKPI_LINUX_DMA_DIRECTION_H_
/* Values of Linux enum dma_data_direction (an in-kernel ABI). */
enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3
};
#endif

#define	DMA_BUF_NAME_LEN	32

struct dma_buf_ops {
	bool		cache_sgt_mapping;
	int		(*attach)(struct dma_buf *, struct dma_buf_attachment *);
	void		(*detach)(struct dma_buf *,
			    struct dma_buf_attachment *);
	int		(*pin)(struct dma_buf_attachment *);
	void		(*unpin)(struct dma_buf_attachment *);
	struct sg_table	*(*map_dma_buf)(struct dma_buf_attachment *,
			    enum dma_data_direction);
	void		(*unmap_dma_buf)(struct dma_buf_attachment *,
			    struct sg_table *, enum dma_data_direction);
	void		(*release)(struct dma_buf *);
	int		(*begin_cpu_access)(struct dma_buf *,
			    enum dma_data_direction);
	int		(*end_cpu_access)(struct dma_buf *,
			    enum dma_data_direction);
	int		(*mmap)(struct dma_buf *, struct vm_area_struct *);
	int		(*vmap)(struct dma_buf *, struct iosys_map *);
	void		(*vunmap)(struct dma_buf *, struct iosys_map *);
};

struct dma_buf {
	size_t			size;
	struct lkpi_file	*file;
	struct list_head	attachments;
	const struct dma_buf_ops *ops;
	struct mutex		lock;		/* attachments, vmap */
	unsigned int		vmapping_counter;
	const char		*exp_name;
	const char		*name;		/* SET_NAME, under name_lock */
	spinlock_t		name_lock;
	struct module		*owner;
	void			*priv;
	struct dma_resv		*resv;
	bool			lkpi_own_resv;
};

struct dma_buf_attach_ops {
	bool	allow_peer2peer;
	void	(*move_notify)(struct dma_buf_attachment *);
};

struct dma_buf_attachment {
	struct dma_buf		*dmabuf;
	struct device		*dev;
	struct list_head	node;
	struct sg_table		*sgt;
	enum dma_data_direction	dir;
	bool			peer2peer;
	const struct dma_buf_attach_ops *importer_ops;
	void			*importer_priv;
	void			*priv;
};

struct dma_buf_export_info {
	const char		*exp_name;
	struct module		*owner;
	const struct dma_buf_ops *ops;
	size_t			size;
	int			flags;
	struct dma_resv		*resv;
	void			*priv;
};

#define	DEFINE_DMA_BUF_EXPORT_INFO(name)				\
	struct dma_buf_export_info name = { .exp_name = "drmglue",	\
	    .owner = NULL }

static inline void
get_dma_buf(struct dma_buf *db)
{
	(void) get_file(db->file);
}

struct dma_buf *dma_buf_export(const struct dma_buf_export_info *exp);
int dma_buf_fd(struct dma_buf *db, int flags);
struct dma_buf *dma_buf_get(int fd);
void dma_buf_put(struct dma_buf *db);
bool lkpi_is_dma_buf_file(struct lkpi_file *f);

struct dma_buf_attachment *dma_buf_attach(struct dma_buf *db,
    struct device *dev);
struct dma_buf_attachment *dma_buf_dynamic_attach(struct dma_buf *db,
    struct device *dev, const struct dma_buf_attach_ops *ops, void *priv);
void dma_buf_detach(struct dma_buf *db, struct dma_buf_attachment *a);
int dma_buf_pin(struct dma_buf_attachment *a);
void dma_buf_unpin(struct dma_buf_attachment *a);
struct sg_table *dma_buf_map_attachment(struct dma_buf_attachment *a,
    enum dma_data_direction dir);
void dma_buf_unmap_attachment(struct dma_buf_attachment *a,
    struct sg_table *sgt, enum dma_data_direction dir);
#define	dma_buf_map_attachment_unlocked(a, d)	dma_buf_map_attachment(a, d)
#define	dma_buf_unmap_attachment_unlocked(a, s, d) \
	dma_buf_unmap_attachment(a, s, d)
int dma_buf_begin_cpu_access(struct dma_buf *db, enum dma_data_direction d);
int dma_buf_end_cpu_access(struct dma_buf *db, enum dma_data_direction d);
int dma_buf_mmap(struct dma_buf *db, struct vm_area_struct *vma,
    unsigned long pgoff);
int dma_buf_vmap(struct dma_buf *db, struct iosys_map *map);
void dma_buf_vunmap(struct dma_buf *db, struct iosys_map *map);
#define	dma_buf_vmap_unlocked(d, m)	dma_buf_vmap(d, m)
#define	dma_buf_vunmap_unlocked(d, m)	dma_buf_vunmap(d, m)

/*
 * dma_resv lifetime for exporters that pass resv == NULL (DRM GEM always
 * passes its object's resv). REQUESTED CHANGE R8: the KPI/dma-resv owner
 * provides these two; drmglue only declares them.
 */
struct dma_resv *lkpi_dma_resv_alloc(void);
void lkpi_dma_resv_free(struct dma_resv *resv);

#endif /* _LINUXKPI_LINUX_DMA_BUF_H_ */
