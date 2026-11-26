/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. dma-buf. Clean-room declaration of the Linux 6.14 interface (member names and
 * order as upstream, because the GPL implementation in gpl/drivers/dma-buf
 * and drivers use them directly); the out-of-line functions live in the
 * separate GPL module (docs/08 5, docs/10 R10). Inline helpers are this
 * project's own, written from the documented behaviour.
 * The file behind a dma-buf is a KPI struct file (lkpi_file) backed by an
 * illumos anonymous vnode (drmglue).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_DMA_BUF_H_
#define	_LINUXKPI_INCLUDE_LINUX_DMA_BUF_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/iosys-map.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-fence.h>
#include <linux/dma-resv.h>
#include <linux/wait.h>
#include <linux/module.h>

struct device;
struct dma_buf;
struct dma_buf_attachment;
struct dma_buf_sysfs_entry;

struct dma_buf_ops {
	bool		cache_sgt_mapping;
	int		(*attach)(struct dma_buf *, struct dma_buf_attachment *);
	void		(*detach)(struct dma_buf *, struct dma_buf_attachment *);
	int		(*pin)(struct dma_buf_attachment *attach);
	void		(*unpin)(struct dma_buf_attachment *attach);
	struct sg_table	*(*map_dma_buf)(struct dma_buf_attachment *,
			    enum dma_data_direction);
	void		(*unmap_dma_buf)(struct dma_buf_attachment *,
			    struct sg_table *, enum dma_data_direction);
	void		(*release)(struct dma_buf *);
	int		(*begin_cpu_access)(struct dma_buf *,
			    enum dma_data_direction);
	int		(*end_cpu_access)(struct dma_buf *,
			    enum dma_data_direction);
	int		(*mmap)(struct dma_buf *, struct vm_area_struct *vma);
	int		(*vmap)(struct dma_buf *dmabuf, struct iosys_map *map);
	void		(*vunmap)(struct dma_buf *dmabuf, struct iosys_map *map);
};

struct dma_buf_poll_cb_t {
	struct dma_fence_cb	cb;
	wait_queue_head_t	*poll;
	__poll_t		active;
};

struct dma_buf {
	size_t				size;
	struct lkpi_file		*file;
	struct list_head		attachments;
	const struct dma_buf_ops	*ops;
	unsigned int			vmapping_counter;
	struct iosys_map		vmap_ptr;
	const char			*exp_name;
	const char			*name;
	spinlock_t			name_lock;
	struct lkpi_module		*owner;
	struct list_head		list_node;
	void				*priv;
	struct dma_resv			*resv;
	wait_queue_head_t		poll;
	struct dma_buf_poll_cb_t	cb_in, cb_out;
	struct dma_buf_sysfs_entry	*sysfs_entry;
};

struct dma_buf_attach_ops {
	bool	allow_peer2peer;
	void	(*move_notify)(struct dma_buf_attachment *attach);
};

struct dma_buf_attachment {
	struct dma_buf				*dmabuf;
	struct device				*dev;
	struct list_head			node;
	struct sg_table				*sgt;
	enum dma_data_direction			dir;
	bool					peer2peer;
	const struct dma_buf_attach_ops		*importer_ops;
	void					*importer_priv;
	void					*priv;
};

struct dma_buf_export_info {
	const char			*exp_name;
	struct lkpi_module		*owner;
	const struct dma_buf_ops	*ops;
	size_t				size;
	int				flags;
	struct dma_resv			*resv;
	void				*priv;
};

#define	DEFINE_DMA_BUF_EXPORT_INFO(name)				\
	struct dma_buf_export_info name = {				\
		.exp_name = KBUILD_MODNAME, .owner = THIS_MODULE }

static inline void
get_dma_buf(struct dma_buf *dmabuf)
{
	(void) get_file(dmabuf->file);
}

static inline bool
dma_buf_is_dynamic(struct dma_buf *dmabuf)
{
	return (dmabuf->ops->pin != NULL);
}

static inline bool
dma_buf_attachment_is_dynamic(struct dma_buf_attachment *attach)
{
	return (attach->importer_ops != NULL);
}

struct dma_buf_attachment *dma_buf_attach(struct dma_buf *dmabuf,
    struct device *dev);
struct dma_buf_attachment *dma_buf_dynamic_attach(struct dma_buf *dmabuf,
    struct device *dev, const struct dma_buf_attach_ops *importer_ops,
    void *importer_priv);
void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach);
int dma_buf_pin(struct dma_buf_attachment *attach);
void dma_buf_unpin(struct dma_buf_attachment *attach);
struct dma_buf *dma_buf_export(const struct dma_buf_export_info *exp_info);
int dma_buf_fd(struct dma_buf *dmabuf, int flags);
struct dma_buf *dma_buf_get(int fd);
void dma_buf_put(struct dma_buf *dmabuf);
struct sg_table *dma_buf_map_attachment(struct dma_buf_attachment *,
    enum dma_data_direction);
void dma_buf_unmap_attachment(struct dma_buf_attachment *, struct sg_table *,
    enum dma_data_direction);
struct sg_table *dma_buf_map_attachment_unlocked(struct dma_buf_attachment *,
    enum dma_data_direction);
void dma_buf_unmap_attachment_unlocked(struct dma_buf_attachment *,
    struct sg_table *, enum dma_data_direction);
void dma_buf_move_notify(struct dma_buf *dma_buf);
int dma_buf_begin_cpu_access(struct dma_buf *dma_buf,
    enum dma_data_direction dir);
int dma_buf_end_cpu_access(struct dma_buf *dma_buf,
    enum dma_data_direction dir);
int dma_buf_mmap(struct dma_buf *, struct vm_area_struct *, unsigned long);
int dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map);
void dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map);
int dma_buf_vmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map);
void dma_buf_vunmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map);

#endif /* _LINUXKPI_INCLUDE_LINUX_DMA_BUF_H_ */
