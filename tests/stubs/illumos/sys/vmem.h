/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_VMEM_H
#define	_STUB_SYS_VMEM_H
#include <sys/types.h>
typedef struct vmem vmem_t;
#define	VM_SLEEP	0x0
#define	VM_NOSLEEP	0x1
extern void *vmem_alloc(vmem_t *, size_t, int);
extern void vmem_free(vmem_t *, void *, size_t);
#endif
