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

/* drmglue/include/drmglue/illumos.h; linked into this module. */
int drmglue_init(void);
void drmglue_fini(void);

const uint32_t linuxkpi_version = LINUXKPI_VERSION;

/*
 * Subsystems in start-up order; torn down in reverse. Each init returns 0
 * or an errno; a failure unwinds the ones already started.
 */
static const struct {
	int	(*init)(void);
	void	(*fini)(void);
} lkpi_subsys[] = {
	{ lkpi_sched_init, lkpi_sched_fini_last },
	{ lkpi_rcu_init, lkpi_rcu_fini },
	{ lkpi_timer_init, lkpi_timer_fini },
	{ lkpi_workqueue_init, lkpi_workqueue_fini },
	/* drmglue: Linux file layer and DRM cdev (docs/11 R-K2). */
	{ drmglue_init, drmglue_fini },
};

#define	LKPI_NSUBSYS	(sizeof (lkpi_subsys) / sizeof (lkpi_subsys[0]))

static void
lkpi_subsys_unwind(unsigned int n)
{
	while (n > 0) {
		n--;
		lkpi_subsys[n].fini();
	}
}

static int
lkpi_subsys_init(void)
{
	unsigned int i;
	int error;

	for (i = 0; i < LKPI_NSUBSYS; i++) {
		error = lkpi_subsys[i].init();
		if (error != 0) {
			lkpi_subsys_unwind(i);
			return (error);
		}
	}
	return (0);
}

static void
lkpi_subsys_fini(void)
{
	lkpi_subsys_unwind(LKPI_NSUBSYS);
}

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

	error = lkpi_subsys_init();
	if (error != 0)
		return (error);
	error = mod_install(&linuxkpi_modlinkage);
	if (error != 0) {
		lkpi_subsys_fini();
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
		lkpi_subsys_fini();
		cmn_err(CE_CONT, "linuxkpi %s unloaded\n", LINUXKPI_RELEASE);
	}
	return (error);
}
