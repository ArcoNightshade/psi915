/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SUNDDI_H
#define	_STUB_SYS_SUNDDI_H
#include <sys/types.h>
extern size_t strnlen(const char *, size_t);
extern int ddi_copyin(const void *, void *, size_t, int);
extern int ddi_copyout(const void *, void *, size_t, int);
extern pid_t ddi_get_pid(void);
typedef struct dev_info dev_info_t;
typedef enum { DDI_INFO_DEVT2DEVINFO = 0, DDI_INFO_DEVT2INSTANCE = 1 }
    ddi_info_cmd_t;
#define	DDI_NT_DISPLAY_DRM	"ddi_display:drm"
#define	DDI_MODEL_MASK		0x00f00000
#define	DDI_MODEL_ILP32		0x00100000
#define	DDI_MODEL_LP64		0x00200000
#define	DDI_MODEL_NONE		0
extern uint_t ddi_model_convert_from(uint_t);
extern int ddi_create_minor_node(dev_info_t *, char *, int, minor_t, char *,
    int);
extern void ddi_remove_minor_node(dev_info_t *, char *);
extern int ddi_get_instance(dev_info_t *);
extern major_t ddi_driver_major(dev_info_t *);
extern int nodev();
extern int nulldev();
extern int ddi_prop_op();
typedef struct ddi_device_acc_attr {
	ushort_t	devacc_attr_version;
	uchar_t		devacc_attr_endian_flags;
	uchar_t		devacc_attr_dataorder;
	uchar_t		devacc_attr_access;
} ddi_device_acc_attr_t;
#define	DDI_DEVICE_ATTR_V0	0x0001
#define	DDI_NEVERSWAP_ACC	0x00
#define	DDI_STRICTORDER_ACC	0x00
#define	DDI_UNORDERED_OK_ACC	0x01
#define	DDI_MERGING_OK_ACC	0x02
#define	DDI_LOADCACHING_OK_ACC	0x03
#define	DDI_STORECACHING_OK_ACC	0x04
typedef unsigned long dev_t;
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
