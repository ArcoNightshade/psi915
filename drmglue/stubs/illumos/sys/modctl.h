/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
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
