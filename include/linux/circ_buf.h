/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Circular-buffer index arithmetic (size is a power of two).
 */
#ifndef _LINUXKPI_LINUX_CIRC_BUF_H_
#define	_LINUXKPI_LINUX_CIRC_BUF_H_

struct circ_buf {
	char	*buf;
	int	head;
	int	tail;
};

#define	CIRC_CNT(head, tail, size)	(((head) - (tail)) & ((size) - 1))
#define	CIRC_SPACE(head, tail, size)	CIRC_CNT((tail), ((head) + 1), (size))
#define	CIRC_CNT_TO_END(head, tail, size) ({				\
	int __end = (size) - (tail);					\
	int __n = ((head) + __end) & ((size) - 1);			\
	__n < __end ? __n : __end; })
#define	CIRC_SPACE_TO_END(head, tail, size) ({				\
	int __end = (size) - 1 - (head);				\
	int __n = (__end + (tail)) & ((size) - 1);			\
	__n <= __end ? __n : __end + 1; })

#endif /* _LINUXKPI_LINUX_CIRC_BUF_H_ */
