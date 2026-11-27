/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_PARAM_H
#define	_STUB_SYS_PARAM_H
#include <sys/types.h>
#define	PAGESIZE	4096
#define	PAGESHIFT	12
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)
#define	ptob(x)		((size_t)(x) << PAGESHIFT)
#define	btop(x)		((x) >> PAGESHIFT)
#endif
