/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. seq_file output buffer. With no debugfs/procfs on illumos
 * no file ever reads through it; seq_printf() and friends format into a
 * caller-provided buffer so drivers' show functions can be called by
 * other KPI code (for example a future kstat or ioctl exporter).
 */
#ifndef _LINUXKPI_LINUX_SEQ_FILE_H_
#define	_LINUXKPI_LINUX_SEQ_FILE_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/string.h>

struct lkpi_file;
struct inode;

struct seq_file {
	char		*buf;
	size_t		size;
	size_t		count;
	void		*private;
	long long	index;
};

struct seq_operations {
	void	*(*start)(struct seq_file *m, long long *pos);
	void	(*stop)(struct seq_file *m, void *v);
	void	*(*next)(struct seq_file *m, void *v, long long *pos);
	int	(*show)(struct seq_file *m, void *v);
};

void seq_printf(struct seq_file *m, const char *fmt, ...) __printf(2, 3);
void seq_puts(struct seq_file *m, const char *s);
void seq_putc(struct seq_file *m, char c);
void seq_write(struct seq_file *m, const void *data, size_t len);
#define	seq_has_overflowed(m)	((m)->count >= (m)->size)

static inline void
lkpi_seq_init(struct seq_file *m, char *buf, size_t size)
{
	memset(m, 0, sizeof (*m));
	m->buf = buf;
	m->size = size;
	if (size > 0)
		buf[0] = '\0';
}

/* No file is created; keep the show function referenced. */
#define	DEFINE_SHOW_ATTRIBUTE(name)					\
	static inline void *__maybe_unused name##_lkpi_show_ref(void)	\
	{ return ((void *)(uintptr_t)name##_show); }
#define	DEFINE_SEQ_ATTRIBUTE(name)					\
	static inline void *__maybe_unused name##_lkpi_ops_ref(void)	\
	{ return ((void *)&name##_sops); }
#define	single_open(f, show, data)	((void)(show), -ENODEV)
#define	single_release(i, f)		(0)
#define	seq_read			NULL
#define	seq_lseek			NULL

#endif /* _LINUXKPI_LINUX_SEQ_FILE_H_ */
