/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * printk, dev_printk, WARN and BUG slow paths (see <linux/printk.h>,
 * <linux/bug.h>, <linux/dev_printk.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/dev_printk.h>
#include <linux/panic.h>
#include <linux/errno.h>

/* Messages at or above this LOGLEVEL_* are dropped (8: keep all). */
int linuxkpi_loglevel = 8;

/* Nonzero: BUG() panics. See <linux/bug.h>. */
#ifdef LINUXKPI_USERLAND
int linuxkpi_bug_panic = 1;
#else
int linuxkpi_bug_panic = 0;
#endif

unsigned int linuxkpi_warn_count;
unsigned int linuxkpi_bug_count;

void (*lkpi_printk_hook)(int level, const char *msg);

/* src/linux_device.c */
const char *lkpi_device_name(const struct device *dev);

static const char *
lkpi_dev_name_default(const struct device *dev)
{
	return (dev == NULL ? "(NULL device)" : lkpi_device_name(dev));
}

const char *(*lkpi_dev_name_hook)(const struct device *dev) =
    lkpi_dev_name_default;

static int
lkpi_plat_level(int level)
{
	if (level < 0)
		return (LKPI_PLAT_LOG_CONT);
	if (level <= LOGLEVEL_WARNING)
		return (LKPI_PLAT_LOG_WARN);
	if (level == LOGLEVEL_NOTICE)
		return (LKPI_PLAT_LOG_NOTE);
	if (level == LOGLEVEL_INFO)
		return (LKPI_PLAT_LOG_INFO);
	return (LKPI_PLAT_LOG_DEBUG);
}

/* Emit one formatted message at a LOGLEVEL_* level. */
static void
lkpi_emit(int level, char *buf, size_t len)
{
	int pl;

	if (level >= linuxkpi_loglevel)
		return;
	if (lkpi_printk_hook != NULL) {
		lkpi_printk_hook(level, buf);
		return;
	}
	pl = lkpi_plat_level(level);
	/* cmn_err adds a newline for CE_WARN/CE_NOTE; drop ours. */
	if (LKPI_PLAT_LOG_ADDS_NEWLINE(pl) && len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	lkpi_plat_log(pl, buf);
}

/*
 * Format "<prefix><fmt>" and emit it. The level comes from levelstr if
 * given, else from a KERN_* prefix of fmt. Returns the formatted length.
 */
static int
lkpi_vprintk_lp(const char *levelstr, const char *prefix, const char *fmt,
    va_list ap)
{
	char buf[LKPI_PRINTK_BUFSZ];
	size_t off = 0, n;
	int level = LOGLEVEL_DEFAULT;
	int lc;

	lc = printk_get_level(levelstr != NULL ? levelstr : fmt);
	if (lc >= '0' && lc <= '7')
		level = lc - '0';
	fmt = printk_skip_level(fmt);

	if (prefix != NULL) {
		n = strlen(prefix);
		if (n >= sizeof (buf))
			n = sizeof (buf) - 1;
		memcpy(buf, prefix, n);
		off = n;
	}
	buf[off] = '\0';
	n = (size_t)lkpi_plat_vsnprintf(buf + off, sizeof (buf) - off, fmt, ap);
	if (off + n >= sizeof (buf)) {
		/* Truncated: mark it, keeping a trailing newline. */
		memcpy(buf + sizeof (buf) - 5, "...\n", 5);
		n = sizeof (buf) - 1 - off;
	}
	lkpi_emit(level, buf, off + n);
	return ((int)(off + n));
}

int
lkpi_vprintk(const char *fmt, va_list ap)
{
	return (lkpi_vprintk_lp(NULL, NULL, fmt, ap));
}

int
lkpi_printk(const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = lkpi_vprintk_lp(NULL, NULL, fmt, ap);
	va_end(ap);
	return (r);
}

static void
lkpi_dev_vprintk(const char *level, const struct device *dev,
    const char *fmt, va_list ap)
{
	char prefix[96];

	(void) snprintf(prefix, sizeof (prefix), "%s: ",
	    lkpi_dev_name_hook(dev));
	(void) lkpi_vprintk_lp(level, prefix, fmt, ap);
}

void
lkpi_dev_printk(const char *level, const struct device *dev,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	lkpi_dev_vprintk(level, dev, fmt, ap);
	va_end(ap);
}

int
lkpi_dev_err_probe(const struct device *dev, int err, const char *fmt, ...)
{
	char prefix[96];
	va_list ap;

	(void) snprintf(prefix, sizeof (prefix), "%s: error %d: ",
	    lkpi_dev_name_hook(dev), err);
	va_start(ap, fmt);
	(void) lkpi_vprintk_lp(err == -EPROBE_DEFER ? KERN_DEBUG : KERN_ERR,
	    prefix, fmt, ap);
	va_end(ap);
	return (err);
}

void
lkpi_warn_slowpath(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	char prefix[128];
	va_list ap;

	__atomic_fetch_add(&linuxkpi_warn_count, 1, __ATOMIC_RELAXED);
	(void) snprintf(prefix, sizeof (prefix), "WARNING: %s:%d %s()%s",
	    file, line, func, fmt != NULL ? ": " : "");
	if (fmt == NULL) {
		(void) lkpi_printk(KERN_WARNING "%s\n", prefix);
		return;
	}
	va_start(ap, fmt);
	(void) lkpi_vprintk_lp(KERN_WARNING, prefix, fmt, ap);
	va_end(ap);
}

void
lkpi_bug(const char *file, int line, const char *func)
{
	char msg[128];

	__atomic_fetch_add(&linuxkpi_bug_count, 1, __ATOMIC_RELAXED);
	(void) snprintf(msg, sizeof (msg), "BUG: %s:%d %s()", file, line,
	    func);
	if (linuxkpi_bug_panic)
		lkpi_plat_panic(msg);
	(void) lkpi_printk(KERN_CRIT "%s (linuxkpi_bug_panic=0, continuing)\n",
	    msg);
}

void
dump_stack(void)
{
#ifdef LINUXKPI_ILLUMOS_KERNEL
	(void) lkpi_printk(KERN_DEBUG
	    "linuxkpi: dump_stack: no unwinder; use DTrace stack() or mdb\n");
#endif
}

#ifdef LINUXKPI_USERLAND
void
lkpi_panic(const char *fmt, ...)
{
	char buf[LKPI_PRINTK_BUFSZ];
	va_list ap;

	va_start(ap, fmt);
	(void) vsnprintf(buf, sizeof (buf), fmt, ap);
	va_end(ap);
	lkpi_plat_panic(buf);
}
#endif
