/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DDITYPES_H
#define	_STUB_SYS_DDITYPES_H
#include <sys/types.h>
typedef struct __ddi_acc_handle *ddi_acc_handle_t;
typedef struct __ddi_dma_handle *ddi_dma_handle_t;
typedef struct __ddi_intr_handle *ddi_intr_handle_t;
typedef struct ddi_device_acc_attr {
	ushort_t devacc_attr_version;
	uchar_t devacc_attr_endian_flags;
	uchar_t devacc_attr_dataorder;
	uchar_t devacc_attr_access;
} ddi_device_acc_attr_t;
#define	DDI_DEVICE_ATTR_V0	0x0001
#define	DDI_NEVERSWAP_ACC	0x00
#define	DDI_STRICTORDER_ACC	0x00
#define	DDI_MERGING_OK_ACC	0x02
#define	DDI_STORECACHING_OK_ACC	0x04
#define	DDI_DEFAULT_ACC		0x01
typedef struct {
	uint64_t	dmac_laddress;
	size_t		dmac_size;
	uint_t		dmac_type;
} ddi_dma_cookie_t;
#endif
