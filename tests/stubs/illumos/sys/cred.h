/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_CRED_H
#define	_STUB_SYS_CRED_H
#include <sys/types.h>
typedef struct cred cred_t;
/* Real illumos: CRED() is a macro over curthread->t_cred. */
extern cred_t *CRED(void);
extern int drv_priv(cred_t *);
#endif
