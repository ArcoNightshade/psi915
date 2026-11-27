/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_DDIDMAREQ_H
#define	_STUB_SYS_DDIDMAREQ_H
#include <sys/types.h>
#include <sys/sunddi.h>
typedef struct ddi_dma_attr {
	uint_t		dma_attr_version;
	uint64_t	dma_attr_addr_lo;
	uint64_t	dma_attr_addr_hi;
	uint64_t	dma_attr_count_max;
	uint64_t	dma_attr_align;
	uint_t		dma_attr_burstsizes;
	uint32_t	dma_attr_minxfer;
	uint64_t	dma_attr_maxxfer;
	uint64_t	dma_attr_seg;
	int		dma_attr_sgllen;
	uint32_t	dma_attr_granular;
	uint_t		dma_attr_flags;
} ddi_dma_attr_t;
#define	DMA_ATTR_V0		0
#define	DDI_DMA_WRITE		0x0001
#define	DDI_DMA_READ		0x0002
#define	DDI_DMA_RDWR		(DDI_DMA_READ | DDI_DMA_WRITE)
#define	DDI_DMA_CONSISTENT	0x0010
#define	DDI_DMA_STREAMING	0x0040
#define	DDI_DMA_SLEEP		((int (*)(caddr_t))1)
#define	DDI_DMA_DONTWAIT	((int (*)(caddr_t))0)
#define	DDI_DMA_MAPPED		0
#define	DDI_DMA_SYNC_FORDEV	0x0
#define	DDI_DMA_SYNC_FORCPU	0x1
struct as;
extern int ddi_dma_alloc_handle(dev_info_t *, ddi_dma_attr_t *,
    int (*)(caddr_t), caddr_t, ddi_dma_handle_t *);
extern void ddi_dma_free_handle(ddi_dma_handle_t *);
extern int ddi_dma_mem_alloc(ddi_dma_handle_t, size_t, ddi_device_acc_attr_t *,
    uint_t, int (*)(caddr_t), caddr_t, caddr_t *, size_t *, ddi_acc_handle_t *);
extern void ddi_dma_mem_free(ddi_acc_handle_t *);
extern int ddi_dma_addr_bind_handle(ddi_dma_handle_t, struct as *, caddr_t,
    size_t, uint_t, int (*)(caddr_t), caddr_t, ddi_dma_cookie_t *, uint_t *);
struct buf;
extern int ddi_dma_buf_bind_handle(ddi_dma_handle_t, struct buf *, uint_t,
    int (*)(caddr_t), caddr_t, ddi_dma_cookie_t *, uint_t *);
extern int ddi_dma_unbind_handle(ddi_dma_handle_t);
extern int ddi_dma_sync(ddi_dma_handle_t, off_t, size_t, uint_t);
#endif
