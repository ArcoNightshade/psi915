/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_CONDVAR_H
#define	_STUB_SYS_CONDVAR_H
#include <sys/types.h>
#include <sys/mutex.h>
typedef struct _kcondvar {
	ushort_t	_opaque;
} kcondvar_t;
typedef enum { CV_DEFAULT, CV_DRIVER } kcv_type_t;
typedef enum {
	TR_NANOSEC, TR_MICROSEC, TR_MILLISEC, TR_SEC, TR_CLOCK_TICK, TR_COUNT
} time_res_t;
extern void cv_init(kcondvar_t *, char *, kcv_type_t, void *);
extern void cv_destroy(kcondvar_t *);
extern void cv_wait(kcondvar_t *, kmutex_t *);
extern int cv_wait_sig(kcondvar_t *, kmutex_t *);
extern clock_t cv_reltimedwait(kcondvar_t *, kmutex_t *, clock_t, time_res_t);
extern clock_t cv_reltimedwait_sig(kcondvar_t *, kmutex_t *, clock_t,
    time_res_t);
extern void cv_signal(kcondvar_t *);
extern void cv_broadcast(kcondvar_t *);
#endif
