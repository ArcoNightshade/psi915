/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for HOST_STUBS=1 syntax checking (copy of tests/stubs/illumos/sys/modctl.h,
 * which unlike drmglue/stubs has struct modldrv). */
#ifndef _STUB_SYS_MODCTL_H
#define	_STUB_SYS_MODCTL_H
#include <sys/types.h>

struct mod_ops;
extern struct mod_ops mod_miscops;
extern struct mod_ops mod_driverops;

struct modlmisc {
	struct mod_ops	*misc_modops;
	char		*misc_linkinfo;
};

struct dev_ops;
struct modldrv {
	struct mod_ops	*drv_modops;
	char		*drv_linkinfo;
	struct dev_ops	*drv_dev_ops;
};

struct modlinkage {
	int		ml_rev;
	void		*ml_linkage[7];
};

struct modinfo;

#define	MODREV_1	1

extern int mod_install(struct modlinkage *);
extern int mod_remove(struct modlinkage *);
extern int mod_info(struct modlinkage *, struct modinfo *);

extern int _init(void);
extern int _info(struct modinfo *);
extern int _fini(void);
#endif
