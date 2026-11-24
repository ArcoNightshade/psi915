/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Out-of-line userland platform primitives (see <linuxkpi/platform.h>):
 * service threads over pthreads. Compiles to nothing in the kernel.
 */

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_USERLAND

struct lkpi_plat_thread {
	pthread_t	tid;
	void		(*fn)(void *);
	void		*arg;
};

static void *
lkpi_plat_thread_start(void *arg)
{
	struct lkpi_plat_thread *t = arg;

	t->fn(t->arg);
	return (NULL);
}

struct lkpi_plat_thread *
lkpi_plat_thread_create(void (*fn)(void *), void *arg, const char *name)
{
	struct lkpi_plat_thread *t;

	(void) name;
	t = malloc(sizeof (*t));
	if (t == NULL)
		return (NULL);
	t->fn = fn;
	t->arg = arg;
	if (pthread_create(&t->tid, NULL, lkpi_plat_thread_start, t) != 0) {
		free(t);
		return (NULL);
	}
	return (t);
}

void
lkpi_plat_thread_join(struct lkpi_plat_thread *t)
{
	(void) pthread_join(t->tid, NULL);
	free(t);
}

#endif /* LINUXKPI_USERLAND */
