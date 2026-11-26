/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * lkpi_ktest: illumos misc module that runs the LinuxKPI unit tests inside
 * the kernel. Each tests/test_*.c file is compiled with -DLKPI_KTEST, which
 * turns its LKPI_SUITE(name) into int lkpi_ktest_<name>(void) (0 = pass,
 * output via cmn_err; see tests/harness.h). _init runs every suite in
 * order and logs a summary:
 *
 *   lkpi_ktest: 15 suites, 0 failed
 *
 * If any suite fails, _init returns EIO so the module does not stay
 * loaded; otherwise unload it with modunload. Intended to run on a DEBUG
 * kernel with "set kmem_flags=0xf" in /etc/system (buffer auditing,
 * redzones, deadbeef/baddcafe patterns) and with lockstat/DTrace
 * available. Depends on misc/linuxkpi, which must be installed.
 *
 * UNTESTED ON ILLUMOS: only compile-checked against stub headers
 * (make check-kmod).
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>

#include <linuxkpi/version.h>

#define	LKPI_KTEST_SUITES(X)						\
	X(kernel) X(list) X(kref) X(atomic) X(bitops) X(printk)		\
	X(slab) X(locks) X(list_sort) X(rbtree) X(rcu) X(xarray)	\
	X(sched) X(workqueue) X(misc)

#define	LKPI_KTEST_DECL(name)	int lkpi_ktest_##name(void);
LKPI_KTEST_SUITES(LKPI_KTEST_DECL)

static const struct {
	const char	*name;
	int		(*fn)(void);
} lkpi_ktest_suites[] = {
#define	LKPI_KTEST_ENTRY(name)	{ #name, lkpi_ktest_##name },
	LKPI_KTEST_SUITES(LKPI_KTEST_ENTRY)
};

static struct modlmisc lkpi_ktest_modlmisc = {
	&mod_miscops,
	"LinuxKPI kernel tests " LINUXKPI_RELEASE
};

static struct modlinkage lkpi_ktest_modlinkage = {
	MODREV_1,
	{ (void *)&lkpi_ktest_modlmisc, NULL }
};

static int
lkpi_ktest_run(void)
{
	unsigned int i, n = sizeof (lkpi_ktest_suites) /
	    sizeof (lkpi_ktest_suites[0]);
	int failed = 0;

	for (i = 0; i < n; i++) {
		if (lkpi_ktest_suites[i].fn() != 0) {
			cmn_err(CE_WARN, "lkpi_ktest: suite %s FAILED",
			    lkpi_ktest_suites[i].name);
			failed++;
		}
	}
	cmn_err(CE_CONT, "lkpi_ktest: %u suites, %d failed\n", n, failed);
	return (failed);
}

int
_init(void)
{
	if (lkpi_ktest_run() != 0)
		return (EIO);
	return (mod_install(&lkpi_ktest_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&lkpi_ktest_modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&lkpi_ktest_modlinkage));
}
