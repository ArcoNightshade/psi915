/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room from the documented API (include/linux/cleanup.h kernel-doc):
 * scope-based cleanup with the compiler's cleanup attribute. DEFINE_FREE
 * and __free() free a pointer when it leaves scope; DEFINE_CLASS and
 * CLASS() declare a variable with a constructor and destructor;
 * DEFINE_GUARD, guard() and scoped_guard() hold a lock for a scope.
 */
#ifndef _LINUXKPI_LINUX_CLEANUP_H_
#define	_LINUXKPI_LINUX_CLEANUP_H_

#include <linux/compiler.h>

#define	__cleanup(f)		__attribute__((__cleanup__(f)))

#define	DEFINE_FREE(_name, _type, _free)				\
	static inline void __free_ ## _name(void *p)			\
	{								\
		_type _T = *(_type *)p;					\
		_free;							\
	}
#define	__free(_name)		__cleanup(__free_ ## _name)

#define	__get_and_null(p, nullvalue)	({				\
	__auto_type __lkpi_gp = &(p);					\
	__auto_type __lkpi_gv = *__lkpi_gp;				\
	*__lkpi_gp = (nullvalue);					\
	__lkpi_gv;							\
})
#define	no_free_ptr(p)		((__typeof__(p))__get_and_null(p, NULL))
#define	return_ptr(p)		return (no_free_ptr(p))
#define	retain_and_null_ptr(p)	((void)__get_and_null(p, NULL))

#define	DEFINE_CLASS(_name, _type, _exit, _init, _init_args...)		\
	typedef _type class_ ## _name ## _t;				\
	static inline void						\
	class_ ## _name ## _destructor(_type *p)			\
	{								\
		_type _T = *p;						\
		_exit;							\
	}								\
	static inline _type						\
	class_ ## _name ## _constructor(_init_args)			\
	{								\
		_type t = _init;					\
		return (t);						\
	}

#define	CLASS(_name, var)						\
	class_ ## _name ## _t var					\
	    __cleanup(class_ ## _name ## _destructor) =			\
	    class_ ## _name ## _constructor

#define	DEFINE_GUARD(_name, _type, _lock, _unlock)			\
	DEFINE_CLASS(_name, _type, if (_T) { _unlock; },		\
	    ({ _lock; _T; }), _type _T);				\
	static inline void *						\
	class_ ## _name ## _lock_ptr(class_ ## _name ## _t *_T)		\
	{								\
		return ((void *)(unsigned long)*_T);			\
	}

#define	__lkpi_guard_paste2(a, b)	a ## b
#define	__lkpi_guard_paste(a, b)	__lkpi_guard_paste2(a, b)
#define	guard(_name)							\
	CLASS(_name, __lkpi_guard_paste(__lkpi_guard_, __COUNTER__))
#define	scoped_guard(_name, args...)					\
	for (CLASS(_name, scope)(args), *__lkpi_done = NULL;		\
	    class_ ## _name ## _lock_ptr(&scope) && __lkpi_done == NULL; \
	    __lkpi_done = (void *)1)

#endif /* _LINUXKPI_LINUX_CLEANUP_H_ */
