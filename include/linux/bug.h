/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * WARN/BUG API; cross-checked against FreeBSD's LinuxKPI kernel.h and
 * bug.h (BSD-2-Clause).
 *
 * WARN*() print "WARNING: file:line func()" (plus the message) at
 * KERN_WARNING, count the event in linuxkpi_warn_count, and evaluate to
 * the condition, as in Linux.
 *
 * BUG()/BUG_ON() print "BUG: file:line func()" and count the event in
 * linuxkpi_bug_count. Then:
 *   linuxkpi_bug_panic != 0   panic (illumos kernel: crash dump; userland:
 *                             abort())
 *   linuxkpi_bug_panic == 0   return to the caller
 * The tunable defaults to 0 in the kernel, so a BUG() is survivable while
 * the port is being brought up; set it in /etc/system with
 *   set linuxkpi:linuxkpi_bug_panic = 1
 * or with mdb -kw. In userland it defaults to 1. Because BUG() can return,
 * it is not __noreturn; code after BUG() still runs when the tunable is 0.
 */
#ifndef _LINUXKPI_LINUX_BUG_H_
#define	_LINUXKPI_LINUX_BUG_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/build_bug.h>
#include <linux/printk.h>

extern int linuxkpi_bug_panic;
extern unsigned int linuxkpi_warn_count;
extern unsigned int linuxkpi_bug_count;

void lkpi_warn_slowpath(const char *file, int line, const char *func,
    const char *fmt, ...) __printf(4, 5);
void lkpi_bug(const char *file, int line, const char *func);

#define	BUG()		lkpi_bug(__FILE__, __LINE__, __func__)

#define	BUG_ON(cond) do {						\
	if (unlikely(cond))						\
		BUG();							\
} while (0)

#define	WARN_ON(cond) ({						\
	bool __lkpi_wo = !!(cond);					\
									\
	if (unlikely(__lkpi_wo))					\
		lkpi_warn_slowpath(__FILE__, __LINE__, __func__, NULL);	\
	unlikely(__lkpi_wo);						\
})

#define	WARN(cond, fmt, ...) ({						\
	bool __lkpi_wf = !!(cond);					\
									\
	if (unlikely(__lkpi_wf))					\
		lkpi_warn_slowpath(__FILE__, __LINE__, __func__,	\
		    fmt, ##__VA_ARGS__);				\
	unlikely(__lkpi_wf);						\
})

#define	WARN_ON_ONCE(cond) ({						\
	static bool __lkpi_woo_done;					\
	bool __lkpi_woo = !!(cond);					\
									\
	if (unlikely(__lkpi_woo) && !__lkpi_woo_done) {			\
		__lkpi_woo_done = true;					\
		lkpi_warn_slowpath(__FILE__, __LINE__, __func__, NULL);	\
	}								\
	unlikely(__lkpi_woo);						\
})

#define	WARN_ONCE(cond, fmt, ...) ({					\
	static bool __lkpi_wfo_done;					\
	bool __lkpi_wfo = !!(cond);					\
									\
	if (unlikely(__lkpi_wfo) && !__lkpi_wfo_done) {			\
		__lkpi_wfo_done = true;					\
		lkpi_warn_slowpath(__FILE__, __LINE__, __func__,	\
		    fmt, ##__VA_ARGS__);				\
	}								\
	unlikely(__lkpi_wfo);						\
})

#define	WARN_TAINT(cond, taint, fmt, ...)	WARN(cond, fmt, ##__VA_ARGS__)
#define	WARN_TAINT_ONCE(cond, taint, fmt, ...)	WARN_ONCE(cond, fmt, ##__VA_ARGS__)
#define	WARN_ON_SMP(cond)	WARN_ON(cond)

/* Kernel taint flags have no illumos meaning. */
#define	TAINT_WARN		9
#define	TAINT_USER		6
#define	LOCKDEP_STILL_OK	0
#define	LOCKDEP_NOW_UNRELIABLE	1
#define	add_taint(flag, lockdep_ok)	do { } while (0)

#endif /* _LINUXKPI_LINUX_BUG_H_ */
