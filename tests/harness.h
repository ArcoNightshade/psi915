/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Minimal userland unit-test harness for the pure-logic LinuxKPI headers.
 *
 *   TEST(name) { ... CHECK(cond); CHECK_EQ(a, b); ... }
 *   int main(void) { RUN(name); ...; return harness_done(); }
 *
 * Test files that want to observe KPI warnings define lkpi_warn before
 * including any linux/ header (see test_kref.c).
 */
#ifndef _PSI915_TESTS_HARNESS_H_
#define	_PSI915_TESTS_HARNESS_H_

#include <stdio.h>
#include <stdlib.h>

static int harness_failures;
static int harness_checks;
static int harness_tests;
static const char *harness_current;

#define	TEST(name)	static void test_##name(void)

#define	RUN(name) do {							\
	int __before = harness_failures;				\
	harness_current = #name;					\
	harness_tests++;						\
	test_##name();							\
	printf("%-4s %s\n", harness_failures == __before ? "ok" : "FAIL", #name); \
} while (0)

#define	CHECK(cond) do {						\
	harness_checks++;						\
	if (!(cond)) {							\
		harness_failures++;					\
		fprintf(stderr, "%s:%d: %s: CHECK(%s) failed\n",	\
		    __FILE__, __LINE__, harness_current, #cond);	\
	}								\
} while (0)

#define	CHECK_EQ(a, b) do {						\
	long long __a = (long long)(a), __b = (long long)(b);		\
	harness_checks++;						\
	if (__a != __b) {						\
		harness_failures++;					\
		fprintf(stderr, "%s:%d: %s: CHECK_EQ(%s, %s): %lld != %lld\n", \
		    __FILE__, __LINE__, harness_current, #a, #b, __a, __b); \
	}								\
} while (0)

static inline int
harness_done(const char *suite)
{
	printf("%s: %d tests, %d checks, %d failures\n", suite,
	    harness_tests, harness_checks, harness_failures);
	return (harness_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

#endif /* _PSI915_TESTS_HARNESS_H_ */
