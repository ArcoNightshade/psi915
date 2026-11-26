/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SUNDDI_H
#define	_STUB_SYS_SUNDDI_H
#include <sys/types.h>
extern size_t strnlen(const char *, size_t);
extern int ddi_copyin(const void *, void *, size_t, int);
extern int ddi_copyout(const void *, void *, size_t, int);
extern pid_t ddi_get_pid(void);
typedef struct __dev_info *dev_info_t;
#define	DDI_DEV_T_ANY		((dev_t)-2)
#define	DDI_DEV_T_NONE		((dev_t)-1)
#define	DDI_PROP_DONTPASS	0x0001
#define	DDI_PROP_CANSLEEP	0x0000
#define	DDI_PROP_SUCCESS	0
#define	DDI_SUCCESS		0
#define	DDI_FAILURE		(-1)
extern int ddi_prop_exists(dev_t, dev_info_t *, uint_t, char *);
extern int ddi_prop_get_int(dev_t, dev_info_t *, uint_t, char *, int);
extern int ddi_prop_lookup_string(dev_t, dev_info_t *, uint_t, char *,
    char **);
extern void ddi_prop_free(void *);
#endif
