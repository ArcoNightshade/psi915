/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Interface between the two halves of the i915 illumos glue:
 *
 *   i915_ddi.c       illumos side: dev_ops/cb_ops (LKPI_PCI_DRIVER_MODULE),
 *                    console minor over gfx_private, drmglue nodes, suspend/
 *                    resume, panic callback. Includes DDI headers first and
 *                    no DRM header.
 *   i915_drm_glue.c  Linux side: compiled like an i915 source (DRM and i915
 *                    headers); finds the DRM minors of a probed device,
 *                    answers drmglue's ioctl-size (kcmd) queries, holds the
 *                    module parameter table and the console hooks.
 *
 * Only plain C types cross this boundary (pointers are passed as void * or
 * as incomplete struct types), so neither side sees the other's headers.
 * See docs/11-integration-notes.md section 4.
 *
 * UNTESTED ON ILLUMOS.
 */
#ifndef _PSI915_I915_GLUE_H_
#define	_PSI915_I915_GLUE_H_

#include <sys/types.h>

struct lkpi_file;
struct lkpi_module_param;

/* One DRM minor of the device, as drmglue_attach_node() wants it. */
#define	PSI915_NODE_NAME_MAX	16
struct psi915_node {
	unsigned int	node;		/* Linux DRM minor: card 0..63, render 128.. */
	char		name[PSI915_NODE_NAME_MAX];	/* "card0", "renderD128" */
	const void	*fops;		/* const struct file_operations * */
};

/* Maximum DRM minors per device: primary, render (no accel for i915). */
#define	PSI915_MAX_NODES	3

/*
 * Linux DRM minors of the drm_device bound to pdev (a struct pci_dev * that
 * i915_pci_probe() succeeded on). Returns the count, or a negative errno.
 */
int psi915_drm_nodes(void *pdev, struct psi915_node *out, int max);

/*
 * drmglue kcmd callback: the kernel's (Linux-encoded) definition of DRM
 * ioctl (type, nr), or 0 if unknown, from the core table (uapi drm.h) and
 * the driver's drm_driver.ioctls[]. drmglue uses it to recover the size of
 * structures of 256 bytes or more, which the illumos encoding truncates
 * (docs/10 section 2.4).
 */
uint32_t psi915_drm_kcmd(struct lkpi_file *f, unsigned int type,
    unsigned int nr);

/* i915 and i915 display module parameters settable from i915.conf. */
const struct lkpi_module_param *const *psi915_i915_params(size_t *n);
/* KPI (src/linux_moduleparam.c), same prototype as <linux/moduleparam.h>. */
int lkpi_module_params_apply(void *dip,
    const struct lkpi_module_param *const *tbl, size_t n);

/* video_firmware_drivers_only() policy (drm_bsd/drivers/video/nomodeset.c). */
void psi915_set_nomodeset(int on);

/*
 * Console hooks, called by the illumos side:
 *   setmode   from gfx_private's blt_ops.setmode on KDSETMODE (tem): kd_text
 *             1 = KD_TEXT (make the console framebuffer visible again),
 *             0 = KD_GRAPHICS (a DRM master owns the display). Process
 *             context, may sleep. Returns 0 or a positive errno.
 *   panic     from the CB_CL_PANIC callback: other CPUs stopped, no locks,
 *             no sleeping, no allocation. Points the primary plane of the
 *             active pipe back at the console framebuffer.
 * Design and status: docs/11-integration-notes.md section 5.
 */
int psi915_drm_console_setmode(void *pdev, int kd_text);
void psi915_drm_console_panic(void *pdev);

/* Linux module init/exit of i915 (module_init(i915_init) in i915_module.c). */
int lkpi_initcall_i915_init(void);
void lkpi_exitcall_i915_exit(void);

#endif /* _PSI915_I915_GLUE_H_ */
