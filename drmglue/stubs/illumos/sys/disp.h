/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DISP_H
#define	_STUB_SYS_DISP_H
#include <sys/types.h>
extern pri_t minclsyspri;
extern int servicing_interrupt(void);
extern void kpreempt(int);
#define	KPREEMPT_SYNC	(-1)
#endif
