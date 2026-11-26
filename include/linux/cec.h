/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. HDMI CEC: the connector-info structure and the invalid
 * physical address, the parts DRM uses (no CEC adapters in the KPI).
 */
#ifndef _LINUXKPI_LINUX_CEC_H_
#define	_LINUXKPI_LINUX_CEC_H_

#include <linux/types.h>

#define	CEC_PHYS_ADDR_INVALID		0xffff

#define	CEC_CONNECTOR_TYPE_NO_CONNECTOR	0
#define	CEC_CONNECTOR_TYPE_DRM		1

struct cec_drm_connector_info {
	u32	card_no;
	u32	connector_id;
};

struct cec_connector_info {
	u32	type;
	union {
		struct cec_drm_connector_info	drm;
		u32				raw[16];
	};
};

#endif /* _LINUXKPI_LINUX_CEC_H_ */
