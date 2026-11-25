/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos side of the DRM glue: the interface a DRM driver's dev_ops use
 * (first part) and the glue-internal interface between
 * src/cdev_illumos.c, src/segdev_illumos.c and src/anonfs_illumos.c.
 * Include after the illumos DDI headers.
 *
 * UNTESTED ON ILLUMOS (docs/10, section 6).
 */
#ifndef _DRMGLUE_ILLUMOS_H_
#define	_DRMGLUE_ILLUMOS_H_

#include <drmglue/lkpi_fs.h>
#include <drmglue/backend.h>

struct uio;
struct pollhead;
struct as;

/*
 * Driver interface. The DRM driver's dev_ops point devo_cb_ops at
 * drmglue_cb_ops and devo_getinfo at drmglue_getinfo; its _init calls
 * drmglue_init() and _fini drmglue_fini(); attach calls
 * drmglue_attach_node() for each DRM minor once drm_dev_register() has
 * created it, detach calls drmglue_detach_node().
 */
extern struct cb_ops drmglue_cb_ops;
/* ioctl command encoding accepted from userland (/etc/system tunable). */
extern int drmglue_ioctl_abi;

int drmglue_init(void);
void drmglue_fini(void);
int drmglue_attach_node(dev_info_t *dip, unsigned int node, const char *name,
    const struct file_operations *fops,
    uint32_t (*kcmd)(struct lkpi_file *, unsigned int, unsigned int));
int drmglue_detach_node(dev_info_t *dip, unsigned int node, const char *name);
int drmglue_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **result);

/*
 * Glue internal.
 */
extern major_t drmglue_major;		/* host major for anon mmaps */
extern dev_info_t *drmglue_host_dip;

/* File of a dev_t (clone open or anon mmap host minor), referenced. */
struct lkpi_file *drmglue_dev_file(dev_t dev);
int drmglue_devmap_file(struct lkpi_file *f, dev_info_t *dip,
    devmap_cookie_t dhp, offset_t off, size_t len, size_t *maplen);
int drmglue_segmap_file(struct lkpi_file *f, dev_t dev, offset_t off,
    struct as *as, caddr_t *addrp, size_t len, uint_t prot, uint_t maxprot,
    uint_t flags, cred_t *cr);
int drmglue_uio_read(struct lkpi_file *f, struct uio *uio);
int drmglue_file_ioctl(struct lkpi_file *f, int cmd, intptr_t arg, int mode,
    uint32_t (*kcmd)(struct lkpi_file *, unsigned int, unsigned int),
    int *rvalp);
int drmglue_file_chpoll(struct lkpi_file *f, short events, int anyyet,
    short *reventsp, struct pollhead **phpp);

void drmglue_seg_be_fill(struct lkpi_glue_backend *be);
void drmglue_anon_be_fill(struct lkpi_glue_backend *be);
int drmglue_anon_init(void);
void drmglue_anon_fini(void);

#endif /* _DRMGLUE_ILLUMOS_H_ */
