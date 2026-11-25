/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SUNDDI_H
#define	_STUB_SYS_SUNDDI_H
#include <sys/types.h>
extern size_t strnlen(const char *, size_t);
extern int ddi_copyin(const void *, void *, size_t, int);
extern int ddi_copyout(const void *, void *, size_t, int);
extern pid_t ddi_get_pid(void);
#endif
