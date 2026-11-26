/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux user-memory access API from its
 * documented behaviour; function set cross-checked against FreeBSD's
 * LinuxKPI uaccess.h (BSD-2-Clause).
 *
 * Copies go through ddi_copyin(9F)/ddi_copyout(9F). When the current
 * task's lkpi_uaccess_kernel flag is set, the "user" pointers are kernel
 * addresses and FKIOCTL is passed (the ioctl glue sets it for FKIOCTL
 * calls from other kernel modules; lkpi_uaccess_kernel_begin/end bracket
 * such a region). ddi_copyin reports only success or failure, so on a
 * fault copy_*_user() returns the whole length as "not copied" (Linux
 * would return the uncopied tail; callers only test for non-zero).
 * access_ok() is always true: ddi_copyin validates the range itself.
 * Never call these from interrupt context.
 */
#ifndef _LINUXKPI_LINUX_UACCESS_H_
#define	_LINUXKPI_LINUX_UACCESS_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>

#define	VERIFY_READ		0
#define	VERIFY_WRITE		1
#define	access_ok(p, n)		((void)(p), (void)(n), 1)

static inline bool
lkpi_uaccess_is_kernel(void)
{
	return (current->lkpi_uaccess_kernel);
}

/* Returns the previous setting, for lkpi_uaccess_kernel_end(). */
static inline bool
lkpi_uaccess_kernel_begin(void)
{
	bool old = current->lkpi_uaccess_kernel;

	current->lkpi_uaccess_kernel = true;
	return (old);
}

static inline void
lkpi_uaccess_kernel_end(bool old)
{
	current->lkpi_uaccess_kernel = old;
}

static inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (n == 0)
		return (0);
	if (lkpi_plat_copyin(from, to, n, lkpi_uaccess_is_kernel()) != 0) {
		memset(to, 0, n);	/* Linux zeroes the uncopied part */
		return (n);
	}
	return (0);
}

static inline unsigned long __must_check
copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (n == 0)
		return (0);
	return (lkpi_plat_copyout(from, to, n, lkpi_uaccess_is_kernel()) != 0 ?
	    n : 0);
}

#define	__copy_from_user(to, from, n)		copy_from_user(to, from, n)
#define	__copy_to_user(to, from, n)		copy_to_user(to, from, n)
#define	__copy_from_user_inatomic(to, from, n)	copy_from_user(to, from, n)
#define	__copy_to_user_inatomic(to, from, n)	copy_to_user(to, from, n)
#define	copy_from_user_inatomic_nocache(to, from, n) copy_from_user(to, from, n)
#define	raw_copy_from_user(to, from, n)		copy_from_user(to, from, n)
#define	raw_copy_to_user(to, from, n)		copy_to_user(to, from, n)

/* 0 or -EFAULT. x is assigned 0 on a fault, as in Linux. */
#define	get_user(x, ptr) ({						\
	__typeof__(*(ptr)) __gu_v;					\
	int __gu_e = copy_from_user(&__gu_v, (ptr), sizeof (__gu_v)) ?	\
	    -EFAULT : 0;						\
	(x) = __gu_e == 0 ? __gu_v : (__typeof__(*(ptr)))0;		\
	__gu_e;								\
})

#define	put_user(x, ptr) ({						\
	__typeof__(*(ptr)) __pu_v = (x);				\
	copy_to_user((ptr), &__pu_v, sizeof (__pu_v)) ? -EFAULT : 0;	\
})

#define	__get_user(x, ptr)	get_user(x, ptr)
#define	__put_user(x, ptr)	put_user(x, ptr)

/* The "unsafe" forms jump to label on a fault. */
#define	user_access_begin(p, n)		access_ok(p, n)
#define	user_access_end()		do { } while (0)
#define	user_read_access_begin(p, n)	access_ok(p, n)
#define	user_read_access_end()		do { } while (0)
#define	user_write_access_begin(p, n)	access_ok(p, n)
#define	user_write_access_end()		do { } while (0)
#define	unsafe_put_user(x, ptr, label) do {				\
	if (put_user(x, ptr) != 0)					\
		goto label;						\
} while (0)
#define	unsafe_get_user(x, ptr, label) do {				\
	if (get_user(x, ptr) != 0)					\
		goto label;						\
} while (0)
#define	unsafe_copy_to_user(to, from, n, label) do {			\
	if (copy_to_user(to, from, n) != 0)				\
		goto label;						\
} while (0)

#define	pagefault_disable()		do { } while (0)
#define	pagefault_enable()		do { } while (0)
#define	pagefault_disabled()		(0)
#define	faulthandler_disabled()		(0)

/* Bytes not cleared. */
static inline unsigned long
clear_user(void __user *to, unsigned long n)
{
	static const char zero[256];
	unsigned long done = 0, chunk;

	while (done < n) {
		chunk = n - done < sizeof (zero) ? n - done : sizeof (zero);
		if (copy_to_user((char __user *)to + done, zero, chunk) != 0)
			return (n - done);
		done += chunk;
	}
	return (0);
}

#define	__clear_user(to, n)	clear_user(to, n)

/*
 * Copy a NUL-terminated string of at most count bytes. Returns its length
 * (count if no NUL in the first count bytes, string not terminated) or
 * -EFAULT.
 */
static inline long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	long r;

	if (count <= 0)
		return (0);
	r = lkpi_plat_copyinstr(src, dst, (size_t)count,
	    lkpi_uaccess_is_kernel());
	if (r == -ENAMETOOLONG)
		return (count);
	return (r);
}

static inline long
strnlen_user(const char __user *s, long n)
{
	char c;
	long i;

	for (i = 0; i < n; i++) {
		if (copy_from_user(&c, s + i, 1) != 0)
			return (0);
		if (c == '\0')
			return (i + 1);
	}
	return (n + 1);
}

/* Is the rest of the user struct beyond ksize zero? 1, 0 or -EFAULT. */
static inline int
check_zeroed_user(const void __user *from, size_t size)
{
	unsigned char c;
	size_t i;

	for (i = 0; i < size; i++) {
		if (copy_from_user(&c, (const char __user *)from + i, 1) != 0)
			return (-EFAULT);
		if (c != 0)
			return (0);
	}
	return (1);
}

/* Extensible-struct copy-in (Linux copy_struct_from_user semantics). */
static inline int
copy_struct_from_user(void *dst, size_t ksize, const void __user *src,
    size_t usize)
{
	size_t size = ksize < usize ? ksize : usize;
	int r;

	if (usize < ksize) {
		memset((char *)dst + size, 0, ksize - size);
	} else if (usize > ksize) {
		r = check_zeroed_user((const char __user *)src + ksize,
		    usize - ksize);
		if (r <= 0)
			return (r == 0 ? -E2BIG : r);
	}
	return (copy_from_user(dst, src, size) != 0 ? -EFAULT : 0);
}

static inline void __user *
u64_to_user_ptr(u64 v)
{
	return ((void __user *)(uintptr_t)v);
}

#define	untagged_addr(a)	(a)

/* kmalloc'ed copy of user memory, or ERR_PTR(-ENOMEM/-EFAULT). */
static inline void *
memdup_user(const void __user *src, size_t len)
{
	void *p = kmalloc(len, GFP_KERNEL);

	if (p == NULL)
		return (ERR_PTR(-ENOMEM));
	if (copy_from_user(p, src, len) != 0) {
		kfree(p);
		return (ERR_PTR(-EFAULT));
	}
	return (p);
}

static inline void *
vmemdup_user(const void __user *src, size_t len)
{
	void *p = kvmalloc(len, GFP_KERNEL);

	if (p == NULL)
		return (ERR_PTR(-ENOMEM));
	if (copy_from_user(p, src, len) != 0) {
		kvfree(p);
		return (ERR_PTR(-EFAULT));
	}
	return (p);
}

static inline void *
memdup_user_nul(const void __user *src, size_t len)
{
	char *p = kmalloc(len + 1, GFP_KERNEL);

	if (p == NULL)
		return (ERR_PTR(-ENOMEM));
	if (copy_from_user(p, src, len) != 0) {
		kfree(p);
		return (ERR_PTR(-EFAULT));
	}
	p[len] = '\0';
	return (p);
}

static inline void *
memdup_array_user(const void __user *src, size_t n, size_t size)
{
	size_t bytes;

	if (__builtin_mul_overflow(n, size, &bytes))
		return (ERR_PTR(-EOVERFLOW));
	return (memdup_user(src, bytes));
}

static inline void *
vmemdup_array_user(const void __user *src, size_t n, size_t size)
{
	size_t bytes;

	if (__builtin_mul_overflow(n, size, &bytes))
		return (ERR_PTR(-EOVERFLOW));
	return (vmemdup_user(src, bytes));
}

static inline char *
strndup_user(const char __user *s, long n)
{
	long len = strnlen_user(s, n);
	char *p;

	if (len == 0)
		return (ERR_PTR(-EFAULT));
	if (len > n)
		return (ERR_PTR(-EINVAL));
	p = memdup_user(s, (size_t)len);
	if (IS_ERR(p))
		return (p);
	p[len - 1] = '\0';
	return (p);
}

#define	__copy_from_user_inatomic_nocache(to, from, n)			\
	__copy_from_user_inatomic((to), (from), (n))

#endif /* _LINUXKPI_LINUX_UACCESS_H_ */
