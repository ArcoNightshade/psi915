/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_NVPAIR_H
#define	_STUB_SYS_NVPAIR_H
#include <sys/types.h>
typedef struct nvlist nvlist_t;
#define	NV_UNIQUE_NAME_TYPE	0x2
extern int nvlist_alloc(nvlist_t **, uint_t, int);
extern void nvlist_free(nvlist_t *);
extern int nvlist_add_string(nvlist_t *, const char *, const char *);
#endif
