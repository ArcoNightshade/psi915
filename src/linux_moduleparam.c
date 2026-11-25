/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Module parameters (see <linux/moduleparam.h>). Clean-room.
 * In the kernel, values come from driver.conf(5) properties via
 * ddi_prop_get_int(9F)/ddi_prop_lookup_string(9F). UNTESTED ON ILLUMOS.
 */

#include <linuxkpi/platform.h>
#ifdef LINUXKPI_ILLUMOS_KERNEL
#include <sys/sunddi.h>
#endif

#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/kstrtox.h>
#include <linux/string.h>
#include <linux/slab.h>

int
param_set_int(const char *val, const struct kernel_param *kp)
{
	return (kstrtoint(val, 0, kp->arg));
}

int
param_get_int(char *buf, const struct kernel_param *kp)
{
	return (snprintf(buf, 32, "%d\n", *(int *)kp->arg));
}

int
param_set_uint(const char *val, const struct kernel_param *kp)
{
	return (kstrtouint(val, 0, kp->arg));
}

int
param_get_uint(char *buf, const struct kernel_param *kp)
{
	return (snprintf(buf, 32, "%u\n", *(unsigned int *)kp->arg));
}

int
param_set_ulong(const char *val, const struct kernel_param *kp)
{
	return (kstrtoul(val, 0, kp->arg));
}

int
param_get_ulong(char *buf, const struct kernel_param *kp)
{
	return (snprintf(buf, 32, "%lu\n", *(unsigned long *)kp->arg));
}

int
param_set_bool(const char *val, const struct kernel_param *kp)
{
	if (val == NULL || *val == '\0') {
		*(bool *)kp->arg = true;
		return (0);
	}
	return (kstrtobool(val, kp->arg));
}

int
param_get_bool(char *buf, const struct kernel_param *kp)
{
	return (snprintf(buf, 4, "%c\n", *(bool *)kp->arg ? 'Y' : 'N'));
}

/* The copy is never freed: parameters live as long as the module. */
int
param_set_charp(const char *val, const struct kernel_param *kp)
{
	char *s = kstrdup(val, GFP_KERNEL);

	if (s == NULL)
		return (-ENOMEM);
	*(char **)kp->arg = s;
	return (0);
}

int
param_get_charp(char *buf, const struct kernel_param *kp)
{
	const char *s = *(char **)kp->arg;

	return (snprintf(buf, 1024, "%s\n", s != NULL ? s : "(null)"));
}

const struct kernel_param_ops param_ops_int = { 0, param_set_int, param_get_int, NULL };
const struct kernel_param_ops param_ops_uint = { 0, param_set_uint, param_get_uint, NULL };
const struct kernel_param_ops param_ops_ulong = { 0, param_set_ulong, param_get_ulong, NULL };
const struct kernel_param_ops param_ops_bool = { 0, param_set_bool, param_get_bool, NULL };
const struct kernel_param_ops param_ops_charp = { 0, param_set_charp, param_get_charp, NULL };

static int
lkpi_param_store_s(void *var, size_t size, long long v)
{
	switch (size) {
	case 1:
		if (v < -128 || v > 255)
			return (-ERANGE);
		*(s8 *)var = (s8)v;
		return (0);
	case 2:
		if (v < -32768 || v > 65535)
			return (-ERANGE);
		*(s16 *)var = (s16)v;
		return (0);
	case 4:
		if (v < INT_MIN || v > (long long)UINT_MAX)
			return (-ERANGE);
		*(s32 *)var = (s32)v;
		return (0);
	case 8:
		*(s64 *)var = (s64)v;
		return (0);
	default:
		return (-EINVAL);
	}
}

int
lkpi_module_param_set(const struct lkpi_module_param *p, const char *val)
{
	long long sv;
	unsigned long long uv;
	bool b;
	int r;

	switch (p->type) {
	case LKPI_PARAM_BYTE:
	case LKPI_PARAM_USHORT:
	case LKPI_PARAM_UINT:
	case LKPI_PARAM_ULONG:
	case LKPI_PARAM_ULLONG:
		r = kstrtoull(val, 0, &uv);
		if (r != 0)
			return (r);
		if (p->size < 8 && uv >> (p->size * 8) != 0)
			return (-ERANGE);
		return (lkpi_param_store_s(p->var, p->size, (long long)uv));
	case LKPI_PARAM_SHORT:
	case LKPI_PARAM_INT:
	case LKPI_PARAM_LONG:
		r = kstrtoll(val, 0, &sv);
		if (r != 0)
			return (r);
		return (lkpi_param_store_s(p->var, p->size, sv));
	case LKPI_PARAM_BOOL:
	case LKPI_PARAM_INVBOOL:
		r = kstrtobool(val, &b);
		if (r != 0)
			return (r);
		if (p->type == LKPI_PARAM_INVBOOL)
			b = !b;
		if (p->size == sizeof (bool))
			*(bool *)p->var = b;
		else
			*(int *)p->var = b;
		return (0);
	case LKPI_PARAM_CHARP: {
		char *s = kstrdup(val, GFP_KERNEL);

		if (s == NULL)
			return (-ENOMEM);
		*(char **)p->var = s;
		return (0);
	}
	case LKPI_PARAM_STRING:
		if (strlen(val) >= p->size)
			return (-ENOSPC);
		(void) strscpy(p->var, val, p->size);
		return (0);
	case LKPI_PARAM_CB:
		if (p->kp.ops == NULL || p->kp.ops->set == NULL)
			return (-EINVAL);
		return (p->kp.ops->set(val, &p->kp));
	default:
		return (-EINVAL);
	}
}

#ifdef LINUXKPI_ILLUMOS_KERNEL

static bool
lkpi_param_is_string(const struct lkpi_module_param *p)
{
	return (p->type == LKPI_PARAM_CHARP || p->type == LKPI_PARAM_STRING ||
	    p->type == LKPI_PARAM_CB);
}

int
lkpi_module_params_apply(void *dipp, const struct lkpi_module_param *const *tbl,
    size_t n)
{
	dev_info_t *dip = dipp;
	char buf[24], *s;
	size_t i;
	int set = 0, v;

	for (i = 0; i < n; i++) {
		const struct lkpi_module_param *p = tbl[i];

		if (p->type == LKPI_PARAM_ARRAY ||
		    ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    (char *)p->name) == 0)
			continue;
		/* Strings first; for numbers a string form is accepted too. */
		if (ddi_prop_lookup_string(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, (char *)p->name, &s) ==
		    DDI_PROP_SUCCESS) {
			if (lkpi_module_param_set(p, s) == 0)
				set++;
			else
				cmn_err(CE_WARN, "linuxkpi: bad value for "
				    "parameter %s", p->name);
			ddi_prop_free(s);
			continue;
		}
		if (lkpi_param_is_string(p))
			continue;
		v = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    (char *)p->name, 0);
		(void) snprintf(buf, sizeof (buf), "%d", v);
		if (lkpi_module_param_set(p, buf) == 0)
			set++;
	}
	return (set);
}

#else

int
lkpi_module_params_apply(void *lookup, const struct lkpi_module_param *const *tbl,
    size_t n)
{
	const char *(*fn)(const char *) =
	    (const char *(*)(const char *))lookup;
	const char *val;
	size_t i;
	int set = 0;

	for (i = 0; i < n; i++) {
		val = fn(tbl[i]->name);
		if (val != NULL && lkpi_module_param_set(tbl[i], val) == 0)
			set++;
	}
	return (set);
}

#endif
