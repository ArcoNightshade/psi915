/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Module linkage of linuxkpi_gpl_g1, the dma-buf group (docs/08 5.2):
 * upstream GPL-2.0 dma-buf.c, dma-fence*.c, sync_file.c, the MIT dma-resv.c
 * and, with DRMGLUE_IN=g1 (default), the BSD drmglue file layer and DRM
 * character device glue they need (docs/10 R9, docs/11 section 1).
 * Depends on misc/linuxkpi only.
 *
 * This file is BSD-2-Clause; the combined module is distributed under the
 * terms of the GPL-2.0 because of the upstream files it contains.
 *
 * UNTESTED ON ILLUMOS.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <linuxkpi/ddi.h>

/* gpl/drivers/dma-buf/dma-buf.c: subsys_initcall / __exitcall (R-K1). */
int lkpi_initcall_dma_buf_init(void);
void lkpi_exitcall_dma_buf_deinit(void);
#ifdef PSI915_DRMGLUE_HERE
int drmglue_init(void);
void drmglue_fini(void);
#endif

static struct modlmisc gpl_g1_modlmisc = {
	&mod_miscops, "LinuxKPI dma-buf/dma-fence/sync_file (GPL-2.0)"
};

static struct modlinkage gpl_g1_modlinkage = {
	MODREV_1, { (void *)&gpl_g1_modlmisc, NULL }
};

int
_init(void)
{
	int r;

#ifdef PSI915_DRMGLUE_HERE
	/* Backend (poll heads, segdev, anon vnodes) before any file exists. */
	if ((r = drmglue_init()) != 0)
		return (r);
#endif
	r = lkpi_initcall_dma_buf_init();
	if (r != 0) {
		cmn_err(CE_WARN, "linuxkpi_gpl_g1: dma_buf_init failed: %d", r);
		r = lkpi_errno_to_illumos(r);
		goto fail;
	}
	if ((r = mod_install(&gpl_g1_modlinkage)) != 0) {
		lkpi_exitcall_dma_buf_deinit();
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
	return (mod_info(&gpl_g1_modlinkage, mip));
}

int
_fini(void)
{
	int r;

	if ((r = mod_remove(&gpl_g1_modlinkage)) != 0)
		return (r);
	lkpi_exitcall_dma_buf_deinit();
#ifdef PSI915_DRMGLUE_HERE
	drmglue_fini();
#endif
	return (0);
}
