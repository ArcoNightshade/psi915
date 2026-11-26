/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * ACPI and DMI helpers (see <linux/acpi.h>, <linux/dmi.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linuxkpi/plat_dev.h>

int acpi_disabled;

acpi_handle
lkpi_acpi_handle(struct device *dev)
{
	void *dip = dev != NULL ? lkpi_dev_dip(dev) : NULL;

	return (dip != NULL ? lkpi_plat_acpi_dev_handle(dip) : NULL);
}

void
lkpi_acpi_free(void *p)
{
	lkpi_plat_acpi_free(p);
}

acpi_status
acpi_get_handle(acpi_handle parent, const char *path, acpi_handle *ret)
{
	return (lkpi_plat_acpi_get_handle(parent, path, ret) == 0 ? AE_OK :
	    AE_NOT_FOUND);
}

/* Only ACPI_ALLOCATE_BUFFER results are supported (all KPI users). */
acpi_status
acpi_evaluate_object(acpi_handle h, const char *method,
    struct acpi_object_list *args, struct acpi_buffer *ret)
{
	void *result = NULL;
	int r;

	r = lkpi_plat_acpi_evaluate(h, method,
	    args != NULL ? args->pointer : NULL,
	    args != NULL ? (int)args->count : 0, &result);
	if (r == -ENOENT)
		return (AE_NOT_FOUND);
	if (r != 0)
		return (AE_ERROR);
	if (ret != NULL) {
		ret->pointer = result;
		ret->length = result != NULL ? sizeof (union acpi_object) : 0;
	} else {
		lkpi_plat_acpi_free(result);
	}
	return (AE_OK);
}

bool
acpi_has_method(acpi_handle h, const char *name)
{
	acpi_handle mh;

	return (h != NULL && acpi_get_handle(h, name, &mh) == AE_OK);
}

union acpi_object *
acpi_evaluate_dsm(acpi_handle h, const guid_t *guid, u64 rev, u64 func,
    union acpi_object *argv4)
{
	union acpi_object args[4];
	struct acpi_object_list list = { 4, args };
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER, NULL };
	guid_t g;

	if (h == NULL)
		return (NULL);
	guid_copy(&g, guid);
	memset(args, 0, sizeof (args));
	args[0].buffer.type = ACPI_TYPE_BUFFER;
	args[0].buffer.length = sizeof (g);
	args[0].buffer.pointer = g.b;
	args[1].integer.type = ACPI_TYPE_INTEGER;
	args[1].integer.value = rev;
	args[2].integer.type = ACPI_TYPE_INTEGER;
	args[2].integer.value = func;
	if (argv4 != NULL) {
		args[3] = *argv4;
	} else {
		args[3].package.type = ACPI_TYPE_PACKAGE;
		args[3].package.count = 0;
		args[3].package.elements = NULL;
	}
	if (acpi_evaluate_object(h, "_DSM", &list, &buf) != AE_OK)
		return (NULL);
	return (buf.pointer);
}

/* Function 0 returns a bit mask of supported functions. */
bool
acpi_check_dsm(acpi_handle h, const guid_t *guid, u64 rev, u64 funcs)
{
	union acpi_object *obj;
	u64 mask = 0;
	u32 i;

	if (funcs == 0)
		return (false);
	obj = acpi_evaluate_dsm(h, guid, rev, 0, NULL);
	if (obj == NULL)
		return (false);
	if (obj->type == ACPI_TYPE_INTEGER) {
		mask = obj->integer.value;
	} else if (obj->type == ACPI_TYPE_BUFFER) {
		for (i = 0; i < obj->buffer.length && i < 8; i++)
			mask |= (u64)obj->buffer.pointer[i] << (i * 8);
	}
	ACPI_FREE(obj);
	return ((mask & 1) != 0 && (mask & funcs) == funcs);
}

/* DMI */

const char *
dmi_get_system_info(int field)
{
	return (lkpi_plat_dmi_string(field));
}

static bool
lkpi_dmi_matches(const struct dmi_system_id *d)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(d->matches); i++) {
		const struct dmi_strmatch *m = &d->matches[i];
		const char *s;

		if (m->slot == DMI_NONE)
			continue;
		s = lkpi_plat_dmi_string(m->slot);
		if (s == NULL)
			return (false);
		if (m->exact_match ? strcmp(s, m->substr) != 0 :
		    strstr(s, m->substr) == NULL)
			return (false);
	}
	return (true);
}

static bool
lkpi_dmi_end(const struct dmi_system_id *d)
{
	return (d->ident == NULL && d->matches[0].slot == DMI_NONE &&
	    d->callback == NULL);
}

int
dmi_check_system(const struct dmi_system_id *list)
{
	const struct dmi_system_id *d;
	int n = 0;

	for (d = list; !lkpi_dmi_end(d); d++) {
		if (!lkpi_dmi_matches(d))
			continue;
		n++;
		if (d->callback != NULL && d->callback(d) != 0)
			break;
	}
	return (n);
}

const struct dmi_system_id *
dmi_first_match(const struct dmi_system_id *list)
{
	const struct dmi_system_id *d;

	for (d = list; !lkpi_dmi_end(d); d++)
		if (lkpi_dmi_matches(d))
			return (d);
	return (NULL);
}

bool
dmi_match(enum dmi_field f, const char *str)
{
	const char *s = lkpi_plat_dmi_string((int)f);

	return (s != NULL && str != NULL && strcmp(s, str) == 0);
}
