/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Intel SoC PMIC (Bay Trail/Cherry Trail DSI panels): no
 * PMIC driver, so MIPI PMIC sequence elements fail with -ENXIO.
 */
#ifndef _LINUXKPI_LINUX_MFD_INTEL_SOC_PMIC_H_
#define	_LINUXKPI_LINUX_MFD_INTEL_SOC_PMIC_H_

#include <linux/types.h>
#include <linux/errno.h>

#define	intel_soc_pmic_exec_mipi_pmic_seq_element(addr, reg, val, mask)	\
	((void)(addr), (void)(reg), (void)(val), (void)(mask), -ENXIO)

#endif /* _LINUXKPI_LINUX_MFD_INTEL_SOC_PMIC_H_ */
