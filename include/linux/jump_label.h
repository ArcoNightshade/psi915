/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Static keys without code patching: a key is a plain
 * boolean read on each test (the branch hints are kept).
 */
#ifndef _LINUXKPI_LINUX_JUMP_LABEL_H_
#define	_LINUXKPI_LINUX_JUMP_LABEL_H_

#include <linux/types.h>
#include <linux/compiler.h>

struct static_key {
	int		enabled;
};
struct static_key_true {
	struct static_key key;
};
struct static_key_false {
	struct static_key key;
};

#define	STATIC_KEY_TRUE_INIT	{ .key = { .enabled = 1 } }
#define	STATIC_KEY_FALSE_INIT	{ .key = { .enabled = 0 } }
#define	DEFINE_STATIC_KEY_TRUE(name)	\
	struct static_key_true name = STATIC_KEY_TRUE_INIT
#define	DEFINE_STATIC_KEY_FALSE(name)	\
	struct static_key_false name = STATIC_KEY_FALSE_INIT
#define	DECLARE_STATIC_KEY_TRUE(name)	extern struct static_key_true name
#define	DECLARE_STATIC_KEY_FALSE(name)	extern struct static_key_false name

#define	static_key_enabled(k)	(READ_ONCE((k)->key.enabled) != 0)
#define	static_branch_likely(k)		__builtin_expect(static_key_enabled(k), 1)
#define	static_branch_unlikely(k)	__builtin_expect(static_key_enabled(k), 0)
#define	static_branch_enable(k)		WRITE_ONCE((k)->key.enabled, 1)
#define	static_branch_disable(k)	WRITE_ONCE((k)->key.enabled, 0)
#define	static_branch_inc(k)		static_branch_enable(k)
#define	static_branch_dec(k)		static_branch_disable(k)

#endif /* _LINUXKPI_LINUX_JUMP_LABEL_H_ */
