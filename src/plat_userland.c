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

__thread char lkpi_plat_self;
__thread int lkpi_plat_fake_signal;

struct lkpi_plat_thread {
	pthread_t	tid;
	void		(*fn)(void *);
	void		*arg;
	int		detached;
};

static void *
lkpi_plat_thread_start(void *arg)
{
	struct lkpi_plat_thread *t = arg;
	void (*fn)(void *) = t->fn;
	void *fnarg = t->arg;

	if (t->detached)
		free(t);
	fn(fnarg);
	return (NULL);
}

static struct lkpi_plat_thread *
lkpi_plat_thread_spawn(void (*fn)(void *), void *arg, int detached)
{
	struct lkpi_plat_thread *t;
	pthread_attr_t attr;
	int err;

	t = malloc(sizeof (*t));
	if (t == NULL)
		return (NULL);
	t->fn = fn;
	t->arg = arg;
	t->detached = detached;
	(void) pthread_attr_init(&attr);
	if (detached)
		(void) pthread_attr_setdetachstate(&attr,
		    PTHREAD_CREATE_DETACHED);
	err = pthread_create(&t->tid, &attr, lkpi_plat_thread_start, t);
	(void) pthread_attr_destroy(&attr);
	if (err != 0) {
		free(t);
		return (NULL);
	}
	return (t);
}

struct lkpi_plat_thread *
lkpi_plat_thread_create(void (*fn)(void *), void *arg, const char *name)
{
	(void) name;
	return (lkpi_plat_thread_spawn(fn, arg, 0));
}

/* The new thread owns (and frees) the descriptor; do not touch it here. */
int
lkpi_plat_thread_create_detached(void (*fn)(void *), void *arg,
    const char *name)
{
	(void) name;
	return (lkpi_plat_thread_spawn(fn, arg, 1) != NULL ? 0 : EAGAIN);
}

void
lkpi_plat_thread_join(struct lkpi_plat_thread *t)
{
	(void) pthread_join(t->tid, NULL);
	free(t);
}

#endif /* LINUXKPI_USERLAND */
