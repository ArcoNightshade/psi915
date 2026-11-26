/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room.
 */
#ifndef _LINUXKPI_LINUX_DMA_DIRECTION_H_
#define	_LINUXKPI_LINUX_DMA_DIRECTION_H_

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3
};

static inline int
valid_dma_direction(enum dma_data_direction dir)
{
	return (dir == DMA_BIDIRECTIONAL || dir == DMA_TO_DEVICE ||
	    dir == DMA_FROM_DEVICE);
}

#endif /* _LINUXKPI_LINUX_DMA_DIRECTION_H_ */
