/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. PCI/PCIe configuration-space register numbers and bits as
 * defined by the PCI Local Bus and PCI Express Base specifications, with
 * the Linux macro names drivers use. Only the subset needed so far.
 */
#ifndef _LINUXKPI_LINUX_PCI_REGS_H_
#define	_LINUXKPI_LINUX_PCI_REGS_H_

#define	PCI_CFG_SPACE_SIZE	256
#define	PCI_CFG_SPACE_EXP_SIZE	4096

#define	PCI_VENDOR_ID		0x00
#define	PCI_DEVICE_ID		0x02
#define	PCI_COMMAND		0x04
#define	 PCI_COMMAND_IO		0x0001
#define	 PCI_COMMAND_MEMORY	0x0002
#define	 PCI_COMMAND_MASTER	0x0004
#define	 PCI_COMMAND_SPECIAL	0x0008
#define	 PCI_COMMAND_INVALIDATE	0x0010
#define	 PCI_COMMAND_PARITY	0x0040
#define	 PCI_COMMAND_SERR	0x0100
#define	 PCI_COMMAND_INTX_DISABLE 0x0400
#define	PCI_STATUS		0x06
#define	 PCI_STATUS_INTERRUPT	0x0008
#define	 PCI_STATUS_CAP_LIST	0x0010
#define	PCI_CLASS_REVISION	0x08
#define	PCI_REVISION_ID		0x08
#define	PCI_CLASS_PROG		0x09
#define	PCI_CLASS_DEVICE	0x0a
#define	PCI_CACHE_LINE_SIZE	0x0c
#define	PCI_LATENCY_TIMER	0x0d
#define	PCI_HEADER_TYPE		0x0e
#define	 PCI_HEADER_TYPE_MASK	0x7f
#define	 PCI_HEADER_TYPE_NORMAL	0
#define	 PCI_HEADER_TYPE_BRIDGE	1
#define	PCI_BIST		0x0f
#define	PCI_BASE_ADDRESS_0	0x10
#define	PCI_BASE_ADDRESS_1	0x14
#define	PCI_BASE_ADDRESS_2	0x18
#define	PCI_BASE_ADDRESS_3	0x1c
#define	PCI_BASE_ADDRESS_4	0x20
#define	PCI_BASE_ADDRESS_5	0x24
#define	 PCI_BASE_ADDRESS_SPACE_IO	0x01
#define	 PCI_BASE_ADDRESS_MEM_TYPE_64	0x04
#define	 PCI_BASE_ADDRESS_MEM_PREFETCH	0x08
#define	 PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define	PCI_SUBSYSTEM_VENDOR_ID	0x2c
#define	PCI_SUBSYSTEM_ID	0x2e
#define	PCI_ROM_ADDRESS		0x30
#define	PCI_CAPABILITY_LIST	0x34
#define	PCI_INTERRUPT_LINE	0x3c
#define	PCI_INTERRUPT_PIN	0x3d

#define	PCI_CAP_LIST_ID		0
#define	PCI_CAP_LIST_NEXT	1
#define	PCI_CAP_ID_PM		0x01
#define	PCI_CAP_ID_AGP		0x02
#define	PCI_CAP_ID_VPD		0x03
#define	PCI_CAP_ID_MSI		0x05
#define	PCI_CAP_ID_VNDR		0x09
#define	PCI_CAP_ID_EXP		0x10
#define	PCI_CAP_ID_MSIX		0x11
#define	PCI_CAP_ID_AF		0x13

#define	PCI_PM_CTRL		4
#define	 PCI_PM_CTRL_STATE_MASK	0x0003

#define	PCI_MSI_FLAGS		2
#define	 PCI_MSI_FLAGS_ENABLE	0x0001
#define	 PCI_MSI_FLAGS_64BIT	0x0080

/* PCI Express capability */
#define	PCI_EXP_FLAGS		0x02
#define	 PCI_EXP_FLAGS_VERS	0x000f
#define	 PCI_EXP_FLAGS_TYPE	0x00f0
#define	PCI_EXP_TYPE_ENDPOINT	0x0
#define	PCI_EXP_TYPE_LEG_END	0x1
#define	PCI_EXP_TYPE_ROOT_PORT	0x4
#define	PCI_EXP_TYPE_UPSTREAM	0x5
#define	PCI_EXP_TYPE_DOWNSTREAM	0x6
#define	PCI_EXP_TYPE_RC_END	0x9
#define	PCI_EXP_DEVCAP		0x04
#define	PCI_EXP_DEVCTL		0x08
#define	PCI_EXP_DEVSTA		0x0a
#define	PCI_EXP_LNKCAP		0x0c
#define	 PCI_EXP_LNKCAP_SLS	0x0000000f
#define	 PCI_EXP_LNKCAP_MLW	0x000003f0
#define	PCI_EXP_LNKCTL		0x10
#define	PCI_EXP_LNKSTA		0x12
#define	 PCI_EXP_LNKSTA_CLS	0x000f
#define	 PCI_EXP_LNKSTA_NLW	0x03f0
#define	PCI_EXP_SLTCAP		0x14
#define	PCI_EXP_SLTCTL		0x18
#define	PCI_EXP_SLTSTA		0x1a
#define	PCI_EXP_RTCTL		0x1c
#define	PCI_EXP_RTSTA		0x20
#define	PCI_EXP_DEVCAP2		0x24
#define	PCI_EXP_DEVCTL2		0x28
#define	PCI_EXP_LNKCAP2		0x2c
#define	PCI_EXP_LNKCTL2		0x30
#define	PCI_EXP_LNKSTA2		0x32

#define	PCI_EXT_CAP_ID(h)	((h) & 0x0000ffff)
#define	PCI_EXT_CAP_VER(h)	(((h) >> 16) & 0xf)
#define	PCI_EXT_CAP_NEXT(h)	(((h) >> 20) & 0xffc)
#define	PCI_EXT_CAP_ID_ERR	0x01
#define	PCI_EXT_CAP_ID_VNDR	0x0b
#define	PCI_EXT_CAP_ID_ACS	0x0d
#define	PCI_EXT_CAP_ID_ATS	0x0f
#define	PCI_EXT_CAP_ID_REBAR	0x15
#define	PCI_EXT_CAP_ID_PASID	0x1b

#endif /* _LINUXKPI_LINUX_PCI_REGS_H_ */
