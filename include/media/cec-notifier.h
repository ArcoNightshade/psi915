/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. CEC notifiers: no CEC adapter can listen, so registration
 * returns NULL (callers treat that as "no CEC") and the setters accept
 * NULL.
 */
#ifndef _LINUXKPI_MEDIA_CEC_NOTIFIER_H_
#define	_LINUXKPI_MEDIA_CEC_NOTIFIER_H_

#include <linux/types.h>
#include <linux/cec.h>

struct device;
struct cec_notifier;
struct drm_connector;

static inline struct cec_notifier *
cec_notifier_conn_register(struct device *hdmi_dev, const char *port_name,
    const struct cec_connector_info *conn_info)
{
	(void) hdmi_dev;
	(void) port_name;
	(void) conn_info;
	return (NULL);
}

static inline void
cec_notifier_conn_unregister(struct cec_notifier *n)
{
	(void) n;
}

static inline void
cec_notifier_set_phys_addr(struct cec_notifier *n, u16 pa)
{
	(void) n;
	(void) pa;
}

static inline void
cec_notifier_phys_addr_invalidate(struct cec_notifier *n)
{
	(void) n;
}

static inline void
cec_fill_conn_info_from_drm(struct cec_connector_info *conn_info,
    const struct drm_connector *connector)
{
	(void) connector;
	conn_info->type = CEC_CONNECTOR_TYPE_NO_CONNECTOR;
}

#endif /* _LINUXKPI_MEDIA_CEC_NOTIFIER_H_ */
