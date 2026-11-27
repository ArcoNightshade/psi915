/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_X_CALL_H
#define	_STUB_SYS_X_CALL_H
#include <sys/types.h>
typedef uintptr_t xc_arg_t;
typedef int (*xc_func_t)(xc_arg_t, xc_arg_t, xc_arg_t);
#define	CPUSET2BV(set)	((ulong_t *)(void *)&(set))
extern void xc_call(xc_arg_t, xc_arg_t, xc_arg_t, ulong_t *, xc_func_t);
#endif
