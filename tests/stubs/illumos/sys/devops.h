/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DEVOPS_H
#define	_STUB_SYS_DEVOPS_H
#include <sys/types.h>
#include <sys/sunddi.h>
/* Real illumos: typed parameters (cred_t, struct buf, struct uio, ...). */
typedef enum { DDI_ATTACH = 0, DDI_RESUME = 1, DDI_PM_RESUME = 2 }
    ddi_attach_cmd_t;
typedef enum { DDI_DETACH = 0, DDI_SUSPEND = 1, DDI_PM_SUSPEND = 2,
    DDI_HOTPLUG_DETACH = 3 } ddi_detach_cmd_t;
typedef enum { DDI_INFO_DEVT2DEVINFO = 0, DDI_INFO_DEVT2INSTANCE = 1 }
    ddi_info_cmd_t;
typedef enum { DDI_RESET_FORCE = 0 } ddi_reset_cmd_t;
typedef enum { PROP_LEN = 0 } ddi_prop_op_t;
struct cb_ops {
	int	(*cb_open)();
	int	(*cb_close)();
	int	(*cb_strategy)();
	int	(*cb_print)();
	int	(*cb_dump)();
	int	(*cb_read)();
	int	(*cb_write)();
	int	(*cb_ioctl)();
	int	(*cb_devmap)();
	int	(*cb_mmap)();
	int	(*cb_segmap)();
	int	(*cb_chpoll)();
	int	(*cb_prop_op)(dev_t, dev_info_t *, ddi_prop_op_t, int, char *,
		    caddr_t, int *);
	void	*cb_str;
	int	cb_flag;
	int	cb_rev;
	int	(*cb_aread)();
	int	(*cb_awrite)();
};
struct bus_ops;
struct dev_ops {
	int	devo_rev;
	int	devo_refcnt;
	int	(*devo_getinfo)(dev_info_t *, ddi_info_cmd_t, void *, void **);
	int	(*devo_identify)(dev_info_t *);
	int	(*devo_probe)(dev_info_t *);
	int	(*devo_attach)(dev_info_t *, ddi_attach_cmd_t);
	int	(*devo_detach)(dev_info_t *, ddi_detach_cmd_t);
	int	(*devo_reset)(dev_info_t *, ddi_reset_cmd_t);
	struct cb_ops	*devo_cb_ops;
	struct bus_ops	*devo_bus_ops;
	int	(*devo_power)(dev_info_t *, int, int);
	int	(*devo_quiesce)(dev_info_t *);
};
#define	DEVO_REV	4
#define	CB_REV		1
extern int nodev();
extern int nulldev();
extern int nochpoll();
extern int ddi_prop_op(dev_t, dev_info_t *, ddi_prop_op_t, int, char *,
    caddr_t, int *);
extern int ddi_quiesce_not_supported(dev_info_t *);
extern void ddi_set_driver_private(dev_info_t *, void *);
extern void *ddi_get_driver_private(dev_info_t *);
extern int ddi_get_instance(dev_info_t *);
extern void ddi_report_dev(dev_info_t *);
extern unsigned long getminor(dev_t);
#endif
