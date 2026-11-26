/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_ACPI_ACPI_H
#define	_STUB_SYS_ACPI_ACPI_H
#include <sys/types.h>
typedef uint32_t UINT32;
typedef uint32_t ACPI_STATUS;
typedef void *ACPI_HANDLE;
typedef char *ACPI_STRING;
typedef size_t ACPI_SIZE;
typedef union acpi_object ACPI_OBJECT;
typedef struct { UINT32 Count; ACPI_OBJECT *Pointer; } ACPI_OBJECT_LIST;
typedef struct { ACPI_SIZE Length; void *Pointer; } ACPI_BUFFER;
#define	AE_OK		0
#define	AE_NOT_FOUND	5
#define	ACPI_ALLOCATE_BUFFER	((ACPI_SIZE)-1)
extern ACPI_STATUS AcpiGetHandle(ACPI_HANDLE, ACPI_STRING, ACPI_HANDLE *);
extern ACPI_STATUS AcpiEvaluateObject(ACPI_HANDLE, ACPI_STRING,
    ACPI_OBJECT_LIST *, ACPI_BUFFER *);
extern void AcpiOsFree(void *);
#endif
