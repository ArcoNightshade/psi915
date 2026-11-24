/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * linuxkpi: illumos loadable misc module providing the LinuxKPI.
 *
 * Module linkage. _init starts the KPI subsystems that own threads (the RCU
 * callback thread), then installs the module; _fini removes it and stops
 * them. Besides the mandatory _init/_info/_fini entry points this file
 * exports one data symbol (the KPI functions live in the other objects):
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
#include <linuxkpi/subsys.h>

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

	error = lkpi_rcu_init();
	if (error != 0)
		return (error);
	error = mod_install(&linuxkpi_modlinkage);
	if (error != 0) {
		lkpi_rcu_fini();
		return (error);
	}

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
	if (error == 0) {
		lkpi_rcu_fini();
		cmn_err(CE_CONT, "linuxkpi %s unloaded\n", LINUXKPI_RELEASE);
	}
	return (error);
}
