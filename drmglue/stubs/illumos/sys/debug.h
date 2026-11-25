/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DEBUG_H
#define	_STUB_SYS_DEBUG_H
extern void assfail(const char *, const char *, int);
#ifdef DEBUG
#define	ASSERT(x)	((void)((x) || (assfail(#x, __FILE__, __LINE__), 0)))
#else
#define	ASSERT(x)	((void)0)
#endif
#endif
