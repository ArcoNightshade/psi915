/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Minimal unit-test harness for the LinuxKPI, usable in two places:
 *
 *   userland (default)    each test file is a program; LKPI_SUITE(name)
 *                         defines main() and prints to stdout/stderr
 *   kernel (LKPI_KTEST)   each test file is an object in the lkpi_ktest
 *                         module; LKPI_SUITE(name) defines
 *                         int lkpi_ktest_<name>(void) (0 = pass), and output
 *                         goes to cmn_err (see tests/kernel/)
 *
 *   TEST(name) { ... CHECK(cond); CHECK_EQ(a, b); ... }
 *   LKPI_SUITE(suite) { RUN(name); ... }
 *
 * Older test files define main() themselves and call harness_done();
 * those are userland-only.
 *
 * harness_run_threads(n, fn, arg) runs fn(arg) on n service threads
 * (lkpi_plat_thread_*: pthreads in userland, kernel threads under
 * LKPI_KTEST) and waits for them, so concurrency tests are portable.
 *
 * Test files that want to observe KPI warnings define lkpi_warn before
 * including any linux/ header (see test_kref.c).
 */
#ifndef _PSI915_TESTS_HARNESS_H_
#define	_PSI915_TESTS_HARNESS_H_

#include <linuxkpi/platform.h>

static int harness_failures;
static int harness_checks;
static int harness_tests;
static const char *harness_current;

#ifdef LKPI_KTEST

#ifndef LINUXKPI_ILLUMOS_KERNEL
#error "LKPI_KTEST requires the illumos kernel configuration"
#endif

#define	harness_out(...)	cmn_err(CE_CONT, __VA_ARGS__)
#define	harness_err(...)	cmn_err(CE_CONT, __VA_ARGS__)
#define	HARNESS_PASS		0
#define	HARNESS_FAIL		1

#else

#include <stdio.h>
#include <stdlib.h>

#define	harness_out(...)	printf(__VA_ARGS__)
#define	harness_err(...)	fprintf(stderr, __VA_ARGS__)
#define	HARNESS_PASS		EXIT_SUCCESS
#define	HARNESS_FAIL		EXIT_FAILURE

#endif

#define	TEST(name)	static void test_##name(void)

#define	RUN(name) do {							\
	int __before = harness_failures;				\
	harness_current = #name;					\
	harness_tests++;						\
	test_##name();							\
	harness_out("%-4s %s\n",					\
	    harness_failures == __before ? "ok" : "FAIL", #name);	\
} while (0)

#define	CHECK(cond) do {						\
	harness_checks++;						\
	if (!(cond)) {							\
		harness_failures++;					\
		harness_err("%s:%d: %s: CHECK(%s) failed\n",		\
		    __FILE__, __LINE__, harness_current, #cond);	\
	}								\
} while (0)

#define	CHECK_EQ(a, b) do {						\
	long long __a = (long long)(a), __b = (long long)(b);		\
	harness_checks++;						\
	if (__a != __b) {						\
		harness_failures++;					\
		harness_err("%s:%d: %s: CHECK_EQ(%s, %s): %lld != %lld\n", \
		    __FILE__, __LINE__, harness_current, #a, #b, __a, __b); \
	}								\
} while (0)

static inline int
harness_done(const char *suite)
{
	harness_out("%s: %d tests, %d checks, %d failures\n", suite,
	    harness_tests, harness_checks, harness_failures);
	return (harness_failures == 0 ? HARNESS_PASS : HARNESS_FAIL);
}

static inline void
harness_reset(void)
{
	harness_failures = harness_checks = harness_tests = 0;
	harness_current = NULL;
}

#define	HARNESS_MAX_THREADS	16

static inline void
harness_run_threads(int n, void (*fn)(void *), void *arg)
{
	struct lkpi_plat_thread *t[HARNESS_MAX_THREADS];
	int i;

	if (n > HARNESS_MAX_THREADS)
		n = HARNESS_MAX_THREADS;
	for (i = 0; i < n; i++) {
		t[i] = lkpi_plat_thread_create(fn, arg, "harness");
		CHECK(t[i] != NULL);
	}
	for (i = 0; i < n; i++)
		if (t[i] != NULL)
			lkpi_plat_thread_join(t[i]);
}

#ifdef LKPI_KTEST
#define	LKPI_SUITE(name)						\
static void harness_suite_body(void);					\
int lkpi_ktest_##name(void);						\
int									\
lkpi_ktest_##name(void)							\
{									\
	harness_reset();						\
	harness_out("== %s (kernel)\n", #name);				\
	harness_suite_body();						\
	return (harness_done(#name));					\
}									\
static void harness_suite_body(void)
#else
#define	LKPI_SUITE(name)						\
static void harness_suite_body(void);					\
int									\
main(void)								\
{									\
	harness_suite_body();						\
	return (harness_done(#name));					\
}									\
static void harness_suite_body(void)
#endif

#endif /* _PSI915_TESTS_HARNESS_H_ */
