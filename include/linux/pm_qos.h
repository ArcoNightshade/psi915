/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. PM QoS requests are recorded and have no effect (illumos has no
 * CPU latency QoS interface for drivers).
 */
#ifndef _LINUXKPI_LINUX_PM_QOS_H_
#define	_LINUXKPI_LINUX_PM_QOS_H_

#include <linux/types.h>

#define	PM_QOS_DEFAULT_VALUE		(-1)
#define	PM_QOS_CPU_LATENCY_DEFAULT_VALUE 2000000000

struct pm_qos_request {
	s32	value;
	bool	active;
};

static inline void
cpu_latency_qos_add_request(struct pm_qos_request *r, s32 v)
{
	r->value = v;
	r->active = true;
}

static inline void
cpu_latency_qos_update_request(struct pm_qos_request *r, s32 v)
{
	r->value = v;
}

static inline void
cpu_latency_qos_remove_request(struct pm_qos_request *r)
{
	r->active = false;
}

static inline bool
cpu_latency_qos_request_active(struct pm_qos_request *r)
{
	return (r->active);
}

#endif /* _LINUXKPI_LINUX_PM_QOS_H_ */
