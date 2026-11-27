/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Tracepoints compile to nothing (DTrace SDT probes are P7 work).
 */
#ifndef _LINUXKPI_LINUX_TRACEPOINT_H_
#define	_LINUXKPI_LINUX_TRACEPOINT_H_

#define	TRACE_EVENT(name, proto, args, struct, assign, print)		\
	static inline void trace_##name(proto) { }			\
	static inline bool trace_##name##_enabled(void) { return (false); }
#define	DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define	DEFINE_EVENT(template, name, proto, args)			\
	static inline void trace_##name(proto) { }			\
	static inline bool trace_##name##_enabled(void) { return (false); }
#define	DEFINE_EVENT_PRINT(template, name, proto, args, print)		\
	DEFINE_EVENT(template, name, PARAMS(proto), PARAMS(args))
#define	TRACE_EVENT_CONDITION(name, proto, args, cond, struct, assign, print) \
	TRACE_EVENT(name, PARAMS(proto), PARAMS(args), struct, assign, print)
#define	EXPORT_TRACEPOINT_SYMBOL(name)
#define	EXPORT_TRACEPOINT_SYMBOL_GPL(name)
#define	PARAMS(args...)		args
#define	TP_PROTO(args...)	args
#define	TP_ARGS(args...)	args
#define	TP_STRUCT__entry(args...)
#define	TP_fast_assign(args...)
#define	TP_printk(fmt, args...)
#define	TP_CONDITION(args...)

#endif /* _LINUXKPI_LINUX_TRACEPOINT_H_ */
