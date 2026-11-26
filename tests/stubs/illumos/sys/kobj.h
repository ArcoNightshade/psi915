/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_KOBJ_H
#define	_STUB_SYS_KOBJ_H
#include <sys/types.h>
struct _buf;
extern struct _buf *kobj_open_file(char *);
extern void kobj_close_file(struct _buf *);
extern int kobj_read_file(struct _buf *, char *, unsigned, unsigned);
extern int kobj_get_filesize(struct _buf *, uint64_t *);
#endif
