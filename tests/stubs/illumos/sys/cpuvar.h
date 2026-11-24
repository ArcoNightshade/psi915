/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_CPUVAR_H
#define	_STUB_SYS_CPUVAR_H
#include <sys/types.h>
extern int ncpus;
typedef int processorid_t;
typedef struct cpu {
	processorid_t	cpu_id;
	processorid_t	cpu_seqid;
} cpu_t;
extern cpu_t *curcpup(void);
#define	CPU		(curcpup())
#endif
