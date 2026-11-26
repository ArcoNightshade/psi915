/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_USER_H
#define	_STUB_SYS_USER_H
#include <sys/types.h>
#define	MAXCOMLEN	16
typedef struct user {
	char	u_comm[MAXCOMLEN + 1];
} user_t;
#endif
