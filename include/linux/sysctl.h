/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. sysctl tables: there is no /proc/sys on illumos, so
 * register_sysctl() accepts a table and returns a handle without
 * publishing anything (the values keep their defaults; i915 exposes the
 * same knobs as module parameters).
 */
#ifndef _LINUXKPI_LINUX_SYSCTL_H_
#define	_LINUXKPI_LINUX_SYSCTL_H_

#include <linux/types.h>
#include <linux/errno.h>

struct ctl_table;
typedef int proc_handler(const struct ctl_table *table, int write,
    void *buffer, size_t *lenp, long long *ppos);

struct ctl_table {
	const char	*procname;
	void		*data;
	int		maxlen;
	umode_t		mode;
	proc_handler	*proc_handler;
	void		*extra1;
	void		*extra2;
};

struct ctl_table_header {
	const struct ctl_table	*lkpi_table;
};

extern const int lkpi_sysctl_vals[];
#define	SYSCTL_ZERO		((void *)&lkpi_sysctl_vals[0])
#define	SYSCTL_ONE		((void *)&lkpi_sysctl_vals[1])
#define	SYSCTL_INT_MAX		((void *)&lkpi_sysctl_vals[2])

int proc_dointvec(const struct ctl_table *table, int write, void *buffer,
    size_t *lenp, long long *ppos);
int proc_dointvec_minmax(const struct ctl_table *table, int write,
    void *buffer, size_t *lenp, long long *ppos);
struct ctl_table_header *register_sysctl(const char *path,
    const struct ctl_table *table);
void unregister_sysctl_table(struct ctl_table_header *h);

#endif /* _LINUXKPI_LINUX_SYSCTL_H_ */
