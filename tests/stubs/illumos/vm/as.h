/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_VM_AS_H
#define	_STUB_VM_AS_H
#include <sys/types.h>
#include <vm/hat.h>
struct as { struct hat *a_hat; };
extern struct as kas;
#endif
