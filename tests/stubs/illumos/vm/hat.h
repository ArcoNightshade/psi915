/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_VM_HAT_H
#define	_STUB_VM_HAT_H
#include <sys/types.h>
struct hat;
#define	PROT_READ		0x1
#define	PROT_WRITE		0x2
#define	HAT_NOSYNC		0x0200
#define	HAT_STRICTORDER		0x0000
#define	HAT_MERGING_OK		0x0002
#define	HAT_STORECACHING_OK	0x0008
#define	HAT_PLAT_NOCACHE	0x100000
#define	HAT_LOAD_LOCK		0x01
#define	HAT_UNLOAD_UNLOCK	0x04
extern pfn_t hat_getpfnum(struct hat *, caddr_t);
extern void hat_devload(struct hat *, caddr_t, size_t, pfn_t, uint_t, int);
extern void hat_unload(struct hat *, caddr_t, size_t, uint_t);
#endif
