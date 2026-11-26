/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * attach(9E)/detach(9E)/getinfo(9E) glue for Linux PCI drivers (see
 * <linuxkpi/ddi.h>). Kernel only. UNTESTED ON ILLUMOS.
 */

#ifdef _KERNEL

/* illumos headers first (see <linuxkpi/platform.h>). */
#include <linuxkpi/ddi.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/errno.h>

struct cb_ops lkpi_ddi_null_cb_ops = {
	.cb_open = nodev,
	.cb_close = nodev,
	.cb_strategy = nodev,
	.cb_print = nodev,
	.cb_dump = nodev,
	.cb_read = nodev,
	.cb_write = nodev,
	.cb_ioctl = nodev,
	.cb_devmap = nodev,
	.cb_mmap = nodev,
	.cb_segmap = nodev,
	.cb_chpoll = nochpoll,
	.cb_prop_op = ddi_prop_op,
	.cb_str = NULL,
	.cb_flag = D_MP | D_NEW,
	.cb_rev = CB_REV,
	.cb_aread = nodev,
	.cb_awrite = nodev,
};

int
lkpi_errno_to_illumos(int err)
{
	if (err >= 0)
		return (err);
	err = -err;
	return (err < 512 ? err : EIO);
}

int
lkpi_pci_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct pci_dev *pdev;
	int r;

	switch (cmd) {
	case DDI_ATTACH:
		pdev = lkpi_pci_dev_alloc(dip);
		if (pdev == NULL)
			return (DDI_FAILURE);
		pdev->lkpi_attached = true;
		ddi_set_driver_private(dip, pdev);
		r = lkpi_pci_probe(pdev);
		if (r != 0) {
			dev_err(&pdev->dev, "probe failed: %d\n", r);
			ddi_set_driver_private(dip, NULL);
			pdev->lkpi_attached = false;
			pci_dev_put(pdev);
			return (DDI_FAILURE);
		}
		ddi_report_dev(dip);
		return (DDI_SUCCESS);
	case DDI_RESUME:
		pdev = ddi_get_driver_private(dip);
		if (pdev == NULL)
			return (DDI_FAILURE);
		return (lkpi_pci_resume(pdev) == 0 ? DDI_SUCCESS :
		    DDI_FAILURE);
	default:
		return (DDI_FAILURE);
	}
}

int
lkpi_pci_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct pci_dev *pdev = ddi_get_driver_private(dip);

	if (pdev == NULL)
		return (DDI_FAILURE);
	switch (cmd) {
	case DDI_DETACH:
		lkpi_pci_remove(pdev);
		ddi_set_driver_private(dip, NULL);
		pdev->lkpi_attached = false;
		pci_dev_put(pdev);
		return (DDI_SUCCESS);
	case DDI_SUSPEND:
		return (lkpi_pci_suspend(pdev) == 0 ? DDI_SUCCESS :
		    DDI_FAILURE);
	default:
		return (DDI_FAILURE);
	}
}

struct lkpi_inst_arg {
	int		instance;
	const char	*drv;
	dev_info_t	*found;
};

int
lkpi_ddi_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **result)
{
	int instance = (int)(getminor((dev_t)arg) >>
	    LKPI_MINOR_INSTANCE_SHIFT);

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		/* VERIFY: dip is the node for this instance in practice. */
		if (dip == NULL || ddi_get_instance(dip) != instance)
			return (DDI_FAILURE);
		*result = dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(uintptr_t)instance;
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

#endif /* _KERNEL */
