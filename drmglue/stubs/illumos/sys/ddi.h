/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DDI_H
#define	_STUB_SYS_DDI_H
#define	min(a, b)	((a) < (b) ? (a) : (b))
#define	max(a, b)	((a) < (b) ? (b) : (a))
#include <sys/types.h>
extern void delay(clock_t);
extern clock_t drv_usectohz(clock_t);
extern clock_t drv_hztousec(clock_t);
extern void drv_usecwait(clock_t);
extern major_t getmajor(dev_t);
extern minor_t getminor(dev_t);
extern dev_t makedevice(major_t, minor_t);
#endif
