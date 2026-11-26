/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Hosted userland platform primitives for the LinuxKPI (unit tests).
 * Included only from <linuxkpi/platform.h>; see there for the interface.
 * Locks are pthread-backed shims with the same contract as the illumos
 * kernel versions; the tick clock runs at 1000 Hz.
 */
#ifndef _LINUXKPI_PLATFORM_USERLAND_H_
#define	_LINUXKPI_PLATFORM_USERLAND_H_

#ifndef _LINUXKPI_PLATFORM_H_
#error "include <linuxkpi/platform.h>, not this file"
#endif

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#define	lkpi_assert(x)		assert(x)

/*
 * Test programs may define lkpi_warn before including any linux/ header
 * to count or capture warnings.
 */
#ifndef lkpi_warn
#define	lkpi_warn(msg)		fprintf(stderr, "linuxkpi: %s\n", (msg))
#endif

/* Mutexes; the iblock cookie has no meaning here and is ignored. */
typedef pthread_mutex_t	lkpi_plat_mutex_t;
#define	LKPI_PLAT_MUTEX_INITIALIZER	PTHREAD_MUTEX_INITIALIZER

static inline void
lkpi_plat_mutex_init(lkpi_plat_mutex_t *m, void *ibc)
{
	(void) ibc;
	(void) pthread_mutex_init(m, NULL);
}

static inline void
lkpi_plat_mutex_destroy(lkpi_plat_mutex_t *m)
{
	(void) pthread_mutex_destroy(m);
}

static inline void
lkpi_plat_mutex_enter(lkpi_plat_mutex_t *m)
{
	(void) pthread_mutex_lock(m);
}

static inline bool
lkpi_plat_mutex_tryenter(lkpi_plat_mutex_t *m)
{
	return (pthread_mutex_trylock(m) == 0);
}

static inline void
lkpi_plat_mutex_exit(lkpi_plat_mutex_t *m)
{
	(void) pthread_mutex_unlock(m);
}

typedef pthread_cond_t	lkpi_plat_cv_t;
#define	LKPI_PLAT_CV_INITIALIZER	PTHREAD_COND_INITIALIZER

static inline void
lkpi_plat_cv_init(lkpi_plat_cv_t *cv)
{
	(void) pthread_cond_init(cv, NULL);
}

static inline void
lkpi_plat_cv_destroy(lkpi_plat_cv_t *cv)
{
	(void) pthread_cond_destroy(cv);
}

static inline void
lkpi_plat_cv_wait(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m)
{
	(void) pthread_cond_wait(cv, m);
}

/*
 * Real signals never interrupt waits in userland. Tests simulate a pending
 * signal for the calling thread by setting lkpi_plat_fake_signal (defined
 * in src/plat_userland.c): signal_pending() then reports it, and
 * interruptible waits return "interrupted" without blocking.
 */
extern __thread int lkpi_plat_fake_signal;

static inline bool
lkpi_plat_cv_wait_sig(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m)
{
	if (lkpi_plat_fake_signal)
		return (true);
	(void) pthread_cond_wait(cv, m);
	return (false);
}

/* One tick is one millisecond. */
static inline int
lkpi_plat_cv_timedwait(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m, long ticks,
    bool intr)
{
	struct timespec ts;

	if (intr && lkpi_plat_fake_signal)
		return (LKPI_PLAT_WAIT_INTR);
	if (ticks <= 0)
		return (LKPI_PLAT_WAIT_TIMEOUT);
	/* Default condvar clock is CLOCK_REALTIME. */
	(void) clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += ticks / 1000;
	ts.tv_nsec += (ticks % 1000) * 1000000L;
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}
	return (pthread_cond_timedwait(cv, m, &ts) == ETIMEDOUT ?
	    LKPI_PLAT_WAIT_TIMEOUT : LKPI_PLAT_WAIT_OK);
}

static inline void
lkpi_plat_cv_signal(lkpi_plat_cv_t *cv)
{
	(void) pthread_cond_signal(cv);
}

static inline void
lkpi_plat_cv_broadcast(lkpi_plat_cv_t *cv)
{
	(void) pthread_cond_broadcast(cv);
}

/*
 * Reader/writer lock with downgrade (pthread_rwlock_t has none): a mutex,
 * a condvar, a reader count and a writer flag; waiting writers block new
 * readers, as krwlock does.
 */
typedef struct {
	pthread_mutex_t	m;
	pthread_cond_t	cv;
	int		readers;
	int		writer;
	int		wwait;
} lkpi_plat_rwlock_t;

#define	LKPI_PLAT_RWLOCK_INITIALIZER					\
	{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0 }

static inline void
lkpi_plat_rwlock_init(lkpi_plat_rwlock_t *rw)
{
	(void) pthread_mutex_init(&rw->m, NULL);
	(void) pthread_cond_init(&rw->cv, NULL);
	rw->readers = rw->writer = rw->wwait = 0;
}

static inline void
lkpi_plat_rwlock_destroy(lkpi_plat_rwlock_t *rw)
{
	(void) pthread_cond_destroy(&rw->cv);
	(void) pthread_mutex_destroy(&rw->m);
}

static inline bool
__lkpi_plat_rw_can_enter(const lkpi_plat_rwlock_t *rw, bool write)
{
	if (write)
		return (!rw->writer && rw->readers == 0);
	return (!rw->writer && rw->wwait == 0);
}

static inline void
lkpi_plat_rwlock_enter(lkpi_plat_rwlock_t *rw, bool write)
{
	(void) pthread_mutex_lock(&rw->m);
	if (write)
		rw->wwait++;
	while (!__lkpi_plat_rw_can_enter(rw, write))
		(void) pthread_cond_wait(&rw->cv, &rw->m);
	if (write) {
		rw->wwait--;
		rw->writer = 1;
	} else {
		rw->readers++;
	}
	(void) pthread_mutex_unlock(&rw->m);
}

static inline bool
lkpi_plat_rwlock_tryenter(lkpi_plat_rwlock_t *rw, bool write)
{
	bool ok;

	(void) pthread_mutex_lock(&rw->m);
	ok = __lkpi_plat_rw_can_enter(rw, write);
	if (ok) {
		if (write)
			rw->writer = 1;
		else
			rw->readers++;
	}
	(void) pthread_mutex_unlock(&rw->m);
	return (ok);
}

static inline void
lkpi_plat_rwlock_exit(lkpi_plat_rwlock_t *rw)
{
	(void) pthread_mutex_lock(&rw->m);
	if (rw->writer)
		rw->writer = 0;
	else
		rw->readers--;
	(void) pthread_cond_broadcast(&rw->cv);
	(void) pthread_mutex_unlock(&rw->m);
}

static inline void
lkpi_plat_rwlock_downgrade(lkpi_plat_rwlock_t *rw)
{
	(void) pthread_mutex_lock(&rw->m);
	rw->writer = 0;
	rw->readers = 1;
	(void) pthread_cond_broadcast(&rw->cv);
	(void) pthread_mutex_unlock(&rw->m);
}

/*
 * The address of a thread-local variable identifies the thread. It must
 * be one variable for the whole program (defined in src/plat_userland.c),
 * not a static in this header, or each translation unit would see a
 * different identity for the same thread.
 */
extern __thread char lkpi_plat_self;

static inline void *
lkpi_plat_curthread(void)
{
	return (&lkpi_plat_self);
}

static inline bool
lkpi_plat_in_interrupt(void)
{
	return (false);
}

static inline unsigned long
lkpi_plat_ticks(void)
{
	struct timespec ts;

	(void) clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((unsigned long)ts.tv_sec * 1000UL +
	    (unsigned long)ts.tv_nsec / 1000000UL);
}

static inline unsigned long
lkpi_plat_hz(void)
{
	return (1000UL);
}

static inline void
lkpi_plat_sleep_ticks(long ticks)
{
	struct timespec ts;

	if (ticks <= 0)
		ticks = 1;
	ts.tv_sec = ticks / 1000;
	ts.tv_nsec = (ticks % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		continue;
}

static inline uint64_t
lkpi_plat_nsec(void)
{
	struct timespec ts;

	(void) clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

static inline void
lkpi_plat_realtime(int64_t *sec, long *nsec)
{
	struct timespec ts;

	(void) clock_gettime(CLOCK_REALTIME, &ts);
	*sec = (int64_t)ts.tv_sec;
	*nsec = ts.tv_nsec;
}

/*
 * Absolute monotonic deadline. The default condvar clock is
 * CLOCK_REALTIME, so convert the remaining time to a realtime deadline.
 */
static inline int
lkpi_plat_cv_timedwait_ns(lkpi_plat_cv_t *cv, lkpi_plat_mutex_t *m,
    uint64_t deadline, bool intr)
{
	struct timespec ts;
	uint64_t now = lkpi_plat_nsec(), left;

	if (intr && lkpi_plat_fake_signal)
		return (LKPI_PLAT_WAIT_INTR);
	if (deadline <= now)
		return (LKPI_PLAT_WAIT_TIMEOUT);
	left = deadline - now;
	(void) clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += (time_t)(left / 1000000000ULL);
	ts.tv_nsec += (long)(left % 1000000000ULL);
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}
	if (pthread_cond_timedwait(cv, m, &ts) == ETIMEDOUT &&
	    lkpi_plat_nsec() >= deadline)
		return (LKPI_PLAT_WAIT_TIMEOUT);
	return (LKPI_PLAT_WAIT_OK);
}

static inline void
lkpi_plat_sleep_ns(uint64_t ns)
{
	struct timespec ts;

	ts.tv_sec = (time_t)(ns / 1000000000ULL);
	ts.tv_nsec = (long)(ns % 1000000000ULL);
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		continue;
}

static inline void
lkpi_plat_udelay(unsigned long us)
{
	uint64_t end = lkpi_plat_nsec() + (uint64_t)us * 1000ULL;

	while (lkpi_plat_nsec() < end)
		continue;
}

static inline unsigned int
lkpi_plat_ncpus(void)
{
	return (4U);
}

static inline unsigned int
lkpi_plat_cpu_id(void)
{
	return (0U);
}

static inline void
lkpi_plat_random(void *buf, size_t len)
{
	FILE *f = fopen("/dev/urandom", "rb");
	size_t n = 0;

	if (f != NULL) {
		n = fread(buf, 1, len, f);
		fclose(f);
	}
	for (; n < len; n++)	/* no /dev/urandom: clock-derived filler */
		((unsigned char *)buf)[n] = (unsigned char)(clock() ^ (n * 131));
}

static inline void
lkpi_plat_yield(void)
{
	(void) sched_yield();
}

/* Thread-specific data with an exit destructor: pthread keys. */
typedef struct {
	pthread_key_t	key;
	int		valid;
} lkpi_plat_tsd_t;

static inline void
lkpi_plat_tsd_create(lkpi_plat_tsd_t *key, void (*dtor)(void *))
{
	if (!key->valid && pthread_key_create(&key->key, dtor) == 0)
		key->valid = 1;
}

static inline void
lkpi_plat_tsd_destroy(lkpi_plat_tsd_t *key)
{
	if (key->valid)
		(void) pthread_key_delete(key->key);
	key->valid = 0;
}

static inline void *
lkpi_plat_tsd_get(lkpi_plat_tsd_t key)
{
	return (key.valid ? pthread_getspecific(key.key) : NULL);
}

static inline int
lkpi_plat_tsd_set(lkpi_plat_tsd_t key, void *value)
{
	return (key.valid ? pthread_setspecific(key.key, value) : EINVAL);
}

static inline bool
lkpi_plat_signal_pending(void)
{
	return (lkpi_plat_fake_signal != 0);
}

static inline bool
lkpi_plat_fatal_signal_pending(void)
{
	return (lkpi_plat_fake_signal > 1);
}

static inline bool
lkpi_plat_privileged(void)
{
	return (geteuid() == 0);
}

static inline int
lkpi_plat_getpid(void)
{
	return ((int)getpid());
}

static inline void
lkpi_plat_getcomm(char *buf, size_t len)
{
	(void) snprintf(buf, len, "%s", "userland");
}

/*
 * "User" memory is ordinary memory here; addresses below 4096 fault, so
 * tests can exercise the -EFAULT paths.
 */
static inline int
lkpi_plat_copyin(const void *uaddr, void *kaddr, size_t len, bool kernel)
{
	(void) kernel;
	if (len != 0 && (uintptr_t)uaddr < 4096)
		return (EFAULT);
	if (len != 0)
		memcpy(kaddr, uaddr, len);
	return (0);
}

static inline int
lkpi_plat_copyout(const void *kaddr, void *uaddr, size_t len, bool kernel)
{
	(void) kernel;
	if (len != 0 && (uintptr_t)uaddr < 4096)
		return (EFAULT);
	if (len != 0)
		memcpy(uaddr, kaddr, len);
	return (0);
}

static inline long
lkpi_plat_copyinstr(const char *uaddr, char *kbuf, size_t len, bool kernel)
{
	size_t i;

	(void) kernel;
	if (len == 0)
		return (0);
	if ((uintptr_t)uaddr < 4096)
		return (-EFAULT);
	for (i = 0; i < len; i++) {
		kbuf[i] = uaddr[i];
		if (uaddr[i] == '\0')
			return ((long)i);
	}
	return (-ENAMETOOLONG);
}

static inline void *
lkpi_plat_alloc(size_t size, int flags)
{
	(void) flags;
	return (malloc(size != 0 ? size : 1));
}

static inline void *
lkpi_plat_zalloc(size_t size, int flags)
{
	(void) flags;
	return (calloc(1, size != 0 ? size : 1));
}

static inline void
lkpi_plat_free(void *p, size_t size)
{
	(void) size;
	free(p);
}

/*
 * Object caches: each object is a separate aligned malloc block and is
 * constructed on every allocation, which gives callers the same guarantee
 * as a constructed-object cache (objects come back constructed).
 */
typedef int (*lkpi_plat_kcache_ctor_t)(void *buf, void *priv, int kmflags);

typedef struct lkpi_plat_kcache {
	size_t			size;
	size_t			align;
	lkpi_plat_kcache_ctor_t	ctor;
	void			*priv;
} lkpi_plat_kcache_t;

static inline lkpi_plat_kcache_t *
lkpi_plat_kcache_create(const char *name, size_t size, size_t align,
    lkpi_plat_kcache_ctor_t ctor, void *priv)
{
	lkpi_plat_kcache_t *kc = malloc(sizeof (*kc));

	(void) name;
	if (kc == NULL)
		return (NULL);
	kc->size = size != 0 ? size : 1;
	kc->align = align < sizeof (void *) ? sizeof (void *) : align;
	kc->ctor = ctor;
	kc->priv = priv;
	return (kc);
}

static inline void *
lkpi_plat_kcache_alloc(lkpi_plat_kcache_t *kc, int flags)
{
	void *p;

	if (posix_memalign(&p, kc->align, kc->size) != 0)
		return (NULL);
	if (kc->ctor != NULL && kc->ctor(p, kc->priv, flags) != 0) {
		free(p);
		return (NULL);
	}
	return (p);
}

static inline void
lkpi_plat_kcache_free(lkpi_plat_kcache_t *kc, void *buf)
{
	(void) kc;
	free(buf);
}

static inline void
lkpi_plat_kcache_destroy(lkpi_plat_kcache_t *kc)
{
	free(kc);
}

static inline void
lkpi_plat_kcache_reap(lkpi_plat_kcache_t *kc)
{
	(void) kc;
}

static inline void
lkpi_plat_log(int level, const char *msg)
{
	static const char *const prefix[] = {
		"WARNING: ", "NOTICE: ", "", "", ""
	};

	fprintf(stderr, "%s%s", prefix[level >= 0 && level <= 4 ? level : 4],
	    msg);
	if (level == LKPI_PLAT_LOG_WARN || level == LKPI_PLAT_LOG_NOTE)
		fputc('\n', stderr);
}

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
	fprintf(stderr, "linuxkpi: panic: %s\n", msg);
	abort();
}

/* Installed physical memory in pages (sizing limits). */
static inline unsigned long
lkpi_plat_physpages(void)
{
	long n = sysconf(_SC_PHYS_PAGES);

	return (n > 0 ? (unsigned long)n : 262144UL);
}

#endif /* _LINUXKPI_PLATFORM_USERLAND_H_ */
