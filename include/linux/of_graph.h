/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Device-tree graph endpoints: no device tree on x86
 * illumos; lookups find nothing.
 */
#ifndef _LINUXKPI_LINUX_OF_GRAPH_H_
#define	_LINUXKPI_LINUX_OF_GRAPH_H_

#include <linux/of.h>

struct of_endpoint {
	unsigned int			port;
	unsigned int			id;
	const struct device_node	*local_node;
};

#define	for_each_endpoint_of_node(parent, child)			\
	for ((void)(parent), (child) = NULL; (child) != NULL; )
#define	of_graph_get_remote_node(n, p, e)	\
	((void)(n), (void)(p), (void)(e), (struct device_node *)NULL)
#define	of_graph_get_next_endpoint(n, prev)	\
	((void)(n), (void)(prev), (struct device_node *)NULL)
#define	of_graph_get_remote_port_parent(n)	\
	((void)(n), (struct device_node *)NULL)
#define	of_graph_get_endpoint_by_regs(n, p, e)	\
	((void)(n), (void)(p), (void)(e), (struct device_node *)NULL)
#define	of_graph_parse_endpoint(n, ep)		((void)(n), (void)(ep), -ENODEV)
#define	of_graph_get_port_by_id(n, id)		\
	((void)(n), (void)(id), (struct device_node *)NULL)

#endif /* _LINUXKPI_LINUX_OF_GRAPH_H_ */
