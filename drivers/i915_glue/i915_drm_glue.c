/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux side of the i915 illumos glue (see i915_glue.h). Compiled with the
 * i915 flags and include path (build/i915), like an imported i915 source,
 * but held to the project warning policy (-Werror).
 *
 * UNTESTED ON ILLUMOS.
 */

/* illumos-facing interface first (it includes <sys/types.h>). */
#include "i915_glue.h"

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/sprintf.h>
#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>

/*
 * DRM minors. i915_driver_probe() stores the drm_device as the PCI driver
 * data: pci_set_drvdata(pdev, &i915->drm) (i915_driver.c).
 */
int
psi915_drm_nodes(void *pdev_arg, struct psi915_node *out, int max)
{
	struct pci_dev *pdev = pdev_arg;
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_minor *minors[2];
	int i, n = 0;

	if (dev == NULL || dev->driver == NULL)
		return (-ENODEV);
	minors[0] = dev->primary;
	minors[1] = dev->render;
	for (i = 0; i < 2; i++) {
		struct drm_minor *m = minors[i];

		if (m == NULL)
			continue;
		/* 0xfe is the console minor, 0xff drmglue's anon host. */
		if (n >= max || m->index < 0 || m->index > 0xfd)
			return (-ERANGE);
		out[n].node = (unsigned int)m->index;
		(void) snprintf(out[n].name, sizeof (out[n].name),
		    m->type == DRM_MINOR_RENDER ? "renderD%d" : "card%d",
		    m->index);
		/*
		 * The driver's fops (drm_open, drm_ioctl, ...) directly: drmglue
		 * gives the inode i_rdev = MKDEV(DRM_MAJOR, node), so drm_open()
		 * finds the minor through drm_minor_acquire(iminor()), which is
		 * all that drm_stub_open() (static in drm_drv.c) would add.
		 */
		out[n].fops = dev->driver->fops;
		n++;
	}
	return (n);
}

/*
 * Core DRM ioctls by number (drm_ioctl.c's drm_ioctls[] is static), from
 * every "#define DRM_IOCTL_x DRM_IO*(nr, ...)" in drm/include/uapi/drm/drm.h
 * (v6.14). Core numbers are 0x00-0x3f and 0xa0-0xff; 0x40-0x9f belong to
 * the driver (DRM_COMMAND_BASE..DRM_COMMAND_END).
 */
#define	PSI915_CORE(c)	[DRM_IOCTL_NR(c)] = (c)
static const uint32_t psi915_core_cmds[256] = {
	PSI915_CORE(DRM_IOCTL_VERSION),
	PSI915_CORE(DRM_IOCTL_GET_UNIQUE),
	PSI915_CORE(DRM_IOCTL_GET_MAGIC),
	PSI915_CORE(DRM_IOCTL_IRQ_BUSID),
	PSI915_CORE(DRM_IOCTL_GET_MAP),
	PSI915_CORE(DRM_IOCTL_GET_CLIENT),
	PSI915_CORE(DRM_IOCTL_GET_STATS),
	PSI915_CORE(DRM_IOCTL_SET_VERSION),
	PSI915_CORE(DRM_IOCTL_MODESET_CTL),
	PSI915_CORE(DRM_IOCTL_GEM_CLOSE),
	PSI915_CORE(DRM_IOCTL_GEM_FLINK),
	PSI915_CORE(DRM_IOCTL_GEM_OPEN),
	PSI915_CORE(DRM_IOCTL_GET_CAP),
	PSI915_CORE(DRM_IOCTL_SET_CLIENT_CAP),
	PSI915_CORE(DRM_IOCTL_SET_UNIQUE),
	PSI915_CORE(DRM_IOCTL_AUTH_MAGIC),
	PSI915_CORE(DRM_IOCTL_BLOCK),
	PSI915_CORE(DRM_IOCTL_UNBLOCK),
	PSI915_CORE(DRM_IOCTL_CONTROL),
	PSI915_CORE(DRM_IOCTL_ADD_MAP),
	PSI915_CORE(DRM_IOCTL_ADD_BUFS),
	PSI915_CORE(DRM_IOCTL_MARK_BUFS),
	PSI915_CORE(DRM_IOCTL_INFO_BUFS),
	PSI915_CORE(DRM_IOCTL_MAP_BUFS),
	PSI915_CORE(DRM_IOCTL_FREE_BUFS),
	PSI915_CORE(DRM_IOCTL_RM_MAP),
	PSI915_CORE(DRM_IOCTL_SET_SAREA_CTX),
	PSI915_CORE(DRM_IOCTL_GET_SAREA_CTX),
	PSI915_CORE(DRM_IOCTL_SET_MASTER),
	PSI915_CORE(DRM_IOCTL_DROP_MASTER),
	PSI915_CORE(DRM_IOCTL_ADD_CTX),
	PSI915_CORE(DRM_IOCTL_RM_CTX),
	PSI915_CORE(DRM_IOCTL_MOD_CTX),
	PSI915_CORE(DRM_IOCTL_GET_CTX),
	PSI915_CORE(DRM_IOCTL_SWITCH_CTX),
	PSI915_CORE(DRM_IOCTL_NEW_CTX),
	PSI915_CORE(DRM_IOCTL_RES_CTX),
	PSI915_CORE(DRM_IOCTL_ADD_DRAW),
	PSI915_CORE(DRM_IOCTL_RM_DRAW),
	PSI915_CORE(DRM_IOCTL_DMA),
	PSI915_CORE(DRM_IOCTL_LOCK),
	PSI915_CORE(DRM_IOCTL_UNLOCK),
	PSI915_CORE(DRM_IOCTL_FINISH),
	PSI915_CORE(DRM_IOCTL_PRIME_HANDLE_TO_FD),
	PSI915_CORE(DRM_IOCTL_PRIME_FD_TO_HANDLE),
	PSI915_CORE(DRM_IOCTL_AGP_ACQUIRE),
	PSI915_CORE(DRM_IOCTL_AGP_RELEASE),
	PSI915_CORE(DRM_IOCTL_AGP_ENABLE),
	PSI915_CORE(DRM_IOCTL_AGP_INFO),
	PSI915_CORE(DRM_IOCTL_AGP_ALLOC),
	PSI915_CORE(DRM_IOCTL_AGP_FREE),
	PSI915_CORE(DRM_IOCTL_AGP_BIND),
	PSI915_CORE(DRM_IOCTL_AGP_UNBIND),
	PSI915_CORE(DRM_IOCTL_SG_ALLOC),
	PSI915_CORE(DRM_IOCTL_SG_FREE),
	PSI915_CORE(DRM_IOCTL_WAIT_VBLANK),
	PSI915_CORE(DRM_IOCTL_CRTC_GET_SEQUENCE),
	PSI915_CORE(DRM_IOCTL_CRTC_QUEUE_SEQUENCE),
	PSI915_CORE(DRM_IOCTL_UPDATE_DRAW),
	PSI915_CORE(DRM_IOCTL_MODE_GETRESOURCES),
	PSI915_CORE(DRM_IOCTL_MODE_GETCRTC),
	PSI915_CORE(DRM_IOCTL_MODE_SETCRTC),
	PSI915_CORE(DRM_IOCTL_MODE_CURSOR),
	PSI915_CORE(DRM_IOCTL_MODE_GETGAMMA),
	PSI915_CORE(DRM_IOCTL_MODE_SETGAMMA),
	PSI915_CORE(DRM_IOCTL_MODE_GETENCODER),
	PSI915_CORE(DRM_IOCTL_MODE_GETCONNECTOR),
	PSI915_CORE(DRM_IOCTL_MODE_ATTACHMODE),
	PSI915_CORE(DRM_IOCTL_MODE_DETACHMODE),
	PSI915_CORE(DRM_IOCTL_MODE_GETPROPERTY),
	PSI915_CORE(DRM_IOCTL_MODE_SETPROPERTY),
	PSI915_CORE(DRM_IOCTL_MODE_GETPROPBLOB),
	PSI915_CORE(DRM_IOCTL_MODE_GETFB),
	PSI915_CORE(DRM_IOCTL_MODE_ADDFB),
	PSI915_CORE(DRM_IOCTL_MODE_RMFB),
	PSI915_CORE(DRM_IOCTL_MODE_PAGE_FLIP),
	PSI915_CORE(DRM_IOCTL_MODE_DIRTYFB),
	PSI915_CORE(DRM_IOCTL_MODE_CREATE_DUMB),
	PSI915_CORE(DRM_IOCTL_MODE_MAP_DUMB),
	PSI915_CORE(DRM_IOCTL_MODE_DESTROY_DUMB),
	PSI915_CORE(DRM_IOCTL_MODE_GETPLANERESOURCES),
	PSI915_CORE(DRM_IOCTL_MODE_GETPLANE),
	PSI915_CORE(DRM_IOCTL_MODE_SETPLANE),
	PSI915_CORE(DRM_IOCTL_MODE_ADDFB2),
	PSI915_CORE(DRM_IOCTL_MODE_OBJ_GETPROPERTIES),
	PSI915_CORE(DRM_IOCTL_MODE_OBJ_SETPROPERTY),
	PSI915_CORE(DRM_IOCTL_MODE_CURSOR2),
	PSI915_CORE(DRM_IOCTL_MODE_ATOMIC),
	PSI915_CORE(DRM_IOCTL_MODE_CREATEPROPBLOB),
	PSI915_CORE(DRM_IOCTL_MODE_DESTROYPROPBLOB),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_CREATE),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_DESTROY),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_WAIT),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_RESET),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_SIGNAL),
	PSI915_CORE(DRM_IOCTL_MODE_CREATE_LEASE),
	PSI915_CORE(DRM_IOCTL_MODE_LIST_LESSEES),
	PSI915_CORE(DRM_IOCTL_MODE_GET_LEASE),
	PSI915_CORE(DRM_IOCTL_MODE_REVOKE_LEASE),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_QUERY),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_TRANSFER),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL),
	PSI915_CORE(DRM_IOCTL_MODE_GETFB2),
	PSI915_CORE(DRM_IOCTL_SYNCOBJ_EVENTFD),
	PSI915_CORE(DRM_IOCTL_MODE_CLOSEFB),
	PSI915_CORE(DRM_IOCTL_SET_CLIENT_NAME),
};

uint32_t
psi915_drm_kcmd(struct lkpi_file *f, unsigned int type, unsigned int nr)
{
	struct drm_file *df;
	const struct drm_driver *drv;

	if (type != DRM_IOCTL_BASE || nr > 0xff)
		return (0);
	if (nr < DRM_COMMAND_BASE || nr >= DRM_COMMAND_END)
		return (psi915_core_cmds[nr]);
	if (f == NULL || (df = f->private_data) == NULL ||
	    df->minor == NULL || df->minor->dev == NULL)
		return (0);
	drv = df->minor->dev->driver;
	if (nr - DRM_COMMAND_BASE >= (unsigned int)drv->num_ioctls)
		return (0);
	return (drv->ioctls[nr - DRM_COMMAND_BASE].cmd);
}

/*
 * Module parameters settable from i915.conf (lkpi_module_params_apply):
 * the unconditionally registered ones of i915_params.c and
 * display/intel_display_params.c for the gen9.config build. A name listed
 * here that the build does not define fails at modload (undefined symbol),
 * so keep it in step with CONFIG_* (error_capture needs
 * CONFIG_DRM_I915_CAPTURE_ERROR, inject_probe_failure CONFIG_DRM_I915_DEBUG,
 * request_timeout_ms a non-zero CONFIG_DRM_I915_REQUEST_TIMEOUT).
 */
#define	PSI915_PARAMS(X)						\
	X(modeset) X(reset) X(enable_hangcheck) X(force_probe) X(memtest)	\
	X(mmio_debug) X(enable_guc) X(guc_log_level) X(guc_firmware_path)	\
	X(huc_firmware_path) X(request_timeout_ms)			\
	X(dmc_firmware_path) X(vbt_firmware) X(enable_dc)		\
	X(disable_power_well) X(enable_ips) X(invert_brightness)		\
	X(enable_dpcd_backlight) X(disable_display) X(verbose_state_checks) \
	X(enable_fbc) X(enable_psr) X(enable_dp_mst)

#define	PSI915_PARAM_EXTERN(n)	LKPI_PARAM_DECLARE(n);
PSI915_PARAMS(PSI915_PARAM_EXTERN)

#define	PSI915_PARAM_REF(n)	LKPI_PARAM_REF(n),
static const struct lkpi_module_param *const psi915_params[] = {
	PSI915_PARAMS(PSI915_PARAM_REF)
};

const struct lkpi_module_param *const *
psi915_i915_params(size_t *n)
{
	*n = ARRAY_SIZE(psi915_params);
	return (psi915_params);
}

/* drm_bsd/drivers/video/nomodeset.c (no header declares it; docs/11). */
void video_bsd_set_firmware_drivers_only(bool on);

void
psi915_set_nomodeset(int on)
{
	video_bsd_set_firmware_drivers_only(on != 0);
}

/*
 * Console hooks. NOT IMPLEMENTED YET: both need i915 changes that belong in
 * the patch series (docs/11 section 5, requests R-P1..R-P3):
 *
 *  setmode(KD_TEXT): restore the console scanout, i.e. commit the saved
 *    console configuration (the firmware framebuffer object kept pinned at
 *    its original GGTT offset, on the pipe and connector it was found on)
 *    with the atomic helpers, as drm_fb_helper_restore_fbdev_mode_unlocked()
 *    does for fbdev. Until then, leaving KD_GRAPHICS shows whatever the last
 *    DRM master displayed, and tem draws into the firmware framebuffer
 *    without it being visible.
 *  panic: lock-free MMIO rewrite of PLANE_SURF/PLANE_STRIDE/PLANE_CTL of the
 *    active pipe's primary plane, pointing it at the console framebuffer.
 */
int
psi915_drm_console_setmode(void *pdev, int kd_text)
{
	(void) pdev;
	(void) kd_text;
	return (0);
}

void
psi915_drm_console_panic(void *pdev)
{
	(void) pdev;
}
