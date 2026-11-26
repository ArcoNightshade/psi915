/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_PARAM_H
#define	_STUB_SYS_PARAM_H
#include <sys/types.h>
#define	PAGESHIFT	12
#define	PAGESIZE	(1UL << PAGESHIFT)
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	btop(x)		((unsigned long)(x) >> PAGESHIFT)
#define	btopr(x)	(((unsigned long)(x) + PAGEOFFSET) >> PAGESHIFT)
#define	ptob(x)		((unsigned long)(x) << PAGESHIFT)
#ifndef INT_MAX
#define	INT_MAX		2147483647
#endif
#endif
