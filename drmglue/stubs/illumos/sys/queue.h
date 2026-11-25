/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_QUEUE_H
#define	_STUB_SYS_QUEUE_H
/* BSD-style LIST_HEAD that linux/list.h must override. */
#define	LIST_HEAD(name, type)	struct name { struct type *lh_first; }
#endif
