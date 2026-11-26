/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. dma_fence tracepoints: no-ops.
 */
#ifndef _LINUXKPI_INCLUDE_TRACE_EVENTS_DMA_FENCE_H_
#define	_LINUXKPI_INCLUDE_TRACE_EVENTS_DMA_FENCE_H_

#include <linux/tracepoint.h>

struct dma_fence;
#define	LKPI_DMAF_TP(n)							\
	static inline void trace_dma_fence_##n(struct dma_fence *f) { (void)f; }
LKPI_DMAF_TP(emit)
LKPI_DMAF_TP(init)
LKPI_DMAF_TP(destroy)
LKPI_DMAF_TP(enable_signal)
LKPI_DMAF_TP(signaled)
LKPI_DMAF_TP(wait_start)
LKPI_DMAF_TP(wait_end)

#endif /* _LINUXKPI_INCLUDE_TRACE_EVENTS_DMA_FENCE_H_ */
