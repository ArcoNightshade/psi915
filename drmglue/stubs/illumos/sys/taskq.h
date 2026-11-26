/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_TASKQ_H
#define	_STUB_SYS_TASKQ_H
#include <sys/types.h>
typedef struct taskq taskq_t;
typedef void (task_func_t)(void *);
typedef struct taskq_ent {
	void		*tqent_next;
	void		*tqent_prev;
	task_func_t	*tqent_func;
	void		*tqent_arg;
	uintptr_t	tqent_flags;
} taskq_ent_t;
#define	TASKQ_PREPOPULATE	0x0001
#define	TQ_NOSLEEP		0x02
extern pri_t minclsyspri;
extern taskq_t *taskq_create(const char *, int, pri_t, int, int, uint_t);
extern void taskq_destroy(taskq_t *);
extern void taskq_wait(taskq_t *);
extern void taskq_dispatch_ent(taskq_t *, task_func_t, void *, uint_t,
    taskq_ent_t *);
#endif
