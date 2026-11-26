/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_DDIDEVMAP_H
#define	_STUB_SYS_DDIDEVMAP_H
#include <sys/types.h>
#include <sys/sunddi.h>
#include <vm/seg_enum.h>
typedef void *devmap_cookie_t;
typedef struct ddi_umem_cookie *ddi_umem_cookie_t;
struct as;
struct devmap_callback_ctl {
	int	devmap_rev;
	int	(*devmap_map)(devmap_cookie_t, dev_t, uint_t, offset_t, size_t,
		    void **);
	int	(*devmap_access)(devmap_cookie_t, void *, offset_t, size_t,
		    uint_t, uint_t);
	int	(*devmap_dup)(devmap_cookie_t, void *, devmap_cookie_t,
		    void **);
	void	(*devmap_unmap)(devmap_cookie_t, void *, offset_t, size_t,
		    devmap_cookie_t, void **, devmap_cookie_t, void **);
};
#define	DEVMAP_OPS_REV		1
extern faultcode_t devmap_load(devmap_cookie_t, offset_t, size_t, uint_t,
    uint_t);
extern int devmap_unload(devmap_cookie_t, offset_t, size_t);
extern int devmap_setup(dev_t, offset_t, struct as *, caddr_t *, size_t,
    uint_t, uint_t, uint_t, cred_t *);
#endif
