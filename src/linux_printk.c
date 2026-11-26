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
	n = (size_t)lkpi_vsnprintf(buf + off, sizeof (buf) - off, fmt, ap);
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

/*
 * Linux printf format extensions (see <linux/sprintf.h>). Conversions are
 * parsed one at a time; ordinary ones are formatted by the platform
 * vsnprintf with their own argument, the %p extensions here:
 *
 *   %pV          struct va_format: nested format and va_list
 *   %pe          error pointer: "-<errno>" (no symbolic names)
 *   %ph[CDN]     up to 64 bytes in hex (with precision/width as length)
 *   %pa[p|d]     phys_addr_t / dma_addr_t by reference
 *   %p4cc        DRM fourcc code by reference
 *   %pR, %pr     struct resource
 *   %pS/%ps/%pB/%pK/%px/%pF/%pf and bare %p   the pointer value in hex
 *
 * Other %p<letter> extensions print the pointer and consume the letters
 * Linux would (alphanumerics after the p).
 */
#include <linux/ioport.h>
#include <linux/slab.h>

static int
lkpi_raw_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (n);
}

struct lkpi_fmtbuf {
	char	*buf;
	size_t	size;
	size_t	len;	/* would-be length */
};

static void
lkpi_fb_put(struct lkpi_fmtbuf *b, const char *s, size_t n)
{
	size_t room;

	if (b->len < b->size) {
		room = b->size - b->len - 1;
		memcpy(b->buf + b->len, s, n < room ? n : room);
		b->buf[b->len + (n < room ? n : room)] = '\0';
	}
	b->len += n;
}

static void
lkpi_fb_printf(struct lkpi_fmtbuf *b, const char *fmt, ...)
{
	char tmp[128];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_plat_vsnprintf(tmp, sizeof (tmp), fmt, ap);
	va_end(ap);
	if (n > 0)
		lkpi_fb_put(b, tmp, (size_t)n < sizeof (tmp) ? (size_t)n :
		    sizeof (tmp) - 1);
}

static void lkpi_vformat(struct lkpi_fmtbuf *b, const char *fmt, va_list *ap);

static const char *
lkpi_fmt_pointer(struct lkpi_fmtbuf *b, const char *f, void *p, int prec)
{
	static const char hex[] = "0123456789abcdef";

	switch (*f) {
	case 'V': {
		struct va_format *vaf = p;
		va_list aq;

		va_copy(aq, *vaf->va);
		lkpi_vformat(b, vaf->fmt, &aq);
		va_end(aq);
		return (f + 1);
	}
	case 'e':
		lkpi_fb_printf(b, "%ld", (long)(intptr_t)p);
		return (f + 1);
	case 'h': {
		char sep = ' ';
		int n = prec > 0 ? prec : 1, i;
		const unsigned char *d = p;

		f++;
		if (*f == 'C' || *f == 'D' || *f == 'N') {
			sep = *f == 'C' ? ':' : (*f == 'D' ? '-' : '\0');
			f++;
		}
		if (n > 64)
			n = 64;
		for (i = 0; i < n; i++) {
			char x[3] = { hex[d[i] >> 4], hex[d[i] & 15], sep };

			lkpi_fb_put(b, x, (i + 1 < n && sep != '\0') ? 3 : 2);
		}
		return (f);
	}
	case 'a':
		f++;
		if (*f == 'd') {
			lkpi_fb_printf(b, "0x%016llx",
			    (unsigned long long)*(dma_addr_t *)p);
			return (f + 1);
		}
		if (*f == 'p')
			f++;
		lkpi_fb_printf(b, "0x%016llx",
		    (unsigned long long)*(phys_addr_t *)p);
		return (f);
	case '4':
		if (f[1] == 'c' && f[2] == 'c') {
			u32 v = *(u32 *)p;
			char s[5];
			int i;

			for (i = 0; i < 4; i++) {
				char c = (char)((v >> (i * 8)) & 0x7f);

				s[i] = c >= 32 && c < 127 ? c : '.';
			}
			s[4] = '\0';
			lkpi_fb_printf(b, "%s %s-endian (0x%08x)", s,
			    (v & (1U << 31)) ? "big" : "little", v);
			return (f + 3);
		}
		break;
	case 'R':
	case 'r': {
		struct resource *r = p;

		lkpi_fb_printf(b, "[mem %#010llx-%#010llx]",
		    (unsigned long long)r->start, (unsigned long long)r->end);
		return (f + 1);
	}
	default:
		break;
	}
	lkpi_fb_printf(b, "%p", p);
	while ((*f >= 'a' && *f <= 'z') || (*f >= 'A' && *f <= 'Z') ||
	    (*f >= '0' && *f <= '9'))
		f++;
	return (f);
}

static void
lkpi_vformat(struct lkpi_fmtbuf *b, const char *fmt, va_list *ap)
{
	const char *f = fmt, *start;
	char spec[32];
	int lng, prec;

	while (*f != '\0') {
		if (*f != '%') {
			start = f;
			while (*f != '\0' && *f != '%')
				f++;
			lkpi_fb_put(b, start, (size_t)(f - start));
			continue;
		}
		start = f++;
		if (*f == '%') {
			lkpi_fb_put(b, "%", 1);
			f++;
			continue;
		}
		/* flags, width, precision */
		while (*f == '-' || *f == '+' || *f == ' ' || *f == '#' ||
		    *f == '0')
			f++;
		{
			int width = -1;

			if (*f == '*') {
				width = va_arg(*ap, int);
				f++;
			} else if (*f >= '0' && *f <= '9') {
				width = 0;
				while (*f >= '0' && *f <= '9')
					width = width * 10 + (*f++ - '0');
			}
			prec = -1;
			if (*f == '.') {
				f++;
				if (*f == '*') {
					prec = va_arg(*ap, int);
					f++;
				} else {
					prec = 0;
					while (*f >= '0' && *f <= '9')
						prec = prec * 10 + (*f++ - '0');
				}
			}
			/* length */
			lng = 0;
			while (*f == 'l' || *f == 'h' || *f == 'z' || *f == 'j' ||
			    *f == 't' || *f == 'L' || *f == 'q') {
				if (*f == 'h')
					lng = lng == -1 ? -2 : -1;
				else if (*f == 'l' || *f == 'z' || *f == 'j' ||
				    *f == 't')
					lng++;
				else
					lng = 2;
				f++;
			}
			if (*f == 'p') {
				void *p = va_arg(*ap, void *);

				if (prec < 0 && width > 0)
					prec = width;
				f = lkpi_fmt_pointer(b, f + 1, p, prec);
				continue;
			}
			if (*f == '\0')
				break;
			/* Rebuild the spec without '*' (values consumed). */
			{
				size_t n = 0;
				const char *s;

				for (s = start; s <= f && n + 12 < sizeof (spec); s++) {
					if (*s == '*') {
						n += (size_t)lkpi_raw_snprintf(spec + n,
						    sizeof (spec) - n, "%d",
						    s > start && s[-1] == '.' ? prec :
						    width);
						continue;
					}
					spec[n++] = *s;
				}
				spec[n] = '\0';
			}
		}
		switch (*f) {
		case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
		case 'c':
			if (lng >= 2)
				lkpi_fb_printf(b, spec, va_arg(*ap, long long));
			else if (lng == 1)
				lkpi_fb_printf(b, spec, va_arg(*ap, long));
			else
				lkpi_fb_printf(b, spec, va_arg(*ap, int));
			break;
		case 's': {
			const char *s = va_arg(*ap, const char *);
			char big[512];
			int n;

			if (spec[1] == 's') {	/* plain %s: no length limit */
				if (s == NULL)
					s = "(null)";
				lkpi_fb_put(b, s, strlen(s));
				break;
			}
			n = lkpi_raw_snprintf(big, sizeof (big), spec,
			    s != NULL ? s : "(null)");

			lkpi_fb_put(b, big, n < 0 ? 0 : ((size_t)n < sizeof (big) ?
			    (size_t)n : sizeof (big) - 1));
			break;
		}
		case 'n':
			(void) va_arg(*ap, void *);
			break;
		default:
			/* Floating point is not used in the kernel. */
			lkpi_fb_put(b, start, (size_t)(f + 1 - start));
			break;
		}
		f++;
	}
}

int
lkpi_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct lkpi_fmtbuf b = { buf, size, 0 };
	va_list aq;

	if (size > 0)
		buf[0] = '\0';
	va_copy(aq, ap);
	lkpi_vformat(&b, fmt, &aq);
	va_end(aq);
	return ((int)b.len);
}

int
lkpi_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (n);
}

int
lkpi_vscnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	int n = lkpi_vsnprintf(buf, size, fmt, ap);

	if (size == 0)
		return (0);
	return ((size_t)n < size ? n : (int)size - 1);
}

int
lkpi_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_vscnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (n);
}

int
lkpi_sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = lkpi_vsnprintf(buf, (size_t)INT_MAX, fmt, ap);
	va_end(ap);
	return (n);
}

char *
kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	va_list aq;
	char *p;
	int n;

	va_copy(aq, ap);
	n = lkpi_vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);
	p = kmalloc((size_t)n + 1, gfp);
	if (p != NULL)
		(void) lkpi_vsnprintf(p, (size_t)n + 1, fmt, ap);
	return (p);
}

char *
kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);
	return (p);
}

const char *
kvasprintf_const(gfp_t gfp, const char *fmt, va_list ap)
{
	return (kvasprintf(gfp, fmt, ap));
}
