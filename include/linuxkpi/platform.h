/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Environment selection for the LinuxKPI headers.
 *
 * The KPI headers and sources are compiled in two environments:
 *
 *   LINUXKPI_ILLUMOS_KERNEL  illumos kernel module (_KERNEL and __sun);
 *                            see <linuxkpi/platform_illumos.h>
 *   LINUXKPI_USERLAND        hosted userland (Linux or illumos), used by the
 *                            unit-test harness in tests/; see
 *                            <linuxkpi/platform_userland.h>
 *
 * Only this file (and the two platform_*.h files it includes) may include
 * environment headers on behalf of the linux/ headers; everything else
 * includes <linuxkpi/platform.h>. Each platform file provides the same small
 * set of lkpi_plat_* primitives, which are the only interface the linux/
 * headers use to reach the environment:
 *
 *   lkpi_plat_mutex_t   sleepable mutex: kmutex_t / pthread_mutex_t
 *   lkpi_plat_rwlock_t  reader/writer lock: krwlock_t / mutex+condvar
 *   lkpi_plat_cv_t      condition variable: kcondvar_t / pthread_cond_t
 *   lkpi_plat_alloc     memory: kmem_alloc / malloc
 *   lkpi_plat_kcache_*  object caches: kmem_cache_* / malloc emulation
 *   lkpi_plat_ticks     tick clock: ddi_get_lbolt64 / CLOCK_MONOTONIC (ms)
 *   lkpi_plat_curthread opaque current-thread identity
 *   lkpi_plat_log       console/log output: cmn_err / stderr
 *   lkpi_plat_thread_*  service threads: thread_create / pthread_create
 *                       (out of line, in src/plat_illumos.c and
 *                       src/plat_userland.c)
 *   lkpi_plat_nsec      monotonic ns clock: gethrtime / CLOCK_MONOTONIC
 *   lkpi_plat_cv_timedwait_ns  absolute ns deadline waits
 *   lkpi_plat_sleep_ns, lkpi_plat_udelay  sleeps and busy-waits
 *   lkpi_plat_tsd_*     thread-specific data with exit destructor:
 *                       tsd_create / pthread_key_create
 *   lkpi_plat_signal_pending, lkpi_plat_getpid, lkpi_plat_getcomm
 *   lkpi_plat_copyin/copyout/copyinstr  ddi_copyin / memcpy
 *
 * illumos headers must be included before any linux/ header: several
 * illumos names (min, max, ARRAY_SIZE, ffs, mutex_init, mutex_destroy,
 * kmem_cache_*) are redefined with Linux meaning by the linux/ headers.
 */
#ifndef _LINUXKPI_PLATFORM_H_
#define	_LINUXKPI_PLATFORM_H_

/* Memory flags for lkpi_plat_alloc() and lkpi_plat_kcache_alloc(). */
#define	LKPI_PLAT_SLEEP		0	/* may block; never fails */
#define	LKPI_PLAT_NOSLEEP	1	/* must not block; may fail */
#define	LKPI_PLAT_NOSLEEP_LAZY	2	/* may fail; low priority */

/* Results of lkpi_plat_cv_timedwait(). */
#define	LKPI_PLAT_WAIT_OK	1	/* woken (possibly spuriously) */
#define	LKPI_PLAT_WAIT_TIMEOUT	0	/* timeout expired */
#define	LKPI_PLAT_WAIT_INTR	(-1)	/* interrupted by a signal */

/* Log levels for lkpi_plat_log(). */
#define	LKPI_PLAT_LOG_WARN	0	/* errors and warnings */
#define	LKPI_PLAT_LOG_NOTE	1	/* notices */
#define	LKPI_PLAT_LOG_INFO	2	/* informational */
#define	LKPI_PLAT_LOG_DEBUG	3	/* debug (log only) */
#define	LKPI_PLAT_LOG_CONT	4	/* continuation of the previous line */

#if defined(_KERNEL) && (defined(__sun) || defined(__illumos__))

#define	LINUXKPI_ILLUMOS_KERNEL	1
#include <linuxkpi/platform_illumos.h>

#elif !defined(_KERNEL)

#define	LINUXKPI_USERLAND	1
#include <linuxkpi/platform_userland.h>

#else
#error "linuxkpi: unsupported environment (expected illumos _KERNEL or userland)"
#endif

/*
 * Struct tag clashes. illumos defines these tags with its own meaning:
 *
 *   struct mutex   kmutex_t   <sys/mutex.h>
 *   struct page    page_t     <vm/page.h>
 *   struct file    file_t     <sys/file.h>
 *   struct module  (krtld)    <sys/kobj.h>
 *   struct cred    cred_t     <sys/cred.h>
 *   struct pid     (proc)     <sys/proc.h>
 *
 * so Linux's structs of the same names cannot use those tags in the
 * kernel. Every later use of each identifier is renamed to lkpi_<name>
 * (struct tags, and also variables, members or parameters with that name,
 * consistently across all code built against the KPI; for example
 * `struct page *page` becomes `struct lkpi_page *lkpi_page` and
 * `task->pid` becomes `task->lkpi_pid`). The environment headers above
 * are already parsed, so kmutex_t, page_t, file_t and cred_t keep their
 * illumos meaning, and KPI sources use those typedefs when they mean the
 * illumos objects. Consequence: every illumos header must be included
 * before the first linux/ header (an illumos header parsed later would
 * have its own tags renamed). Done in userland too, so tests see the
 * same names.
 */
#define	mutex	lkpi_mutex
#define	page	lkpi_page
#define	file	lkpi_file
#define	module	lkpi_module
#define	cred	lkpi_cred
#define	pid	lkpi_pid

/*
 * Function and tag names Linux and illumos both use with different
 * signatures or meanings (found by building against the real illumos
 * headers, scripts/illumos-hdr-check.sh), renamed the same way:
 *
 *   list_move_tail  <sys/list.h> list_move_tail(list_t *, list_t *)
 *   struct sysinfo  <sys/sysinfo.h> CPU statistics (also a variable)
 *   sema_init       <sys/semaphore.h> ksema_t initialisation
 *   resume          <sys/disp.h> the context-switch routine (i915 has a
 *                   static resume() in gt/intel_reset.c; dev_pm_ops
 *                   members of that name are renamed consistently)
 *   timer_setup     clashes on some illumos distributions' headers
 */
#define	list_move_tail	lkpi_list_move_tail
#define	sysinfo		lkpi_sysinfo
#define	sema_init	lkpi_sema_init
#define	resume		lkpi_resume
#define	timer_setup	lkpi_timer_setup

/* Service threads (src/plat_*.c). */
struct lkpi_plat_thread;
struct lkpi_plat_thread *lkpi_plat_thread_create(void (*fn)(void *),
    void *arg, const char *name);
void lkpi_plat_thread_join(struct lkpi_plat_thread *t);
/* A thread nobody joins; its resources go away when fn returns. 0 or errno. */
int lkpi_plat_thread_create_detached(void (*fn)(void *), void *arg,
    const char *name);

static inline void
lkpi_cpu_relax(void)
{
#if defined(__x86_64__) || defined(__amd64) || defined(__i386)
	__asm__ __volatile__("pause" : : : "memory");
#else
	__asm__ __volatile__("" : : : "memory");
#endif
}

#endif /* _LINUXKPI_PLATFORM_H_ */
