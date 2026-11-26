/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for HOST_STUBS=1 syntax checking of drivers/i915_glue only. */
#ifndef _STUB_SYS_CALLB_H
#define	_STUB_SYS_CALLB_H
#include <sys/types.h>
typedef void *callb_id_t;
#define	CB_CL_PANIC	5
#define	CB_CL_MDBOOT	8
extern callb_id_t callb_add(boolean_t (*)(void *, int), void *, int, char *);
extern int callb_delete(callb_id_t);
#endif
