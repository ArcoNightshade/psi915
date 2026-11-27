/* Stub of illumos <sys/buf.h> for compile checks only. */
#ifndef _SYS_BUF_H
#define	_SYS_BUF_H

#include <sys/types.h>

struct page;
struct proc;

typedef struct buf {
	int		b_flags;
	size_t		b_bcount;
	union {
		caddr_t	b_addr;
	} b_un;
	struct page	**b_shadow;
	struct proc	*b_proc;
} buf_t;

#define	B_BUSY		0x0001
#define	B_READ		0x0040
#define	B_WRITE		0x0100
#define	B_SHADOW	0x200000

extern struct buf *getrbuf(int);
extern void freerbuf(struct buf *);

#endif
