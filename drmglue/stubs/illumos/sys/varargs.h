/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_VARARGS_H
#define	_STUB_SYS_VARARGS_H
typedef __builtin_va_list	__va_list;
typedef __builtin_va_list	va_list;
#define	va_start(list, name)	__builtin_va_start(list, name)
#define	va_arg(list, type)	__builtin_va_arg(list, type)
#define	va_copy(to, from)	__builtin_va_copy(to, from)
#define	va_end(list)		__builtin_va_end(list)
#endif
