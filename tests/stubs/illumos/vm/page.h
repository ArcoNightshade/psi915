/* Stub of illumos <vm/page.h> for compile checks only. */
#ifndef _VM_PAGE_H
#define	_VM_PAGE_H

#include <sys/types.h>

typedef struct page {
	pfn_t	p_pagenum;
} page_t;

extern page_t *page_numtopp_nolock(pfn_t);

#endif
