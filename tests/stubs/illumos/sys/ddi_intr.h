/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DDI_INTR_H
#define	_STUB_SYS_DDI_INTR_H
#include <sys/types.h>
#include <sys/sunddi.h>
#define	DDI_INTR_TYPE_FIXED	0x1
#define	DDI_INTR_TYPE_MSI	0x2
#define	DDI_INTR_TYPE_MSIX	0x4
#define	DDI_INTR_ALLOC_NORMAL	0
#define	DDI_INTR_FLAG_BLOCK	0x0100
#define	DDI_INTR_CLAIMED	1
#define	DDI_INTR_UNCLAIMED	0
#define	DDI_INTR_PRI(pri)	((void *)(uintptr_t)(pri))
typedef uint_t (*ddi_intr_handler_t)(caddr_t, caddr_t);
extern int ddi_intr_get_supported_types(dev_info_t *, int *);
extern int ddi_intr_get_navail(dev_info_t *, int, int *);
extern int ddi_intr_alloc(dev_info_t *, ddi_intr_handle_t *, int, int, int,
    int *, int);
extern int ddi_intr_free(ddi_intr_handle_t);
extern int ddi_intr_get_cap(ddi_intr_handle_t, int *);
extern int ddi_intr_get_pri(ddi_intr_handle_t, uint_t *);
extern uint_t ddi_intr_get_hilevel_pri(void);
extern int ddi_intr_add_handler(ddi_intr_handle_t, ddi_intr_handler_t,
    void *, void *);
extern int ddi_intr_remove_handler(ddi_intr_handle_t);
extern int ddi_intr_enable(ddi_intr_handle_t);
extern int ddi_intr_disable(ddi_intr_handle_t);
extern int ddi_intr_block_enable(ddi_intr_handle_t *, int);
extern int ddi_intr_block_disable(ddi_intr_handle_t *, int);
#endif
