/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SYSMACROS_H
#define	_STUB_SYS_SYSMACROS_H
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#define	MAX(a, b)	((a) < (b) ? (b) : (a))
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#define	ARRAY_SIZE(x)	(sizeof (x) / sizeof (x[0]))
#endif
