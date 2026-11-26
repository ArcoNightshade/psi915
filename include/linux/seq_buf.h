/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Fixed-size string buffer.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SEQ_BUF_H_
#define	_LINUXKPI_INCLUDE_LINUX_SEQ_BUF_H_

#include <linux/types.h>
#include <linux/string.h>

struct seq_buf {
	char	*buffer;
	size_t	size;
	size_t	len;
};

#define	DECLARE_SEQ_BUF(name, sz)					\
	char name##_buffer[sz];						\
	struct seq_buf name = { name##_buffer, sz, 0 }

static inline void
seq_buf_init(struct seq_buf *s, char *buf, size_t size)
{
	s->buffer = buf;
	s->size = size;
	s->len = 0;
	if (size > 0)
		buf[0] = '\0';
}

int seq_buf_printf(struct seq_buf *s, const char *fmt, ...) __printf(2, 3);
#define	seq_buf_str(s)		((s)->buffer)
#define	seq_buf_used(s)		((s)->len < (s)->size ? (s)->len : (s)->size)
#define	seq_buf_has_overflowed(s) ((s)->len > (s)->size)

#endif /* _LINUXKPI_INCLUDE_LINUX_SEQ_BUF_H_ */
