/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No relay channels (i915 GuC log relay is unused on Gen9).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_RELAY_H_
#define	_LINUXKPI_INCLUDE_LINUX_RELAY_H_

#include <linux/types.h>

struct rchan_buf;

/* Never allocated (relay_open fails); members for type checks only. */
struct rchan {
	size_t			subbuf_size;
	size_t			n_subbufs;
	size_t			alloc_size;
	const struct rchan_callbacks *cb;
	void			*private_data;
	int			is_global;
	struct rchan_buf	**buf;
};
struct dentry;
struct file_operations;

/*
 * relay channels (GuC log relay, a debugfs feature): not supported,
 * relay_open() fails (NULL) and the other calls accept NULL.
 */
struct rchan_callbacks {
	int	(*subbuf_start)(struct rchan_buf *, void *, void *, size_t);
	struct dentry *(*create_buf_file)(const char *filename,
		    struct dentry *parent, umode_t mode,
		    struct rchan_buf *buf, int *is_global);
	int	(*remove_buf_file)(struct dentry *dentry);
};
extern const struct file_operations relay_file_operations;
#define	relay_buf_full(b)		((void)(b), 0)
#define	relay_write(c, d, l)		do { (void)(c); } while (0)
#define	relay_open(n, p, s, c, cb, pd)	((struct rchan *)NULL)
#define	relay_close(c)			do { (void)(c); } while (0)
#define	relay_flush(c)			do { (void)(c); } while (0)
#define	relay_reserve(c, l)		((void *)NULL)

#endif /* _LINUXKPI_INCLUDE_LINUX_RELAY_H_ */
