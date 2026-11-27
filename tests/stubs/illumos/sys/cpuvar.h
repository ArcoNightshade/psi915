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
/* Real illumos: struct cpuset is a bitmap of NCPU bits. */
typedef struct cpuset {
	ulong_t	cpub[4];
} cpuset_t;
extern void cpuset_all(cpuset_t *);
#define	CPUSET_ALL(set)		cpuset_all(&(set))
#endif
