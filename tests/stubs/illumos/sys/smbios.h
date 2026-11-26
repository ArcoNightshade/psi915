/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SMBIOS_H
#define	_STUB_SYS_SMBIOS_H
#include <sys/types.h>
typedef struct smbios_hdl smbios_hdl_t;
typedef int id_t;
typedef struct { id_t smbstr_id; uint_t smbstr_type; } smbios_struct_t;
typedef struct { const char *smbb_vendor; const char *smbb_version;
    const char *smbb_reldate; } smbios_bios_t;
typedef struct { const uint8_t *smbs_uuid; } smbios_system_t;
typedef struct { const char *smbi_manufacturer; const char *smbi_product;
    const char *smbi_version; const char *smbi_serial; } smbios_info_t;
#define	SMB_TYPE_BASEBOARD	2
#define	SMB_TYPE_CHASSIS	3
extern smbios_hdl_t *ksmbios;
extern int smbios_lookup_type(smbios_hdl_t *, uint_t, smbios_struct_t *);
extern int smbios_info_common(smbios_hdl_t *, id_t, smbios_info_t *);
extern id_t smbios_info_bios(smbios_hdl_t *, smbios_bios_t *);
extern id_t smbios_info_system(smbios_hdl_t *, smbios_system_t *);
#endif
