/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Linux module parameters.
 *
 * module_param*() defines the variable's descriptor, a global named
 * __lkpi_param_<LKPI_MODNAME>_<name> (struct lkpi_module_param) when the
 * build defines LKPI_MODNAME before any include (a token, e.g.
 * -DLKPI_MODNAME=i915; the
 * module build does), so modules cannot collide (docs/11 R-K4); else
 * __lkpi_param_<name>. LKPI_PARAM_SYM(name) names it and
 * LKPI_PARAM_DECLARE(name) declares it. illumos has no
 * equivalent of Linux's parameter section walked at load time, and
 * krtld neither runs constructors nor (portably) provides section bounds,
 * so a driver collects its descriptors in a table and applies them from
 * attach(9E):
 *
 *   static const struct lkpi_module_param *const i915_params[] = {
 *           LKPI_PARAM_REF(modeset), LKPI_PARAM_REF(enable_guc), ...
 *   };
 *   lkpi_module_params_apply(dip, i915_params, ARRAY_SIZE(i915_params));
 *
 * which reads each name as a driver.conf(5) property (integers as int
 * properties, charp as string properties; bool also accepts "Y"/"N"
 * strings). Plain global parameters can also be set from /etc/system
 * ("set i915:<variable> = 0"). Values are applied once; the sysfs
 * write-back of perm bits is not supported. module_param_array() is
 * accepted but only the descriptor exists (no driver.conf parsing).
 */
#ifndef _LINUXKPI_LINUX_MODULEPARAM_H_
#define	_LINUXKPI_LINUX_MODULEPARAM_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stringify.h>

enum lkpi_param_type {
	LKPI_PARAM_BYTE,
	LKPI_PARAM_SHORT,
	LKPI_PARAM_USHORT,
	LKPI_PARAM_INT,
	LKPI_PARAM_UINT,
	LKPI_PARAM_LONG,
	LKPI_PARAM_ULONG,
	LKPI_PARAM_ULLONG,
	LKPI_PARAM_BOOL,
	LKPI_PARAM_INVBOOL,
	LKPI_PARAM_CHARP,
	LKPI_PARAM_STRING,	/* fixed buffer, size = buffer length */
	LKPI_PARAM_ARRAY,
	LKPI_PARAM_CB
};

struct kernel_param;
struct kernel_param_ops {
	unsigned int	flags;
	int		(*set)(const char *val, const struct kernel_param *kp);
	int		(*get)(char *buf, const struct kernel_param *kp);
	void		(*free)(void *arg);
};

struct kernel_param {
	const char			*name;
	const struct kernel_param_ops	*ops;
	unsigned short			perm;
	void				*arg;
};

struct lkpi_module_param {
	const char		*name;
	void			*var;
	enum lkpi_param_type	type;
	size_t			size;
	struct kernel_param	kp;	/* LKPI_PARAM_CB */
};

#define	__lkpi_param_type_byte		LKPI_PARAM_BYTE
#define	__lkpi_param_type_short		LKPI_PARAM_SHORT
#define	__lkpi_param_type_ushort	LKPI_PARAM_USHORT
#define	__lkpi_param_type_int		LKPI_PARAM_INT
#define	__lkpi_param_type_uint		LKPI_PARAM_UINT
#define	__lkpi_param_type_hexint	LKPI_PARAM_UINT
#define	__lkpi_param_type_long		LKPI_PARAM_LONG
#define	__lkpi_param_type_ulong		LKPI_PARAM_ULONG
#define	__lkpi_param_type_ullong	LKPI_PARAM_ULLONG
#define	__lkpi_param_type_bool		LKPI_PARAM_BOOL
#define	__lkpi_param_type__Bool		LKPI_PARAM_BOOL
#define	__lkpi_param_type_invbool	LKPI_PARAM_INVBOOL
#define	__lkpi_param_type_charp		LKPI_PARAM_CHARP

#define	__lkpi_pcat3(a, b, c)	a ## b ## _ ## c
#define	__lkpi_pcat3x(a, b, c)	__lkpi_pcat3(a, b, c)
#ifdef LKPI_MODNAME
#define	LKPI_PARAM_SYM(name)	__lkpi_pcat3x(__lkpi_param_, LKPI_MODNAME, name)
#else
#define	LKPI_PARAM_SYM(name)	__lkpi_param_ ## name
#endif
#define	LKPI_PARAM_DECLARE(name)					\
	extern const struct lkpi_module_param LKPI_PARAM_SYM(name)
#define	LKPI_PARAM_REF(name)	(&LKPI_PARAM_SYM(name))

#define	module_param_named(n_, v_, t_, p_)				\
	const struct lkpi_module_param LKPI_PARAM_SYM(n_) __used = {	\
		.name = #n_, .var = &(v_),				\
		.type = __lkpi_param_type_##t_, .size = sizeof (v_) }
#define	module_param(n_, t_, p_)	module_param_named(n_, n_, t_, p_)
#define	module_param_unsafe(n_, t_, p_)	module_param_named(n_, n_, t_, p_)
#define	module_param_named_unsafe(n_, v_, t_, p_)			\
	module_param_named(n_, v_, t_, p_)
#define	module_param_string(n_, buf_, len_, p_)				\
	const struct lkpi_module_param LKPI_PARAM_SYM(n_) __used = {	\
		.name = #n_, .var = (buf_), .type = LKPI_PARAM_STRING,	\
		.size = (len_) }
#define	module_param_array_named(n_, arr_, t_, nump_, p_)		\
	const struct lkpi_module_param LKPI_PARAM_SYM(n_) __used = {	\
		.name = #n_, .var = (arr_), .type = LKPI_PARAM_ARRAY,	\
		.size = sizeof (arr_) }
#define	module_param_array(n_, t_, nump_, p_)				\
	module_param_array_named(n_, n_, t_, nump_, p_)
#define	module_param_cb(n_, ops_, arg_, p_)				\
	const struct lkpi_module_param LKPI_PARAM_SYM(n_) __used = {	\
		.name = #n_, .var = (arg_), .type = LKPI_PARAM_CB,	\
		.kp = { .name = #n_, .ops = (ops_), .perm = (p_),	\
		    .arg = (arg_) } }
#define	module_param_cb_unsafe(n_, ops_, arg_, p_)			\
	module_param_cb(n_, ops_, arg_, p_)
#define	MODULE_PARM_DESC(name, desc)
#define	__MODULE_PARM_TYPE(name, type)
#define	core_param(name, var, type, perm)

int param_set_int(const char *val, const struct kernel_param *kp);
int param_get_int(char *buf, const struct kernel_param *kp);
int param_set_uint(const char *val, const struct kernel_param *kp);
int param_get_uint(char *buf, const struct kernel_param *kp);
int param_set_ulong(const char *val, const struct kernel_param *kp);
int param_get_ulong(char *buf, const struct kernel_param *kp);
int param_set_bool(const char *val, const struct kernel_param *kp);
int param_get_bool(char *buf, const struct kernel_param *kp);
int param_set_charp(const char *val, const struct kernel_param *kp);
int param_get_charp(char *buf, const struct kernel_param *kp);

extern const struct kernel_param_ops param_ops_int;
extern const struct kernel_param_ops param_ops_uint;
extern const struct kernel_param_ops param_ops_ulong;
extern const struct kernel_param_ops param_ops_bool;
extern const struct kernel_param_ops param_ops_charp;

/* Set one parameter from its string form. 0 or a negative errno. */
int lkpi_module_param_set(const struct lkpi_module_param *p,
    const char *val);
/*
 * Apply driver.conf properties of dip (a dev_info_t *) to the
 * parameters. Returns the number of parameters set. In userland, dip is
 * a const char *(*)(const char *name) lookup function instead.
 */
int lkpi_module_params_apply(void *dip,
    const struct lkpi_module_param *const *tbl, size_t n);

#endif /* _LINUXKPI_LINUX_MODULEPARAM_H_ */
