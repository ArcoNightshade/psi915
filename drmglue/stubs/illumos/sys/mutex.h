/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_MUTEX_H
#define	_STUB_SYS_MUTEX_H
#include <sys/types.h>
typedef enum {
	MUTEX_ADAPTIVE = 0,
	MUTEX_SPIN = 1,
	MUTEX_DRIVER = 4,
	MUTEX_DEFAULT = 6
} kmutex_type_t;
typedef struct mutex {
	void	*_opaque[1];
} kmutex_t;
struct _kthread;
extern void mutex_init(kmutex_t *, char *, kmutex_type_t, void *);
extern void mutex_destroy(kmutex_t *);
extern void mutex_enter(kmutex_t *);
extern int mutex_tryenter(kmutex_t *);
extern void mutex_exit(kmutex_t *);
extern int mutex_owned(const kmutex_t *);
extern struct _kthread *mutex_owner(const kmutex_t *);
#endif
