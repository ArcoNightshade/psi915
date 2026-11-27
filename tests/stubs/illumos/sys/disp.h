/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DISP_H
#define	_STUB_SYS_DISP_H
#include <sys/types.h>
extern pri_t minclsyspri;
extern int servicing_interrupt(void);
extern void kpreempt(int);
extern void lkpi_stub_kpreempt_disable(void);
extern void lkpi_stub_kpreempt_enable(void);
#define	kpreempt_disable()	lkpi_stub_kpreempt_disable()
#define	kpreempt_enable()	lkpi_stub_kpreempt_enable()
#define	KPREEMPT_SYNC	(-1)
#endif
