/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Out-of-line illumos kernel platform primitives (see <linuxkpi/platform.h>):
 * service threads over thread_create(9F-like, uts/common/disp/thread.c).
 *
 * A joinable thread is emulated with a done flag under a mutex: the thread
 * runs fn, sets done, broadcasts and calls thread_exit(); the joiner waits
 * for done and then frees the descriptor. illumos mutexes may be destroyed
 * as soon as the last holder has dropped them, so the joiner may free the
 * descriptor while the exiting thread is still in thread_exit().
 *
 * UNTESTED ON ILLUMOS.
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/disp.h>

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_ILLUMOS_KERNEL

struct lkpi_plat_thread {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
	bool			done;
	void			(*fn)(void *);
	void			*arg;
};

static void
lkpi_plat_thread_start(void *arg)
{
	struct lkpi_plat_thread *t = arg;

	t->fn(t->arg);
	lkpi_plat_mutex_enter(&t->lock);
	t->done = true;
	lkpi_plat_cv_broadcast(&t->cv);
	lkpi_plat_mutex_exit(&t->lock);
	thread_exit();
}

struct lkpi_plat_thread *
lkpi_plat_thread_create(void (*fn)(void *), void *arg, const char *name)
{
	struct lkpi_plat_thread *t;

	(void) name;
	t = lkpi_plat_zalloc(sizeof (*t), LKPI_PLAT_SLEEP);
	lkpi_plat_mutex_init(&t->lock, NULL);
	lkpi_plat_cv_init(&t->cv);
	t->fn = fn;
	t->arg = arg;
	(void) thread_create(NULL, 0, lkpi_plat_thread_start, t, 0, &p0,
	    TS_RUN, minclsyspri);
	return (t);
}

void
lkpi_plat_thread_join(struct lkpi_plat_thread *t)
{
	lkpi_plat_mutex_enter(&t->lock);
	while (!t->done)
		lkpi_plat_cv_wait(&t->cv, &t->lock);
	lkpi_plat_mutex_exit(&t->lock);
	lkpi_plat_cv_destroy(&t->cv);
	lkpi_plat_mutex_destroy(&t->lock);
	lkpi_plat_free(t, sizeof (*t));
}

#endif /* LINUXKPI_ILLUMOS_KERNEL */
