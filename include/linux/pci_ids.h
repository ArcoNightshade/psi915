/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. PCI vendor ids and class codes (PCI SIG assignments) used
 * by the DRM/i915 import.
 */
#ifndef _LINUXKPI_LINUX_PCI_IDS_H_
#define	_LINUXKPI_LINUX_PCI_IDS_H_

#define	PCI_VENDOR_ID_INTEL		0x8086
#define	PCI_VENDOR_ID_AMD		0x1022
#define	PCI_VENDOR_ID_ATI		0x1002
#define	PCI_VENDOR_ID_NVIDIA		0x10de
#define	PCI_VENDOR_ID_VMWARE		0x15ad
#define	PCI_VENDOR_ID_REDHAT_QUMRANET	0x1af4
#define	PCI_VENDOR_ID_DELL		0x1028
#define	PCI_VENDOR_ID_LENOVO		0x17aa
#define	PCI_VENDOR_ID_HP		0x103c
#define	PCI_VENDOR_ID_ASUSTEK		0x1043
#define	PCI_SUBVENDOR_ID_REDHAT_QUMRANET 0x1af4
#define	PCI_SUBDEVICE_ID_QEMU		0x1100

#define	PCI_BASE_CLASS_DISPLAY		0x03
#define	PCI_CLASS_DISPLAY_VGA		0x0300
#define	PCI_CLASS_DISPLAY_XGA		0x0301
#define	PCI_CLASS_DISPLAY_3D		0x0302
#define	PCI_CLASS_DISPLAY_OTHER		0x0380
#define	PCI_BASE_CLASS_BRIDGE		0x06
#define	PCI_CLASS_BRIDGE_HOST		0x0600
#define	PCI_CLASS_BRIDGE_ISA		0x0601
#define	PCI_CLASS_BRIDGE_PCI		0x0604
#define	PCI_CLASS_BRIDGE_OTHER		0x0680
#define	PCI_CLASS_MULTIMEDIA_AUDIO	0x0401
#define	PCI_CLASS_MULTIMEDIA_HD_AUDIO	0x0403
#define	PCI_CLASS_OTHERS		0xff

#define	PCI_DEVICE_ID_INTEL_82830_HB	0x3575

#endif /* _LINUXKPI_LINUX_PCI_IDS_H_ */
