/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos driver glue for Linux PCI drivers (kernel only). Not a Linux
 * header. A driver kmod built on the KPI contains, in one illumos-side
 * source file (included before any linux/ header, see platform.h):
 *
 *   #include <linuxkpi/ddi.h>
 *   LKPI_PCI_DRIVER_MODULE(toy, "LinuxKPI toy PCI driver", NULL,
 *       lkpi_initcall_toy_init, lkpi_exitcall_toy_exit);
 *
 * where lkpi_initcall_<fn>/lkpi_exitcall_<fn> are the wrappers that the
 * driver's module_init(fn)/module_exit(fn) define (<linux/module.h>).
 * The macro provides dev_ops, modldrv, modlinkage and _init/_info/_fini:
 *
 *   _init    runs the Linux module init (which calls
 *            pci_register_driver()), then mod_install(); undoes the init
 *            if mod_install() fails
 *   attach   DDI_ATTACH: wrap the dev_info_t in a struct pci_dev, match
 *            the registered drivers' id_tables, call probe(); the pci_dev
 *            is the driver-private data of the dev_info_t.
 *            DDI_RESUME: dev_pm_ops resume sequence (lkpi_pci_resume)
 *   detach   DDI_DETACH: remove(), devres release; DDI_SUSPEND: suspend
 *            sequence. Refusing a detach while the device is open is the
 *            cb_ops layer's job (cbops->cb_open accounting, P3).
 *   _fini    mod_remove(), then the Linux module exit
 *   getinfo  minor numbers encode the instance in the bits above
 *            LKPI_MINOR_INSTANCE_SHIFT
 *   quiesce  ddi_quiesce_not_supported (Linux shutdown may block)
 *
 * cbops may be NULL for a driver without device nodes.
 *
 * UNTESTED ON ILLUMOS (stub-header compile only).
 */
#ifndef _LINUXKPI_DDI_H_
#define	_LINUXKPI_DDI_H_

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	LKPI_MINOR_INSTANCE_SHIFT	8

int lkpi_pci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
int lkpi_pci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
int lkpi_ddi_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **result);
/* Linux negative errno -> illumos errno for _init/_fini returns. */
int lkpi_errno_to_illumos(int err);
extern struct cb_ops lkpi_ddi_null_cb_ops;

#define	LKPI_PCI_DRIVER_MODULE(_name, _desc, _cbops, _initfn, _exitfn)	\
int _initfn(void);							\
void _exitfn(void);							\
static struct dev_ops _name##_lkpi_dev_ops = {				\
	DEVO_REV, 0, lkpi_ddi_getinfo, nulldev, nulldev,		\
	lkpi_pci_attach, lkpi_pci_detach, nodev,			\
	(_cbops) != NULL ? (_cbops) : &lkpi_ddi_null_cb_ops,		\
	NULL, NULL, ddi_quiesce_not_supported				\
};									\
static struct modldrv _name##_lkpi_modldrv = {				\
	&mod_driverops, (char *)(_desc), &_name##_lkpi_dev_ops		\
};									\
static struct modlinkage _name##_lkpi_modlinkage = {			\
	MODREV_1, { (void *)&_name##_lkpi_modldrv, NULL }		\
};									\
int									\
_init(void)								\
{									\
	int r = _initfn();						\
									\
	if (r != 0)							\
		return (lkpi_errno_to_illumos(r));			\
	r = mod_install(&_name##_lkpi_modlinkage);			\
	if (r != 0)							\
		_exitfn();						\
	return (r);							\
}									\
int									\
_info(struct modinfo *mip)						\
{									\
	return (mod_info(&_name##_lkpi_modlinkage, mip));		\
}									\
int									\
_fini(void)								\
{									\
	int r = mod_remove(&_name##_lkpi_modlinkage);			\
									\
	if (r == 0)							\
		_exitfn();						\
	return (r);							\
}

#endif /* _LINUXKPI_DDI_H_ */
