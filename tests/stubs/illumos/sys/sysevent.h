/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SYSEVENT_H
#define	_STUB_SYS_SYSEVENT_H
#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/nvpair.h>
typedef struct { uint64_t eid_seq; int64_t eid_ts; } sysevent_id_t;
#define	DDI_VENDOR_SUNW	"SUNW"
#define	DDI_SLEEP	0
#define	DDI_NOSLEEP	1
extern int ddi_log_sysevent(dev_info_t *, char *, char *, char *, nvlist_t *,
    sysevent_id_t *, int);
#endif
