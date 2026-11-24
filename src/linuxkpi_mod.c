/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * linuxkpi: illumos loadable misc module providing the LinuxKPI.
 *
 * P0: module linkage only. The module installs, prints a banner, and exports
 * exactly one data symbol besides the mandatory _init/_info/_fini entry
 * points:
 *
 *   linuxkpi_version   the value of LINUXKPI_VERSION this module was built
 *                      with (see <linuxkpi/version.h>)
 *
 * Consumer modules will link with -N misc/linuxkpi and may check
 * linuxkpi_version at their own _init.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>

#include <linuxkpi/version.h>

const uint32_t linuxkpi_version = LINUXKPI_VERSION;

static struct modlmisc linuxkpi_modlmisc = {
	&mod_miscops,
	"LinuxKPI " LINUXKPI_RELEASE
};

static struct modlinkage linuxkpi_modlinkage = {
	MODREV_1,
	{ (void *)&linuxkpi_modlmisc, NULL }
};

int
_init(void)
{
	int error;

	error = mod_install(&linuxkpi_modlinkage);
	if (error != 0)
		return (error);

	cmn_err(CE_CONT, "linuxkpi %s loaded (LINUXKPI_VERSION %u)\n",
	    LINUXKPI_RELEASE, (uint_t)linuxkpi_version);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&linuxkpi_modlinkage, modinfop));
}

int
_fini(void)
{
	int error;

	error = mod_remove(&linuxkpi_modlinkage);
	if (error == 0)
		cmn_err(CE_CONT, "linuxkpi %s unloaded\n", LINUXKPI_RELEASE);
	return (error);
}
