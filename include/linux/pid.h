/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Reference-counted process identifiers. One struct pid
 * exists per id number in use (a global table, src/linux_sched.c);
 * task_tgid() and get_task_pid() look it up or create it. task_tgid()
 * returns it without a reference, as in Linux; get_pid()/put_pid() count.
 */
#ifndef _LINUXKPI_LINUX_PID_H_
#define	_LINUXKPI_LINUX_PID_H_

#include <linux/types.h>
#include <linux/refcount.h>
#include <linux/list.h>

enum pid_type {
	PIDTYPE_PID,
	PIDTYPE_TGID,
	PIDTYPE_PGID,
	PIDTYPE_SID,
	PIDTYPE_MAX
};

struct pid {
	unsigned int		count;		/* under the table lock */
	pid_t			nr;
	struct list_head	lkpi_link;
};

struct task_struct;

struct pid *get_pid(struct pid *pid);
void put_pid(struct pid *pid);
struct pid *get_task_pid(struct task_struct *task, enum pid_type type);
struct pid *task_tgid(struct task_struct *task);
struct pid *task_pid(struct task_struct *task);
#define	pid_nr(p)		((p) != NULL ? (p)->nr : 0)
#define	pid_vnr(p)		pid_nr(p)

#endif /* _LINUXKPI_LINUX_PID_H_ */
