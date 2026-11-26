/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. The part of the Linux ACPI API the DRM/i915 import uses,
 * over the illumos ACPICA module (src/linux_acpi.c). Linux and illumos
 * both embed ACPICA, so union acpi_object has ACPICA's ACPI_OBJECT layout
 * and objects pass through unchanged. Provided: handles of a device
 * (ACPI_HANDLE(dev) via acpica_get_handle()), acpi_get_handle(),
 * acpi_evaluate_object(), acpi_has_method(), _DSM evaluation. ACPI
 * video/backlight, lid and notifier interfaces are stubs that report
 * "not present" (illumos has no ACPI video driver to coordinate with).
 */
#ifndef _LINUXKPI_LINUX_ACPI_H_
#define	_LINUXKPI_LINUX_ACPI_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/uuid.h>
#include <linux/mod_devicetable.h>

struct device;
struct notifier_block;

typedef void *acpi_handle;
typedef u32 acpi_status;
typedef u32 acpi_object_type;
typedef u64 acpi_size;
typedef char *acpi_string;

#define	AE_OK			((acpi_status)0)
#define	AE_ERROR		((acpi_status)1)
#define	AE_NOT_FOUND		((acpi_status)5)
#define	AE_NOT_EXIST		((acpi_status)6)
#define	AE_BAD_PARAMETER	((acpi_status)0x1001)
#define	ACPI_SUCCESS(s)		((s) == AE_OK)
#define	ACPI_FAILURE(s)		((s) != AE_OK)

#define	ACPI_TYPE_ANY		0x00
#define	ACPI_TYPE_INTEGER	0x01
#define	ACPI_TYPE_STRING	0x02
#define	ACPI_TYPE_BUFFER	0x03
#define	ACPI_TYPE_PACKAGE	0x04
#define	ACPI_TYPE_LOCAL_REFERENCE 0x14
#define	ACPI_ALLOCATE_BUFFER	((acpi_size)-1)

union acpi_object {
	acpi_object_type	type;
	struct {
		acpi_object_type	type;
		u64			value;
	} integer;
	struct {
		acpi_object_type	type;
		u32			length;
		char			*pointer;
	} string;
	struct {
		acpi_object_type	type;
		u32			length;
		u8			*pointer;
	} buffer;
	struct {
		acpi_object_type	type;
		u32			count;
		union acpi_object	*elements;
	} package;
	struct {
		acpi_object_type	type;
		acpi_object_type	actual_type;
		acpi_handle		handle;
	} reference;
};

struct acpi_object_list {
	u32			count;
	union acpi_object	*pointer;
};

struct acpi_buffer {
	acpi_size		length;
	void			*pointer;
};

acpi_handle lkpi_acpi_handle(struct device *dev);
#define	ACPI_HANDLE(dev)	lkpi_acpi_handle(dev)
#define	ACPI_COMPANION(dev)	((struct acpi_device *)NULL)
#define	ACPI_FREE(p)		lkpi_acpi_free(p)
void lkpi_acpi_free(void *p);

acpi_status acpi_get_handle(acpi_handle parent, const char *path,
    acpi_handle *ret);
acpi_status acpi_evaluate_object(acpi_handle h, const char *method,
    struct acpi_object_list *args, struct acpi_buffer *ret);
bool acpi_has_method(acpi_handle h, const char *name);
union acpi_object *acpi_evaluate_dsm(acpi_handle h, const guid_t *guid,
    u64 rev, u64 func, union acpi_object *argv4);
bool acpi_check_dsm(acpi_handle h, const guid_t *guid, u64 rev, u64 funcs);

static inline union acpi_object *
acpi_evaluate_dsm_typed(acpi_handle h, const guid_t *guid, u64 rev, u64 func,
    union acpi_object *argv4, acpi_object_type type)
{
	union acpi_object *obj = acpi_evaluate_dsm(h, guid, rev, func, argv4);

	if (obj != NULL && obj->type != type) {
		ACPI_FREE(obj);
		obj = NULL;
	}
	return (obj);
}

#define	ACPI_INIT_DSM_ARGV4(cnt, eles)					\
	{ .package.type = ACPI_TYPE_PACKAGE, .package.count = (cnt),	\
	    .package.elements = (eles) }

extern int acpi_disabled;
#define	acpi_video_register()			(0)
#define	acpi_video_unregister()			do { } while (0)
#define	acpi_video_register_backlight()		do { } while (0)
#define	acpi_video_backlight_use_native()	(true)
#define	acpi_video_get_backlight_type()		(0)
#define	acpi_lid_open()				(-ENODEV)
#define	acpi_target_system_state()		(0)
#define	acpi_reconfig_notifier_register(nb)	(0)
#define	acpi_reconfig_notifier_unregister(nb)	(0)
/* ACPI bus events (notifier payload); none are delivered. */
struct acpi_bus_event {
	struct list_head	node;
	char			device_class[20];
	char			bus_id[15];
	u32			type;
	u32			data;
};

#define	register_acpi_notifier(nb)		(0)
#define	unregister_acpi_notifier(nb)		(0)
#define	acpi_bus_get_device(h, d)		(-ENODEV)
#define	acpi_dev_present(hid, uid, hrv)		(false)
#define	acpi_os_ioremap(p, s)			ioremap_cache(p, s)
#define	ACPI_STATE_S0				0
#define	ACPI_STATE_S3				3
#define	ACPI_VIDEO_NOTIFY_PROBE			0x81

#endif /* _LINUXKPI_LINUX_ACPI_H_ */
