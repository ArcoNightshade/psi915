/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_VM_FAULTCODE_H
#define	_STUB_VM_FAULTCODE_H
#define	FC_OBJERR	0x3
#define	FC_CODE(fc)	((fc) & 0xff)
#define	FC_ERRNO(fc)	((unsigned)(fc) >> 8)
#define	FC_MAKE_ERR(e)	(((e) << 8) | FC_OBJERR)
#endif
