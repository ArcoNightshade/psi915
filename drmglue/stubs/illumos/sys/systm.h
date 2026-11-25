/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SYSTM_H
#define	_STUB_SYS_SYSTM_H
#include <sys/types.h>
extern int hz;
extern void delay(clock_t);
extern int ffs(uintmax_t);
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern void *memmove(void *, const void *, size_t);
extern int memcmp(const void *, const void *, size_t);
extern size_t strlen(const char *);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern size_t strlcpy(char *, const char *, size_t);
extern void bzero(void *, size_t);
extern int copyinstr(const char *, char *, size_t, size_t *);
extern int copystr(const char *, char *, size_t, size_t *);
#endif
