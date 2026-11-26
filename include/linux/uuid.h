/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. GUIDs (little-endian first three fields, as ACPI _DSM uses)
 * and UUIDs (big-endian).
 */
#ifndef _LINUXKPI_LINUX_UUID_H_
#define	_LINUXKPI_LINUX_UUID_H_

#include <linux/types.h>
#include <linux/string.h>

#define	UUID_SIZE	16
#define	UUID_STRING_LEN	36

typedef struct {
	u8	b[UUID_SIZE];
} guid_t;

typedef struct {
	u8	b[UUID_SIZE];
} uuid_t;

#define	GUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)		\
	((guid_t){ { (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, \
	    ((a) >> 24) & 0xff, (b) & 0xff, ((b) >> 8) & 0xff,		\
	    (c) & 0xff, ((c) >> 8) & 0xff,				\
	    (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) } })
#define	UUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)		\
	((uuid_t){ { ((a) >> 24) & 0xff, ((a) >> 16) & 0xff,		\
	    ((a) >> 8) & 0xff, (a) & 0xff, ((b) >> 8) & 0xff, (b) & 0xff, \
	    ((c) >> 8) & 0xff, (c) & 0xff,				\
	    (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) } })

static inline bool
guid_equal(const guid_t *a, const guid_t *b)
{
	return (memcmp(a, b, sizeof (guid_t)) == 0);
}

static inline void
guid_copy(guid_t *dst, const guid_t *src)
{
	memcpy(dst, src, sizeof (guid_t));
}

static inline bool
uuid_equal(const uuid_t *a, const uuid_t *b)
{
	return (memcmp(a, b, sizeof (uuid_t)) == 0);
}

#define	guid_is_null(g)		mem_is_zero((g), sizeof (guid_t))
#define	uuid_is_null(u)		mem_is_zero((u), sizeof (uuid_t))
#define	import_guid(dst, src)	memcpy((dst), (src), sizeof (guid_t))
#define	export_guid(dst, src)	memcpy((dst), (src), sizeof (guid_t))
bool uuid_is_valid(const char *uuid);
void guid_gen(guid_t *g);
void uuid_gen(uuid_t *u);

#endif /* _LINUXKPI_LINUX_UUID_H_ */
