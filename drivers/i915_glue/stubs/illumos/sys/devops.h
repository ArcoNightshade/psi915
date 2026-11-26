/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Stub for HOST_STUBS=1 syntax checking of drivers/i915_glue only:
 * tests/stubs' devops.h made compatible with drmglue/stubs (whose
 * sunddi.h defines ddi_info_cmd_t and conf.h struct cb_ops).
 */
#ifndef _STUB_SYS_DEVOPS_H
#define	_STUB_SYS_DEVOPS_H
#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/conf.h>
typedef enum { DDI_ATTACH = 0, DDI_RESUME = 1, DDI_PM_RESUME = 2 }
    ddi_attach_cmd_t;
typedef enum { DDI_DETACH = 0, DDI_SUSPEND = 1, DDI_PM_SUSPEND = 2,
    DDI_HOTPLUG_DETACH = 3 } ddi_detach_cmd_t;
typedef enum { DDI_RESET_FORCE = 0 } ddi_reset_cmd_t;
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
#define	DDI_NT_DISPLAY	"ddi_display"
#define	S_IFCHR		0x2000
extern int nochpoll();
extern int ddi_quiesce_not_supported(dev_info_t *);
extern void ddi_set_driver_private(dev_info_t *, void *);
extern void *ddi_get_driver_private(dev_info_t *);
extern void ddi_report_dev(dev_info_t *);
#endif
