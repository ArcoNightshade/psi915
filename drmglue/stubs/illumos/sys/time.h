/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_TIME_H
#define	_STUB_SYS_TIME_H
#include <sys/types.h>
extern clock_t ddi_get_lbolt(void);
extern int64_t ddi_get_lbolt64(void);
typedef struct timespec {
	long	tv_sec;
	long	tv_nsec;
} timespec_t;
typedef struct timespec timestruc_t;
extern hrtime_t gethrtime(void);
extern void gethrestime(timespec_t *);
#endif
