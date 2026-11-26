/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Module linkage of linuxkpi_drm: the Linux DRM core (drm, drm_kms_helper,
 * drm_display_helper, ttm, drm_buddy, mipi-dsi bus, video helpers) and
 * drm_bsd/, in one illumos misc module. Depends on misc/linuxkpi and
 * misc/linuxkpi_gpl_g1 (DT_NEEDED, build/drm/Makefile).
 *
 * The Linux modules' init/exit functions are called here in Linux initcall
 * order (postcore, subsys, device/module), since krtld walks no initcall
 * section (<linux/module.h>). Exits run in reverse.
 *
 * Pure illumos source: no linux/ header is included; the
 * lkpi_initcall_<fn>/lkpi_exitcall_<fn> wrappers come from module_init() /
 * module_exit() / postcore_initcall() in the imported files.
 *
 * UNTESTED ON ILLUMOS.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <linuxkpi/ddi.h>

/* drm/drivers/gpu/drm/drm_mipi_dsi.c: postcore_initcall (docs/11 R-K1). */
int lkpi_initcall_mipi_dsi_bus_init(void);
/* drm/drivers/gpu/drm/drm_drv.c */
int lkpi_initcall_drm_core_init(void);
void lkpi_exitcall_drm_core_exit(void);
/* drm/drivers/gpu/drm/display/drm_display_helper_mod.c */
int lkpi_initcall_drm_display_helper_module_init(void);
void lkpi_exitcall_drm_display_helper_module_exit(void);
/* drm/drivers/gpu/drm/drm_buddy.c */
int lkpi_initcall_drm_buddy_module_init(void);
void lkpi_exitcall_drm_buddy_module_exit(void);
#ifdef PSI915_WITH_DRM_SCHED
/* drm/drivers/gpu/drm/scheduler/sched_fence.c */
int lkpi_initcall_drm_sched_fence_slab_init(void);
void lkpi_exitcall_drm_sched_fence_slab_fini(void);
#endif
#ifdef PSI915_DRMGLUE_HERE
int drmglue_init(void);
void drmglue_fini(void);
#endif

struct lkpi_drm_initcall {
	const char	*name;
	int		(*init)(void);
	void		(*exit)(void);
};

static const struct lkpi_drm_initcall lkpi_drm_initcalls[] = {
	{ "mipi_dsi_bus", lkpi_initcall_mipi_dsi_bus_init, NULL },
	{ "drm_core", lkpi_initcall_drm_core_init,
	    lkpi_exitcall_drm_core_exit },
	{ "drm_display_helper", lkpi_initcall_drm_display_helper_module_init,
	    lkpi_exitcall_drm_display_helper_module_exit },
	{ "drm_buddy", lkpi_initcall_drm_buddy_module_init,
	    lkpi_exitcall_drm_buddy_module_exit },
#ifdef PSI915_WITH_DRM_SCHED
	{ "drm_sched", lkpi_initcall_drm_sched_fence_slab_init,
	    lkpi_exitcall_drm_sched_fence_slab_fini },
#endif
};
#define	LKPI_DRM_NINIT	\
	(sizeof (lkpi_drm_initcalls) / sizeof (lkpi_drm_initcalls[0]))

static struct modlmisc linuxkpi_drm_modlmisc = {
	&mod_miscops, "LinuxKPI DRM core (Linux 6.14)"
};

static struct modlinkage linuxkpi_drm_modlinkage = {
	MODREV_1, { (void *)&linuxkpi_drm_modlmisc, NULL }
};

static void
linuxkpi_drm_exit_from(size_t n)
{
	while (n-- > 0) {
		if (lkpi_drm_initcalls[n].exit != NULL)
			lkpi_drm_initcalls[n].exit();
	}
}

int
_init(void)
{
	size_t i;
	int r;

#ifdef PSI915_DRMGLUE_HERE
	if ((r = drmglue_init()) != 0)
		return (r);
#endif
	for (i = 0; i < LKPI_DRM_NINIT; i++) {
		r = lkpi_drm_initcalls[i].init();
		if (r != 0) {
			cmn_err(CE_WARN, "linuxkpi_drm: %s init failed: %d",
			    lkpi_drm_initcalls[i].name, r);
			linuxkpi_drm_exit_from(i);
			r = lkpi_errno_to_illumos(r);
			goto fail;
		}
	}
	if ((r = mod_install(&linuxkpi_drm_modlinkage)) != 0) {
		linuxkpi_drm_exit_from(LKPI_DRM_NINIT);
		goto fail;
	}
	return (0);
fail:
#ifdef PSI915_DRMGLUE_HERE
	drmglue_fini();
#endif
	return (r);
}

int
_info(struct modinfo *mip)
{
	return (mod_info(&linuxkpi_drm_modlinkage, mip));
}

int
_fini(void)
{
	int r;

	if ((r = mod_remove(&linuxkpi_drm_modlinkage)) != 0)
		return (r);
	linuxkpi_drm_exit_from(LKPI_DRM_NINIT);
#ifdef PSI915_DRMGLUE_HERE
	drmglue_fini();
#endif
	return (0);
}
