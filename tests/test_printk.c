/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for printk, pr_* and dev_* logging, WARN/BUG, <linux/err.h>,
 * <linux/jiffies.h> and <linux/ratelimit.h>.
 */

#define	pr_fmt(fmt)	"tp: " fmt

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>

#include "harness.h"

/* Capture printk output through lkpi_printk_hook. */
static char last_msg[LKPI_PRINTK_BUFSZ];
static int last_level;
static int nmsgs;

static void
capture(int level, const char *msg)
{
	size_t i;

	for (i = 0; i + 1 < sizeof (last_msg) && msg[i] != '\0'; i++)
		last_msg[i] = msg[i];
	last_msg[i] = '\0';
	last_level = level;
	nmsgs++;
}

static bool
streq(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return (*a == *b);
}

static bool
starts_with(const char *s, const char *prefix)
{
	while (*prefix != '\0' && *s == *prefix) {
		s++;
		prefix++;
	}
	return (*prefix == '\0');
}

static size_t
slen(const char *s)
{
	size_t n = 0;

	while (s[n] != '\0')
		n++;
	return (n);
}

static void
begin(void)
{
	lkpi_printk_hook = capture;
	nmsgs = 0;
	last_msg[0] = '\0';
	last_level = -100;
}

static void
end(void)
{
	lkpi_printk_hook = NULL;
}

TEST(levels)
{
	begin();
	CHECK_EQ(printk_get_level(KERN_ERR "x"), '3');
	CHECK_EQ(printk_get_level("x"), 0);
	CHECK(streq(printk_skip_level(KERN_INFO "abc"), "abc"));

	pr_err("e%d\n", 1);
	CHECK(streq(last_msg, "tp: e1\n"));
	CHECK_EQ(last_level, LOGLEVEL_ERR);
	pr_warn("w\n");
	CHECK_EQ(last_level, LOGLEVEL_WARNING);
	pr_notice("n\n");
	CHECK_EQ(last_level, LOGLEVEL_NOTICE);
	pr_info("i\n");
	CHECK_EQ(last_level, LOGLEVEL_INFO);
	pr_emerg("x\n");
	CHECK_EQ(last_level, LOGLEVEL_EMERG);
	printk("plain %s\n", "text");
	CHECK(streq(last_msg, "plain text\n"));
	CHECK_EQ(last_level, LOGLEVEL_DEFAULT);
	pr_cont("more\n");
	CHECK(streq(last_msg, "more\n"));
	CHECK_EQ(last_level, LOGLEVEL_DEFAULT);
	CHECK_EQ(printk(KERN_INFO "%d", 12345), 5);
	CHECK_EQ(nmsgs, 8);

	/* pr_debug is compiled out without DEBUG but type-checks. */
	pr_debug("dbg %d\n", 1);
	CHECK_EQ(nmsgs, 8);
	end();
}

TEST(loglevel_filter)
{
	begin();
	linuxkpi_loglevel = LOGLEVEL_INFO;
	pr_info("dropped\n");
	CHECK_EQ(nmsgs, 0);
	pr_notice("kept\n");
	CHECK_EQ(nmsgs, 1);
	linuxkpi_loglevel = 8;
	end();
}

TEST(truncation)
{
	char big[LKPI_PRINTK_BUFSZ * 2];
	size_t i;

	for (i = 0; i < sizeof (big) - 1; i++)
		big[i] = 'a' + (i % 26);
	big[sizeof (big) - 1] = '\0';
	begin();
	printk(KERN_ERR "%s", big);
	CHECK_EQ(slen(last_msg), LKPI_PRINTK_BUFSZ - 1);
	CHECK(streq(last_msg + LKPI_PRINTK_BUFSZ - 5, "...\n"));
	end();
}

static int
print_once_helper(void)
{
	return (pr_info_once("once\n") ? 1 : 0);
}

TEST(once_and_ratelimit)
{
	int i, firsts = 0;

	begin();
	for (i = 0; i < 5; i++)
		firsts += print_once_helper();
	CHECK_EQ(nmsgs, 1);
	CHECK_EQ(firsts, 1);

	nmsgs = 0;
	for (i = 0; i < 50; i++)
		pr_err_ratelimited("rl %d\n", i);
	CHECK_EQ(nmsgs, DEFAULT_RATELIMIT_BURST);
	end();
}

TEST(ratelimit_state)
{
	DEFINE_RATELIMIT_STATE(rs, 1000000, 3);
	struct ratelimit_state off = RATELIMIT_STATE_INIT_DISABLED;
	struct ratelimit_state zero;
	int i, ok = 0;

	for (i = 0; i < 10; i++)
		ok += __ratelimit(&rs);
	CHECK_EQ(ok, 3);
	CHECK_EQ(rs.missed, 7);
	CHECK(__ratelimit(&off));
	ratelimit_state_init(&zero, HZ, 0);
	CHECK(!__ratelimit(&zero));
}

#include <linux/device.h>

TEST(dev_printk)
{
	struct device d = { .init_name = "dev0" };

	begin();
	dev_err((struct device *)NULL, "bad %d\n", 7);
	CHECK(streq(last_msg, "(NULL device): bad 7\n"));
	CHECK_EQ(last_level, LOGLEVEL_ERR);
	dev_info(&d, "hello\n");
	CHECK(streq(last_msg, "dev0: hello\n"));
	CHECK_EQ(last_level, LOGLEVEL_INFO);
	for (int i = 0; i < 3; i++)
		dev_warn_once((struct device *)NULL, "w\n");	/* one site */
	CHECK_EQ(nmsgs, 3);
	CHECK_EQ(dev_err_probe((struct device *)NULL, -EINVAL, "p\n"), -EINVAL);
	CHECK(starts_with(last_msg, "(NULL device): error "));
	CHECK_EQ(last_level, LOGLEVEL_ERR);
	CHECK_EQ(dev_err_probe((struct device *)NULL, -EPROBE_DEFER, "d\n"),
	    -EPROBE_DEFER);
	CHECK_EQ(last_level, LOGLEVEL_DEBUG);
	dev_dbg((struct device *)NULL, "nothing\n");
	CHECK_EQ(nmsgs, 5);
	end();
}

static int
warn_once_helper(int v)
{
	return (WARN_ON_ONCE(v != 0));
}

TEST(warn)
{
	unsigned int before = linuxkpi_warn_count;
	int i, hits = 0;

	begin();
	CHECK(!WARN_ON(0));
	CHECK_EQ(nmsgs, 0);
	CHECK(WARN_ON(1 + 1 == 2));
	CHECK_EQ(nmsgs, 1);
	CHECK(starts_with(last_msg, "WARNING: "));
	CHECK_EQ(last_level, LOGLEVEL_WARNING);
	CHECK(WARN(1, "value %d\n", 42));
	CHECK(starts_with(last_msg, "WARNING: "));
	CHECK(streq(last_msg + slen(last_msg) - 10, ": value 42" "\n") ||
	    streq(last_msg + slen(last_msg) - 9, "value 42\n"));
	CHECK(!WARN(0, "never\n"));
	for (i = 0; i < 4; i++)
		hits += warn_once_helper(1);
	CHECK_EQ(hits, 4);		/* still evaluates to the condition */
	CHECK_EQ(nmsgs, 3);		/* but prints once */
	CHECK(WARN_ONCE(1, "x\n"));
	CHECK_EQ(linuxkpi_warn_count - before, 4);
	end();
}

TEST(bug_survivable)
{
	unsigned int before = linuxkpi_bug_count;
	int saved = linuxkpi_bug_panic;

	begin();
	linuxkpi_bug_panic = 0;
	BUG_ON(0);
	CHECK_EQ(linuxkpi_bug_count, before);
	BUG_ON(1);
	CHECK_EQ(linuxkpi_bug_count, before + 1);
	CHECK(starts_with(last_msg, "BUG: "));
	CHECK_EQ(last_level, LOGLEVEL_CRIT);
	BUG();
	CHECK_EQ(linuxkpi_bug_count, before + 2);
	linuxkpi_bug_panic = saved;
	end();
}

TEST(err_ptr)
{
	void *p = ERR_PTR(-ENOMEM);
	int x = 0;

	CHECK(IS_ERR(p));
	CHECK(IS_ERR_OR_NULL(p));
	CHECK(IS_ERR_OR_NULL(NULL));
	CHECK(!IS_ERR(NULL));
	CHECK(!IS_ERR(&x));
	CHECK_EQ(PTR_ERR(p), -ENOMEM);
	CHECK_EQ(PTR_ERR_OR_ZERO(p), -ENOMEM);
	CHECK_EQ(PTR_ERR_OR_ZERO(&x), 0);
	CHECK(ERR_CAST(p) == p);
	CHECK(IS_ERR(ERR_PTR(-MAX_ERRNO)));
	CHECK(!IS_ERR(ERR_PTR(-MAX_ERRNO - 1)));
	CHECK(ENOTSUPP > 511 && EPROBE_DEFER > 511 && ERESTARTSYS > 511);
}

TEST(jiffies)
{
	unsigned long j0 = jiffies;
	unsigned long hz = (unsigned long)HZ;

	CHECK(hz >= 100);
	CHECK_EQ(msecs_to_jiffies(1000), hz);
	CHECK_EQ(msecs_to_jiffies(0), 0);
	CHECK(msecs_to_jiffies(1) >= 1);	/* rounds up */
	CHECK_EQ(usecs_to_jiffies(1000000), hz);
	CHECK_EQ(nsecs_to_jiffies(1000000000ULL), hz);
	CHECK_EQ(jiffies_to_msecs(hz), 1000);
	CHECK_EQ(jiffies_to_usecs(hz), 1000000);
	CHECK(jiffies_to_nsecs(hz) == 1000000000ULL);
	CHECK_EQ(secs_to_jiffies(2), 2 * hz);
	CHECK_EQ(msecs_to_jiffies(~0U) <= MAX_JIFFY_OFFSET, 1);
	CHECK(time_after(j0 + 1, j0));
	CHECK(!time_after(j0, j0));
	CHECK(time_after_eq(j0, j0));
	CHECK(time_before(j0, j0 + 1));
	CHECK(time_after(5UL, ~0UL - 5));	/* wraparound */
	CHECK(time_in_range(j0, j0 - 1, j0 + 1));
	CHECK(time_is_after_jiffies(jiffies + 1000 * hz));
	CHECK(time_is_before_jiffies(j0 - 1));
	CHECK(get_jiffies_64() >= j0);
}

static int
vaf_fmt(char *buf, size_t n, const char *fmt, ...)
{
	struct va_format vaf;
	va_list ap;
	int r;

	va_start(ap, fmt);
	vaf.fmt = fmt;
	vaf.va = &ap;
	r = snprintf(buf, n, "[%pV]", &vaf);
	va_end(ap);
	return (r);
}

TEST(linux_formats)
{
	char b[128];
	u8 bytes[4] = { 0xde, 0xad, 0xbe, 0xef };
	phys_addr_t pa = 0x1234;
	u32 fourcc = 0x34325258;	/* "XR24" */

	CHECK_EQ(snprintf(b, sizeof (b), "%d %5s|%-3u|%llx %*d %.*s", -3, "ab",
	    7U, 0xabcULL, 4, 9, 2, "xyz"), 24);
	CHECK(strcmp(b, "-3    ab|7  |abc    9 xy") == 0);
	CHECK_EQ(vaf_fmt(b, sizeof (b), "%s=%d", "x", 5), 5);
	CHECK(strcmp(b, "[x=5]") == 0);
	{
		int e = 0;

		(void) snprintf(b, sizeof (b), "%pe", ERR_PTR(-ENOMEM));
		CHECK_EQ(kstrtoint(b, 10, &e), 0);
		CHECK_EQ(e, -ENOMEM);
	}
	(void) snprintf(b, sizeof (b), "%4ph|%*phN", bytes, 2, bytes);
	CHECK(strcmp(b, "de ad be ef|dead") == 0);
	(void) snprintf(b, sizeof (b), "%pa", &pa);
	CHECK(strcmp(b, "0x0000000000001234") == 0);
	(void) snprintf(b, sizeof (b), "%p4cc", &fourcc);
	CHECK(strncmp(b, "XR24 little-endian", 18) == 0);
	CHECK_EQ(scnprintf(b, 4, "%s", "abcdef"), 3);
	CHECK_EQ(snprintf(NULL, 0, "%d%%", 100), 4);
	{
		char *k = kasprintf(GFP_KERNEL, "n%03d", 7);

		CHECK(k != NULL && strcmp(k, "n007") == 0);
		kfree(k);
	}
}

TEST(hex_dump)
{
	static const u8 d[] = { 0x01, 0x02, 0x03, 0x04, 'A', 'b', 0x7f, 0x00 };
	char line[128];
	int n;

	n = hex_dump_to_buffer(d, 4, 16, 1, line, sizeof (line), false);
	CHECK_EQ(strcmp(line, "01 02 03 04"), 0);
	CHECK_EQ(n, 11);
	(void) hex_dump_to_buffer(d, 8, 16, 4, line, sizeof (line), false);
	CHECK_EQ(strcmp(line, "04030201 007f6241"), 0);
	(void) hex_dump_to_buffer(d, 6, 16, 4, line, sizeof (line), false);
	CHECK_EQ(strcmp(line, "01 02 03 04 41 62"), 0);	/* 6 % 4: bytes */
	(void) hex_dump_to_buffer(d, 8, 16, 1, line, sizeof (line), true);
	/* ascii column at 16 * 2 + 16 + 1 = 49 */
	CHECK_EQ(strlen(line), 49 + 8);
	CHECK_EQ(strcmp(line + 49, "....Ab.."), 0);
	n = hex_dump_to_buffer(d, 8, 16, 1, line, 5, false);
	CHECK_EQ(n, 23);
	CHECK_EQ(strcmp(line, "01 0"), 0);
	print_hex_dump(KERN_DEBUG, "t: ", DUMP_PREFIX_OFFSET, 16, 1, d,
	    sizeof (d), true);
}

LKPI_SUITE(printk)
{
	RUN(levels);
	RUN(loglevel_filter);
	RUN(truncation);
	RUN(once_and_ratelimit);
	RUN(ratelimit_state);
	RUN(dev_printk);
	RUN(warn);
	RUN(bug_survivable);
	RUN(err_ptr);
	RUN(jiffies);
	RUN(linux_formats);
	RUN(hex_dump);
}
