/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. File mode bits (POSIX values, as illumos <sys/stat.h>).
 */
#ifndef _LINUXKPI_LINUX_STAT_H_
#define	_LINUXKPI_LINUX_STAT_H_

#include <linux/types.h>

#ifndef S_IRWXU
#define	S_IRWXU		00700
#define	S_IRUSR		00400
#define	S_IWUSR		00200
#define	S_IXUSR		00100
#define	S_IRWXG		00070
#define	S_IRGRP		00040
#define	S_IWGRP		00020
#define	S_IXGRP		00010
#define	S_IRWXO		00007
#define	S_IROTH		00004
#define	S_IWOTH		00002
#define	S_IXOTH		00001
#endif
#define	S_IRUGO		(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWUGO		(S_IWUSR | S_IWGRP | S_IWOTH)
#define	S_IXUGO		(S_IXUSR | S_IXGRP | S_IXOTH)
#define	S_IRWXUGO	(S_IRWXU | S_IRWXG | S_IRWXO)

#endif /* _LINUXKPI_LINUX_STAT_H_ */
