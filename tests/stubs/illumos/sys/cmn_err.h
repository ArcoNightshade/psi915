/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_CMN_ERR_H
#define	_STUB_SYS_CMN_ERR_H
#define	CE_CONT		0
#define	CE_NOTE		1
#define	CE_WARN		2
#define	CE_PANIC	3
#define	CE_IGNORE	4
extern void cmn_err(int, const char *, ...)
    __attribute__((__format__(__printf__, 2, 3)));
#include <sys/types.h>
extern void vcmn_err(int, const char *, __builtin_va_list)
    __attribute__((__format__(__printf__, 2, 0)));
extern size_t vsnprintf(char *, size_t, const char *, __builtin_va_list)
    __attribute__((__format__(__printf__, 3, 0)));
extern size_t snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(__printf__, 3, 4)));
extern void panic(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2), __noreturn__));
#endif
