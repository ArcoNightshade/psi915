/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_ATOMIC_H
#define	_STUB_SYS_ATOMIC_H
#include <sys/types.h>
extern void atomic_or_uint(volatile uint_t *, uint_t);
extern uint_t atomic_swap_uint(volatile uint_t *, uint_t);
extern uint_t atomic_cas_uint(volatile uint_t *, uint_t, uint_t);
extern void membar_producer(void);
#endif
