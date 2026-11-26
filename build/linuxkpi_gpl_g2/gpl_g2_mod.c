/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Module linkage of linuxkpi_gpl_g2, the GPL DRM helpers (docs/08 5.2):
 * upstream display/drm_hdcp_helper.c and drm_gem_framebuffer_helper.c.
 * Neither has a module_init. Depends on misc/linuxkpi, misc/linuxkpi_gpl_g1
 * and misc/linuxkpi_drm; only i915 depends on it.
 *
 * This file is BSD-2-Clause; the combined module is distributed under the
 * terms of the GPL-2.0 because of the upstream files it contains.
 *
 * UNTESTED ON ILLUMOS.
 */

#include <sys/types.h>
#include <sys/modctl.h>

static struct modlmisc gpl_g2_modlmisc = {
	&mod_miscops, "LinuxKPI DRM GPL helpers (GPL-2.0)"
};

static struct modlinkage gpl_g2_modlinkage = {
	MODREV_1, { (void *)&gpl_g2_modlmisc, NULL }
};

int
_init(void)
{
	return (mod_install(&gpl_g2_modlinkage));
}

int
_info(struct modinfo *mip)
{
	return (mod_info(&gpl_g2_modlinkage, mip));
}

int
_fini(void)
{
	return (mod_remove(&gpl_g2_modlinkage));
}
