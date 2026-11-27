/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_KLWP_H
#define	_STUB_SYS_KLWP_H
#include <sys/types.h>
/* Real illumos: klwp_t is much larger; only fields we use. */
typedef struct _klwp {
	uchar_t		lwp_sysabort;
} klwp_t;
#endif
