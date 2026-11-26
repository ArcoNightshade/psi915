/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos kernel platform primitives for the LinuxKPI. Included only from
 * <linuxkpi/platform.h>; see there for the interface.
 *
 * UNTESTED ON ILLUMOS: written against illumos-gate headers (f71cb08e8c53)
 * and compile-checked against the stubs in tests/stubs/illumos only.
 */
#ifndef _LINUXKPI_PLATFORM_ILLUMOS_H_
#define	_LINUXKPI_PLATFORM_ILLUMOS_H_

#ifndef _LINUXKPI_PLATFORM_H_
#error "include <linuxkpi/platform.h>, not this file"
#endif

#include <sys/types.h>
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/stdbool.h>
#include <sys/errno.h>
#include <sys/varargs.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/condvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/time.h>
#include <sys/callo.h>
#include <sys/cpuvar.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/file.h>
#include <sys/random.h>

#define	lkpi_assert(x)		ASSERT(x)

#ifndef lkpi_warn
#define	lkpi_warn(msg)		cmn_err(CE_WARN, "linuxkpi: %s", (msg))
#endif

/*
 * Mutexes. ibc == NULL gives an adaptive mutex (MUTEX_DEFAULT): fine for
 * thread context and for interrupt handlers below LOCK_LEVEL that never
 * use the lock. A lock that an interrupt handler takes must be created
 * with that handler's priority as the iblock cookie (MUTEX_DRIVER,
 * DDI_INTR_PRI(pri) from ddi_intr_get_pri(9F)); above LOCK_LEVEL that
 * makes it a spin mutex that raises the SPL. See mutex_init(9F).
 *
 * A zero-filled kmutex_t/krwlock_t/kcondvar_t is a valid, unheld adaptive
 * lock (illumos uts/common/os/mutex.c), so static initialisers are zero.
 */
typedef kmutex_t	lkpi_plat_mutex_t;
#define	LKPI_PLAT_MUTEX_INITIALIZER	{ { 0 } }

static inline void
lkpi_plat_mutex_init(lkpi_plat_mutex_t *m, void *ibc)
{
	mutex_init(m, NULL, ibc != NULL ? MUTEX_DRIVER : MUTEX_DEFAULT, ibc);
}

static inline void
lkpi_plat_mutex_destroy(lkpi_plat_mutex_t *m)
{
	mutex_destroy(m);
}

static inline void
lkpi_plat_mutex_enter(lkpi_plat_mutex_t *m)
{
	mutex_enter(m);
}

static inline bool
lkpi_plat_mutex_tryenter(lkpi_plat_mutex_t *m)
{
	return (mutex_tryenter(m) != 0);
}

static inline void
lkpi_plat_mutex_exit(lkpi_plat_mutex_t *m)
{
	mutex_exit(m);
}

typedef krwlock_t	lkpi_plat_rwlock_t;
#define	LKPI_PLAT_RWLOCK_INITIALIZER	{ { 0 } }

static inline void
lkpi_plat_rwlock_init(lkpi_plat_rwlock_t *rw)
{
	rw_init(rw, NULL, RW_DEFAULT, NULL);
}

static inline void
lkpi_plat_rwlock_destroy(lkpi_plat_rwlock_t *rw)
{
	rw_destroy(rw);
}

static inline void
lkpi_plat_rwlock_enter(lkpi_plat_rwlock_t *rw, bool write)
{
	rw_enter(rw, write ? RW_WRITER : RW_READER);
}

static inline bool
lkpi_plat_rwlock_tryenter(lkpi_plat_rwlock_t *rw, bool write)
{
	return (rw_tryenter(rw, write ? RW_WRITER : RW_READER) != 0);
}

static inline void
lkpi_plat_rwlock_exit(lkpi_plat_rwlock_t *rw)
{
	rw_exit(rw);
}

static inline void
lkpi_plat_rwlock_downgrade(lkpi_plat_rwlock_t *rw)
{
	rw_downgrade(rw);
}

typedef kcondvar_t	lkpi_plat_cv_t;
#define	LKPI_PLAT_CV_INITIALIZER	{ 0 }

static inline void
lkpi_plat_cv_init(lkpi_plat_cv_t *cv)
{
	cv_init(cv, NULL, CV_DEFAULT, NULL);
}

static inline void
lkpi_plat_cv_destroy(lkpi_plat_cv_t *cv)
{
	cv_destroy(cv);
}

static inline void
lkpi_plat_cv_wait(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m)
{
	cv_wait(cv, m);
}

/* Returns true if a signal interrupted the wait. */
static inline bool
lkpi_plat_cv_wait_sig(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m)
{
	return (cv_wait_sig(cv, m) == 0);
}

/* Wait at most ticks clock ticks; LKPI_PLAT_WAIT_{OK,TIMEOUT,INTR}. */
static inline int
lkpi_plat_cv_timedwait(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m, long ticks,
    bool intr)
{
	clock_t r;

	if (ticks <= 0)
		return (LKPI_PLAT_WAIT_TIMEOUT);
	if (intr) {
		r = cv_reltimedwait_sig(cv, m, (clock_t)ticks, TR_CLOCK_TICK);
		if (r == 0)
			return (LKPI_PLAT_WAIT_INTR);
	} else {
		r = cv_reltimedwait(cv, m, (clock_t)ticks, TR_CLOCK_TICK);
	}
	return (r < 0 ? LKPI_PLAT_WAIT_TIMEOUT : LKPI_PLAT_WAIT_OK);
}

static inline void
lkpi_plat_cv_signal(lkpi_plat_cv_t *cv)
{
	cv_signal(cv);
}

static inline void
lkpi_plat_cv_broadcast(lkpi_plat_cv_t *cv)
{
	cv_broadcast(cv);
}

/* Opaque identity of the running thread (interrupt threads included). */
static inline void *
lkpi_plat_curthread(void)
{
	return ((void *)curthread);
}

static inline bool
lkpi_plat_in_interrupt(void)
{
	return (servicing_interrupt() != 0);
}

/* Tick clock: lbolt (does not wrap in practice), hz ticks per second. */
static inline unsigned long
lkpi_plat_ticks(void)
{
	return ((unsigned long)ddi_get_lbolt64());
}

static inline unsigned long
lkpi_plat_hz(void)
{
	return ((unsigned long)hz);
}

static inline void
lkpi_plat_sleep_ticks(long ticks)
{
	delay((clock_t)(ticks > 0 ? ticks : 1));
}

/*
 * High-resolution monotonic clock (gethrtime(9F)) and wall clock
 * (gethrestime(9F)), both in nanoseconds.
 */
static inline uint64_t
lkpi_plat_nsec(void)
{
	return ((uint64_t)gethrtime());
}

static inline void
lkpi_plat_realtime(int64_t *sec, long *nsec)
{
	timespec_t ts;

	gethrestime(&ts);
	*sec = (int64_t)ts.tv_sec;
	*nsec = (long)ts.tv_nsec;
}

/*
 * Wait until the monotonic clock reaches deadline (ns, lkpi_plat_nsec()
 * scale); LKPI_PLAT_WAIT_{OK,TIMEOUT,INTR}. Uses the high-resolution
 * callout (cv_timedwait_hires / cv_timedwait_sig_hrtime, absolute), so
 * the wait is not rounded up to a clock tick.
 */
static inline int
lkpi_plat_cv_timedwait_ns(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m,
    uint64_t deadline, bool intr)
{
	clock_t r;

	if ((int64_t)deadline <= 0 || deadline <= lkpi_plat_nsec())
		return (LKPI_PLAT_WAIT_TIMEOUT);
	if (intr) {
		r = cv_timedwait_sig_hrtime(cv, m, (hrtime_t)deadline);
		if (r == 0)
			return (LKPI_PLAT_WAIT_INTR);
	} else {
		r = cv_timedwait_hires(cv, m, (hrtime_t)deadline, 1000,
		    CALLOUT_FLAG_ABSOLUTE);
	}
	return (r < 0 ? LKPI_PLAT_WAIT_TIMEOUT : LKPI_PLAT_WAIT_OK);
}

/* Busy-wait (never sleeps; usable in any context). */
static inline void
lkpi_plat_udelay(unsigned long us)
{
	drv_usecwait((clock_t)us);
}

/* Number of online CPUs (sizing of worker pools). */
static inline unsigned int
lkpi_plat_ncpus(void)
{
	return (ncpus > 0 ? (unsigned int)ncpus : 1U);
}

/* Sequential id (0..ncpus-1) of the CPU running the caller (a hint). */
static inline unsigned int
lkpi_plat_cpu_id(void)
{
	return ((unsigned int)CPU->cpu_seqid);
}

/* Random bytes (random_get_pseudo_bytes(9F); never fails once booted). */
static inline void
lkpi_plat_random(void *buf, size_t len)
{
	(void) random_get_pseudo_bytes(buf, len);
}

/* Give up the CPU to other runnable threads. */
static inline void
lkpi_plat_yield(void)
{
	kpreempt(KPREEMPT_SYNC);
}

/*
 * Thread-specific data with a destructor that runs when the thread exits
 * (tsd_create(9F-like), uts/common/disp/thread.c): the destructor is
 * called from thread_exit()/lwp_exit() via tsd_exit(), and for every
 * thread still holding a value when the key is destroyed (tsd_destroy()).
 * Keys start at 0 = unallocated.
 */
typedef uint_t	lkpi_plat_tsd_t;

static inline void
lkpi_plat_tsd_create(lkpi_plat_tsd_t *key, void (*dtor)(void *))
{
	tsd_create(key, dtor);
}

static inline void
lkpi_plat_tsd_destroy(lkpi_plat_tsd_t *key)
{
	tsd_destroy(key);
}

static inline void *
lkpi_plat_tsd_get(lkpi_plat_tsd_t key)
{
	return (tsd_get(key));
}

/* May allocate (KM_SLEEP): never call from interrupt context. */
static inline int
lkpi_plat_tsd_set(lkpi_plat_tsd_t key, void *value)
{
	return (tsd_set(key, value));
}

/*
 * User memory access (ddi_copyin(9F)/ddi_copyout(9F)). kernel = true when
 * the "user" address is really a kernel address (FKIOCTL ioctls issued by
 * other kernel modules). 0 or EFAULT.
 */
static inline int
lkpi_plat_copyin(const void *uaddr, void *kaddr, size_t len, bool kernel)
{
	return (ddi_copyin(uaddr, kaddr, len, kernel ? FKIOCTL : 0) == 0 ?
	    0 : EFAULT);
}

static inline int
lkpi_plat_copyout(const void *kaddr, void *uaddr, size_t len, bool kernel)
{
	return (ddi_copyout(kaddr, uaddr, len, kernel ? FKIOCTL : 0) == 0 ?
	    0 : EFAULT);
}

/*
 * Out of line (src/plat_illumos.c), because they need process internals:
 * signal checks for the current thread (always false for kernel threads,
 * which have no lwp), the current process id and command name, string
 * copy-in, and sub-tick sleeps.
 */
bool lkpi_plat_signal_pending(void);
bool lkpi_plat_fatal_signal_pending(void);
int lkpi_plat_getpid(void);
void lkpi_plat_getcomm(char *buf, size_t len);
/* Copies at most len bytes incl. NUL; returns length or -EFAULT/-ENAMETOOLONG. */
long lkpi_plat_copyinstr(const char *uaddr, char *kbuf, size_t len,
    bool kernel);
void lkpi_plat_sleep_ns(uint64_t ns);
bool lkpi_plat_privileged(void);

static inline int
__lkpi_plat_kmflag(int flags)
{
	switch (flags) {
	case LKPI_PLAT_NOSLEEP:
		return (KM_NOSLEEP);
	case LKPI_PLAT_NOSLEEP_LAZY:
		return (KM_NOSLEEP | KM_NORMALPRI);
	default:
		return (KM_SLEEP);
	}
}

/*
 * General-purpose memory. kmem_alloc() returns virtually contiguous memory
 * that is not necessarily physically contiguous above a page; device DMA
 * must go through the DMA API (P2), never through virt-to-phys.
 */
static inline void *
lkpi_plat_alloc(size_t size, int flags)
{
	return (kmem_alloc(size, __lkpi_plat_kmflag(flags)));
}

static inline void *
lkpi_plat_zalloc(size_t size, int flags)
{
	return (kmem_zalloc(size, __lkpi_plat_kmflag(flags)));
}

static inline void
lkpi_plat_free(void *p, size_t size)
{
	kmem_free(p, size);
}

/*
 * Object caches. The constructor has illumos semantics (called when a
 * buffer is first created, not on every allocation), which match Linux
 * kmem_cache constructors. priv is passed to the constructor.
 */
typedef kmem_cache_t	lkpi_plat_kcache_t;
typedef int (*lkpi_plat_kcache_ctor_t)(void *buf, void *priv, int kmflags);

static inline lkpi_plat_kcache_t *
lkpi_plat_kcache_create(const char *name, size_t size, size_t align,
    lkpi_plat_kcache_ctor_t ctor, void *priv)
{
	/* kmem_cache_create() copies the name. */
	return ((kmem_cache_create)((char *)name, size, align, ctor, NULL,
	    NULL, priv, NULL, 0));
}

static inline void *
lkpi_plat_kcache_alloc(lkpi_plat_kcache_t *kc, int flags)
{
	return ((kmem_cache_alloc)(kc, __lkpi_plat_kmflag(flags)));
}

static inline void
lkpi_plat_kcache_free(lkpi_plat_kcache_t *kc, void *buf)
{
	(kmem_cache_free)(kc, buf);
}

static inline void
lkpi_plat_kcache_destroy(lkpi_plat_kcache_t *kc)
{
	(kmem_cache_destroy)(kc);
}

static inline void
lkpi_plat_kcache_reap(lkpi_plat_kcache_t *kc)
{
	kmem_cache_reap_soon(kc);
}

/*
 * Logging. msg is printed verbatim ("%s"), so cmn_err's leading-character
 * conventions never apply to message text. CE_WARN/CE_NOTE add their own
 * prefix and newline; the caller strips a trailing newline for those.
 */
static inline void
lkpi_plat_log(int level, const char *msg)
{
	switch (level) {
	case LKPI_PLAT_LOG_WARN:
		cmn_err(CE_WARN, "%s", msg);
		break;
	case LKPI_PLAT_LOG_NOTE:
		cmn_err(CE_NOTE, "%s", msg);
		break;
	case LKPI_PLAT_LOG_INFO:
		/* System log always; console only on verbose boot. */
		cmn_err(CE_CONT, "?%s", msg);
		break;
	case LKPI_PLAT_LOG_DEBUG:
		/* System log only. */
		cmn_err(CE_CONT, "!%s", msg);
		break;
	default:
		cmn_err(CE_CONT, "%s", msg);
		break;
	}
}

/* Whether lkpi_plat_log() appends the newline itself for this level. */
#define	LKPI_PLAT_LOG_ADDS_NEWLINE(level)				\
	((level) == LKPI_PLAT_LOG_WARN || (level) == LKPI_PLAT_LOG_NOTE)

/* A function, so a later Linux vsnprintf macro cannot capture it. */
static inline int
lkpi_plat_vsnprintf(char *buf, size_t len, const char *fmt, va_list ap)
{
	return ((int)vsnprintf(buf, len, fmt, ap));
}

static inline void __attribute__((__noreturn__))
lkpi_plat_panic(const char *msg)
{
	panic("%s", msg);
}

#endif /* _LINUXKPI_PLATFORM_ILLUMOS_H_ */
