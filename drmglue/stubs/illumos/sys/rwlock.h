/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_RWLOCK_H
#define	_STUB_SYS_RWLOCK_H
#include <sys/types.h>
typedef enum { RW_DRIVER = 2, RW_DEFAULT = 4 } krw_type_t;
typedef enum { RW_WRITER, RW_READER, RW_READER_STARVEWRITER } krw_t;
typedef struct _krwlock {
	void	*_opaque[1];
} krwlock_t;
extern void rw_init(krwlock_t *, char *, krw_type_t, void *);
extern void rw_destroy(krwlock_t *);
extern void rw_enter(krwlock_t *, krw_t);
extern int rw_tryenter(krwlock_t *, krw_t);
extern void rw_exit(krwlock_t *);
extern void rw_downgrade(krwlock_t *);
extern int rw_read_held(krwlock_t *);
extern int rw_write_held(krwlock_t *);
#endif
