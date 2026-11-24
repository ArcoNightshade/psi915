/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * printk API; macro set cross-checked against FreeBSD's LinuxKPI printk.h
 * and kernel.h (BSD-2-Clause).
 *
 * printk() formats into a bounded buffer (LKPI_PRINTK_BUFSZ, longer
 * messages are truncated) and hands the text to the platform log:
 *
 *   KERN_EMERG..KERN_WARNING  cmn_err(CE_WARN)        console + log
 *   KERN_NOTICE               cmn_err(CE_NOTE)        console + log
 *   KERN_INFO                 cmn_err(CE_CONT, "?")   log; console if -v boot
 *   KERN_DEBUG                cmn_err(CE_CONT, "!")   log only
 *   no level, KERN_CONT       cmn_err(CE_CONT)        console + log
 *
 * Userland builds write to stderr. Formatting is done by the environment's
 * vsnprintf, so the Linux %p extensions (%pe, %pK, %pS, %ph, ...) are NOT
 * interpreted: they print the pointer followed by the extension letters.
 *
 * Messages at or above the level in linuxkpi_loglevel (default 8: all)
 * are dropped. pr_debug()/pr_devel() compile to nothing unless DEBUG is
 * defined; note that illumos DEBUG kernel builds define DEBUG.
 */
#ifndef _LINUXKPI_LINUX_PRINTK_H_
#define	_LINUXKPI_LINUX_PRINTK_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stdarg.h>
#include <linux/ratelimit.h>

#define	KERN_SOH	"\001"
#define	KERN_SOH_ASCII	'\001'
#define	KERN_EMERG	KERN_SOH "0"
#define	KERN_ALERT	KERN_SOH "1"
#define	KERN_CRIT	KERN_SOH "2"
#define	KERN_ERR	KERN_SOH "3"
#define	KERN_WARNING	KERN_SOH "4"
#define	KERN_NOTICE	KERN_SOH "5"
#define	KERN_INFO	KERN_SOH "6"
#define	KERN_DEBUG	KERN_SOH "7"
#define	KERN_DEFAULT	""
#define	KERN_CONT	KERN_SOH "c"

#define	LOGLEVEL_SCHED		(-2)
#define	LOGLEVEL_DEFAULT	(-1)
#define	LOGLEVEL_EMERG		0
#define	LOGLEVEL_ALERT		1
#define	LOGLEVEL_CRIT		2
#define	LOGLEVEL_ERR		3
#define	LOGLEVEL_WARNING	4
#define	LOGLEVEL_NOTICE		5
#define	LOGLEVEL_INFO		6
#define	LOGLEVEL_DEBUG		7

#define	LKPI_PRINTK_BUFSZ	512

/* Tunables and hooks (src/linux_printk.c). */
extern int linuxkpi_loglevel;
/*
 * If set, receives every formatted message (level LOGLEVEL_*, or
 * LOGLEVEL_DEFAULT; 'c' continuation is passed as LOGLEVEL_DEFAULT) instead
 * of the platform log. Used by tests and the kernel test module.
 */
extern void (*lkpi_printk_hook)(int level, const char *msg);

int lkpi_printk(const char *fmt, ...) __printf(1, 2);
int lkpi_vprintk(const char *fmt, va_list ap) __printf(1, 0);

/* Level character of a KERN_* prefixed string, or 0. */
static inline int
printk_get_level(const char *buf)
{
	if (buf[0] == KERN_SOH_ASCII && buf[1] != '\0') {
		switch (buf[1]) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case 'c':
			return (buf[1]);
		}
	}
	return (0);
}

static inline const char *
printk_skip_level(const char *buf)
{
	return (printk_get_level(buf) != 0 ? buf + 2 : buf);
}

#define	printk(...)		lkpi_printk(__VA_ARGS__)
#define	vprintk(fmt, ap)	lkpi_vprintk(fmt, ap)

/* Type-checks the arguments, prints nothing. */
#define	no_printk(fmt, ...) ({						\
	if (0)								\
		lkpi_printk(fmt, ##__VA_ARGS__);			\
	0;								\
})

#ifndef pr_fmt
#define	pr_fmt(fmt)	fmt
#endif

#define	pr_emerg(fmt, ...)	printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_alert(fmt, ...)	printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_crit(fmt, ...)	printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_err(fmt, ...)	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_warn(fmt, ...)	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_warning(fmt, ...)	pr_warn(fmt, ##__VA_ARGS__)
#define	pr_notice(fmt, ...)	printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_info(fmt, ...)	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_cont(fmt, ...)	printk(KERN_CONT fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define	pr_debug(fmt, ...)	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_devel(fmt, ...)	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define	pr_debug(fmt, ...)	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_devel(fmt, ...)	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* Print once per call site. */
#define	printk_once(fmt, ...) ({					\
	static bool __lkpi_printed_once;				\
	bool __lkpi_po_first = !__lkpi_printed_once;			\
									\
	if (unlikely(__lkpi_po_first)) {				\
		__lkpi_printed_once = true;				\
		printk(fmt, ##__VA_ARGS__);				\
	}								\
	__lkpi_po_first;						\
})

#define	pr_emerg_once(fmt, ...)	printk_once(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_alert_once(fmt, ...)	printk_once(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_crit_once(fmt, ...)	printk_once(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_err_once(fmt, ...)	printk_once(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_warn_once(fmt, ...)	printk_once(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_notice_once(fmt, ...) printk_once(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_info_once(fmt, ...)	printk_once(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#ifdef DEBUG
#define	pr_debug_once(fmt, ...)	printk_once(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define	pr_debug_once(fmt, ...)	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* Print at most DEFAULT_RATELIMIT_BURST times per interval per site. */
#define	printk_ratelimited(fmt, ...) ({					\
	static struct ratelimit_state __lkpi_rs =			\
	    RATELIMIT_STATE_INIT(__lkpi_rs, 0, DEFAULT_RATELIMIT_BURST);	\
									\
	if (__ratelimit(&__lkpi_rs))					\
		printk(fmt, ##__VA_ARGS__);				\
})

#define	pr_emerg_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_alert_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_crit_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_err_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_warn_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_notice_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define	pr_info_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#ifdef DEBUG
#define	pr_debug_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define	pr_debug_ratelimited(fmt, ...)					\
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

#define	printk_ratelimit()	(1)

/*
 * No stack unwinder is exposed to modules on illumos: dump_stack() prints
 * a pointer to mdb/DTrace instead (kernel) or nothing (userland).
 */
void dump_stack(void);

#endif /* _LINUXKPI_LINUX_PRINTK_H_ */
