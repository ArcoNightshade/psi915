/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for uaccess, module parameters, ww_mutex, semaphores,
 * kstrto*, sort/bsearch, hashtable, xarray marks, mempool and SRCU.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ww_mutex.h>
#include <linux/semaphore.h>
#include <linux/kstrtox.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/hashtable.h>
#include <linux/xarray.h>
#include <linux/mempool.h>
#include <linux/srcu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/notifier.h>
#include <linux/llist.h>
#include <linux/average.h>
#include <linux/ctype.h>
#include <linux/random.h>
#include <linux/radix-tree.h>
#include <linux/iosys-map.h>
#include <linux/string_helpers.h>
#include <linux/nospec.h>
#include <linux/circ_buf.h>
#include <linux/kconfig.h>
#include <linux/cleanup.h>
#include <linux/jump_label.h>
#include <linux/minmax.h>

#include "harness.h"

TEST(uaccess)
{
	char ubuf[16] = "hello", kbuf[16];
	int uv = 7, kv = 0;
	u64 uq = 0;
	char *dup;
	struct { int a, b, c; } big = { 1, 2, 0 }, small;
	/* Test buffers are kernel memory in lkpi_ktest: use FKIOCTL mode. */
	bool old = lkpi_uaccess_kernel_begin();

	CHECK_EQ(copy_from_user(kbuf, ubuf, 6), 0);
	CHECK_EQ(strcmp(kbuf, "hello"), 0);
	CHECK_EQ(copy_to_user(ubuf, "world", 6), 0);
	CHECK_EQ(strcmp(ubuf, "world"), 0);
	CHECK_EQ(get_user(kv, &uv), 0);
	CHECK_EQ(kv, 7);
	CHECK_EQ(put_user((u64)0x1122334455ULL, &uq), 0);
	CHECK_EQ(uq, 0x1122334455LL);
	CHECK(access_ok(ubuf, 6));
#ifndef LKPI_KTEST
	/* Userland: addresses below 4096 fault. */
	CHECK_EQ(copy_from_user(kbuf, (void *)16, 4), 4);
	CHECK_EQ(copy_to_user((void *)16, kbuf, 4), 4);
	CHECK_EQ(get_user(kv, (int *)16), -EFAULT);
	CHECK_EQ(kv, 0);
	CHECK_EQ(strncpy_from_user(kbuf, (char *)16, 8), -EFAULT);
	CHECK(IS_ERR(memdup_user((void *)16, 4)));
#endif
	CHECK_EQ(strncpy_from_user(kbuf, ubuf, sizeof (kbuf)), 5);
	CHECK_EQ(strncpy_from_user(kbuf, ubuf, 3), 3);
	CHECK_EQ(clear_user(ubuf, 4), 0);
	CHECK_EQ(ubuf[0], 0);
	CHECK_EQ(ubuf[4], 'd');
	dup = memdup_user("abc", 4);
	CHECK(!IS_ERR(dup));
	if (!IS_ERR(dup)) {
		CHECK_EQ(strcmp(dup, "abc"), 0);
		kfree(dup);
	}
	dup = strndup_user("xyz", 10);
	CHECK(!IS_ERR(dup));
	if (!IS_ERR(dup))
		kfree(dup);
	/* Extensible structs: larger user struct must have zero tail. */
	CHECK_EQ(copy_struct_from_user(&small, 8, &big, 12), 0);
	big.c = 5;
	CHECK_EQ(copy_struct_from_user(&small, 8, &big, 12), -E2BIG);
	CHECK_EQ(copy_struct_from_user(&small, 12, &big, 8), 0);
	CHECK_EQ(small.c, 0);
	CHECK(u64_to_user_ptr((u64)(uintptr_t)ubuf) == (void *)ubuf);
	lkpi_uaccess_kernel_end(old);
	CHECK(!lkpi_uaccess_is_kernel() || old);
}

static int p_int = 3;
static unsigned int p_uint;
static bool p_bool;
static char *p_charp;
static char p_str[8];
static unsigned long p_ulong;
static int p_cb;

module_param(p_int, int, 0400);
module_param_named(renamed, p_uint, uint, 0400);
module_param(p_bool, bool, 0600);
module_param(p_charp, charp, 0400);
module_param_string(p_str, p_str, sizeof (p_str), 0400);
module_param(p_ulong, ulong, 0400);
module_param_cb(p_cb, &param_ops_int, &p_cb, 0400);
MODULE_PARM_DESC(p_int, "test");

#ifndef LKPI_KTEST
static const char *
param_lookup(const char *name)
{
	if (strcmp(name, "p_int") == 0)
		return ("-12");
	if (strcmp(name, "renamed") == 0)
		return ("0x10");
	if (strcmp(name, "p_bool") == 0)
		return ("Y");
	if (strcmp(name, "p_charp") == 0)
		return ("guc");
	if (strcmp(name, "p_cb") == 0)
		return ("9");
	return (NULL);
}
#endif

TEST(module_params)
{
	static const struct lkpi_module_param *const tbl[] = {
		LKPI_PARAM_REF(p_int), LKPI_PARAM_REF(renamed),
		LKPI_PARAM_REF(p_bool), LKPI_PARAM_REF(p_charp),
		LKPI_PARAM_REF(p_str), LKPI_PARAM_REF(p_ulong),
		LKPI_PARAM_REF(p_cb)
	};

	CHECK_EQ(strcmp(__lkpi_param_renamed.name, "renamed"), 0);
	CHECK(__lkpi_param_renamed.var == &p_uint);
	CHECK_EQ(lkpi_module_param_set(&__lkpi_param_p_str, "abc"), 0);
	CHECK_EQ(strcmp(p_str, "abc"), 0);
	CHECK_EQ(lkpi_module_param_set(&__lkpi_param_p_str, "toolongvalue"),
	    -ENOSPC);
	CHECK_EQ(lkpi_module_param_set(&__lkpi_param_p_ulong, "123"), 0);
	CHECK_EQ(p_ulong, 123);
	CHECK_EQ(lkpi_module_param_set(&__lkpi_param_renamed, "-1"), -EINVAL);
	CHECK_EQ(lkpi_module_param_set(&__lkpi_param_p_int, "x"), -EINVAL);
#ifndef LKPI_KTEST
	CHECK_EQ(lkpi_module_params_apply((void *)param_lookup, tbl,
	    ARRAY_SIZE(tbl)), 5);
	CHECK_EQ(p_int, -12);
	CHECK_EQ(p_uint, 16);
	CHECK(p_bool);
	CHECK(p_charp != NULL && strcmp(p_charp, "guc") == 0);
	CHECK_EQ(p_cb, 9);
	kfree(p_charp);
#else
	(void) tbl;
#endif
	CHECK(try_module_get(THIS_MODULE));
	module_put(THIS_MODULE);
}

static DEFINE_WD_CLASS(test_wd_class);
static DEFINE_WW_CLASS(test_ww_class);

TEST(ww_mutex_basic)
{
	struct ww_mutex a, b;
	struct ww_acquire_ctx c1, c2;

	ww_mutex_init(&a, &test_wd_class);
	ww_mutex_init(&b, &test_wd_class);
	ww_acquire_init(&c1, &test_wd_class);
	CHECK_EQ(ww_mutex_lock(&a, &c1), 0);
	CHECK(ww_mutex_is_locked(&a));
	CHECK(lockdep_is_held(&a.base));
	CHECK_EQ(ww_mutex_lock(&a, &c1), -EALREADY);
	CHECK_EQ(c1.acquired, 1);

	/* Younger c2 holding b meets a held by older c1: dies. */
	ww_acquire_init(&c2, &test_wd_class);
	CHECK_EQ(ww_mutex_lock(&b, &c2), 0);
	CHECK_EQ(ww_mutex_lock(&a, &c2), -EDEADLK);
	ww_mutex_unlock(&b);
	CHECK_EQ(c2.acquired, 0);
	/* Back-off done: take the contended lock via the slow path later. */
	ww_mutex_unlock(&a);
	ww_mutex_lock_slow(&a, &c2);
	CHECK_EQ(ww_mutex_lock(&b, &c2), 0);
	CHECK_EQ(ww_mutex_trylock(&b, NULL), 0);
	ww_mutex_unlock(&b);
	ww_mutex_unlock(&a);
	ww_acquire_fini(&c2);
	ww_acquire_fini(&c1);

	/* No context: a plain mutex. */
	CHECK_EQ(ww_mutex_lock(&a, NULL), 0);
	CHECK_EQ(ww_mutex_trylock(&a, NULL), 0);
	ww_mutex_unlock(&a);
	CHECK_EQ(ww_mutex_trylock(&a, NULL), 1);
	ww_mutex_unlock(&a);
	ww_mutex_destroy(&a);
	ww_mutex_destroy(&b);
}

/* Many threads take random pairs of locks with backoff; must not hang. */
#define	WW_NLOCKS	6
static struct ww_mutex ww_locks[WW_NLOCKS];
static atomic_t ww_counter[WW_NLOCKS];
static atomic_t ww_backoffs;
static atomic_t ww_seed;
static struct ww_class *ww_stress_class;

static void
ww_take_all(struct ww_mutex **l, int n)
{
	struct ww_acquire_ctx ctx;
	struct ww_mutex *contended = NULL;
	int i, r;

	ww_acquire_init(&ctx, ww_stress_class);
retry:
	if (contended != NULL)
		ww_mutex_lock_slow(contended, &ctx);
	for (i = 0; i < n; i++) {
		if (l[i] == contended)
			continue;
		r = ww_mutex_lock(l[i], &ctx);
		if (r == -EDEADLK) {
			int j;

			for (j = 0; j < i; j++)
				if (l[j] != contended)
					ww_mutex_unlock(l[j]);
			if (contended != NULL)
				ww_mutex_unlock(contended);
			contended = l[i];
			atomic_inc(&ww_backoffs);
			goto retry;
		}
	}
	ww_acquire_done(&ctx);
	for (i = 0; i < n; i++)
		atomic_inc(&ww_counter[l[i] - ww_locks]);
	for (i = 0; i < n; i++)
		ww_mutex_unlock(l[i]);
	ww_acquire_fini(&ctx);
}

static void
ww_stress(void *arg)
{
	unsigned int s = (unsigned int)atomic_inc_return(&ww_seed) * 2654435761U;
	struct ww_mutex *l[3];
	int it, k;

	(void) arg;
	for (it = 0; it < 300; it++) {
		for (k = 0; k < 3; k++) {
			s = s * 1103515245U + 12345U;
			l[k] = &ww_locks[(s >> 16) % WW_NLOCKS];
		}
		if (l[1] == l[0])
			l[1] = &ww_locks[(l[0] - ww_locks + 1) % WW_NLOCKS];
		if (l[2] == l[0] || l[2] == l[1])
			l[2] = &ww_locks[(l[0] - ww_locks + 2) % WW_NLOCKS];
		if (l[2] == l[1])
			l[2] = &ww_locks[(l[1] - ww_locks + 1) % WW_NLOCKS];
		ww_take_all(l, 3);
	}
}

TEST(ww_mutex_stress)
{
	int pass, i, total;

	for (pass = 0; pass < 2; pass++) {
		ww_stress_class = pass == 0 ? &test_wd_class : &test_ww_class;
		for (i = 0; i < WW_NLOCKS; i++) {
			ww_mutex_init(&ww_locks[i], ww_stress_class);
			atomic_set(&ww_counter[i], 0);
		}
		harness_run_threads(6, ww_stress, NULL);
		total = 0;
		for (i = 0; i < WW_NLOCKS; i++) {
			CHECK(!ww_mutex_is_locked(&ww_locks[i]));
			total += atomic_read(&ww_counter[i]);
			ww_mutex_destroy(&ww_locks[i]);
		}
		CHECK_EQ(total, 6 * 300 * 3);
	}
}

static struct semaphore sem;

static void
sem_up_later(void *arg)
{
	(void) arg;
	msleep(10);
	up(&sem);
}

TEST(semaphore)
{
	sema_init(&sem, 2);
	down(&sem);
	CHECK_EQ(down_interruptible(&sem), 0);
	CHECK_EQ(down_trylock(&sem), 1);
	CHECK_EQ(down_timeout(&sem, msecs_to_jiffies(10)), -ETIME);
	up(&sem);
	CHECK_EQ(down_trylock(&sem), 0);
	harness_run_threads(1, sem_up_later, NULL);
	CHECK_EQ(down_timeout(&sem, msecs_to_jiffies(1000)), 0);
#ifndef LKPI_KTEST
	lkpi_plat_fake_signal = 1;
	CHECK_EQ(down_interruptible(&sem), -EINTR);
	lkpi_plat_fake_signal = 0;
#endif
	up(&sem);
	up(&sem);
	lkpi_sema_destroy(&sem);
}

TEST(kstrtox)
{
	unsigned long long ull;
	long long ll;
	unsigned int ui;
	int i;
	u8 u8v;
	s8 s8v;
	bool b;
	char *end;

	CHECK_EQ(kstrtoull("12345", 10, &ull), 0);
	CHECK_EQ(ull, 12345);
	CHECK_EQ(kstrtoull("0x1f\n", 0, &ull), 0);
	CHECK_EQ(ull, 31);
	CHECK_EQ(kstrtoull("0x1f", 16, &ull), 0);
	CHECK_EQ(kstrtoull("017", 0, &ull), 0);
	CHECK_EQ(ull, 15);
	CHECK_EQ(kstrtoull("+5", 10, &ull), 0);
	CHECK_EQ(kstrtoull("-5", 10, &ull), -EINVAL);
	CHECK_EQ(kstrtoull("", 10, &ull), -EINVAL);
	CHECK_EQ(kstrtoull("12a", 10, &ull), -EINVAL);
	CHECK_EQ(kstrtoull("5\n\n", 10, &ull), -EINVAL);
	CHECK_EQ(kstrtoull("18446744073709551615", 10, &ull), 0);
	CHECK(ull == ~0ULL);
	CHECK_EQ(kstrtoull("18446744073709551616", 10, &ull), -ERANGE);
	CHECK_EQ(kstrtoll("-9223372036854775808", 10, &ll), 0);
	CHECK(ll == LLONG_MIN);
	CHECK_EQ(kstrtoll("9223372036854775808", 10, &ll), -ERANGE);
	CHECK_EQ(kstrtoint("-42", 0, &i), 0);
	CHECK_EQ(i, -42);
	CHECK_EQ(kstrtoint("2147483648", 10, &i), -ERANGE);
	CHECK_EQ(kstrtouint("4294967295", 10, &ui), 0);
	CHECK_EQ(kstrtouint("4294967296", 10, &ui), -ERANGE);
	CHECK_EQ(kstrtou8("256", 10, &u8v), -ERANGE);
	CHECK_EQ(kstrtou8("255", 10, &u8v), 0);
	CHECK_EQ(kstrtos8("-128", 10, &s8v), 0);
	CHECK_EQ(kstrtos8("-129", 10, &s8v), -ERANGE);
	CHECK_EQ(kstrtobool("on", &b), 0);
	CHECK(b);
	CHECK_EQ(kstrtobool("N", &b), 0);
	CHECK(!b);
	CHECK_EQ(kstrtobool("x", &b), -EINVAL);
	{
		bool old = lkpi_uaccess_kernel_begin();

		CHECK_EQ(kstrtoint_from_user("77\n", 3, 10, &i), 0);
		lkpi_uaccess_kernel_end(old);
	}
	CHECK_EQ(i, 77);
	CHECK_EQ(simple_strtoul("123xyz", &end, 10), 123);
	CHECK_EQ(*end, 'x');
	CHECK_EQ(simple_strtol("-0x10", NULL, 0), -16);
}

static int
cmp_int(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;

	return (x < y ? -1 : x > y);
}

static void
swap_int(void *a, void *b, int size)
{
	int t = *(int *)a;

	(void) size;
	*(int *)a = *(int *)b;
	*(int *)b = t;
}

struct hnode {
	int			key;
	struct hlist_node	node;
};

TEST(sort_bsearch_hashtable)
{
	int v[200], i, key, *found;
	DEFINE_HASHTABLE(ht, 4);
	struct hnode nodes[40], *pos;
	struct hlist_node *tmp;
	unsigned int bkt;
	int count;

	for (i = 0; i < 200; i++)
		v[i] = (i * 7919) % 211;
	sort(v, 200, sizeof (int), cmp_int, NULL);
	for (i = 1; i < 200; i++)
		CHECK(v[i - 1] <= v[i]);
	for (i = 0; i < 200; i++)
		v[i] = 200 - i;
	sort(v, 200, sizeof (int), cmp_int, swap_int);
	for (i = 0; i < 200; i++)
		CHECK_EQ(v[i], i + 1);
	sort(v, 1, sizeof (int), cmp_int, NULL);
	key = 57;
	found = bsearch(&key, v, 200, sizeof (int), cmp_int);
	CHECK(found != NULL && *found == 57);
	key = 999;
	CHECK(bsearch(&key, v, 200, sizeof (int), cmp_int) == NULL);
	CHECK(bsearch(&key, v, 0, sizeof (int), cmp_int) == NULL);

	CHECK(hash_empty(ht));
	CHECK_EQ(HASH_SIZE(ht), 16);
	CHECK_EQ(HASH_BITS(ht), 4);
	for (i = 0; i < 40; i++) {
		nodes[i].key = i * 3;
		hash_add(ht, &nodes[i].node, nodes[i].key);
	}
	CHECK(!hash_empty(ht));
	count = 0;
	hash_for_each_possible(ht, pos, node, 27)
		if (pos->key == 27)
			count++;
	CHECK_EQ(count, 1);
	count = 0;
	hash_for_each(ht, bkt, pos, node)
		count++;
	CHECK_EQ(count, 40);
	hash_for_each_safe(ht, bkt, tmp, pos, node)
		hash_del(&pos->node);
	CHECK(hash_empty(ht));
	CHECK(!hash_hashed(&nodes[0].node));
}

TEST(xarray_marks)
{
	DEFINE_XARRAY(xa);
	unsigned long idx;
	void *e;
	int n;

	for (idx = 0; idx < 300; idx += 3)
		CHECK(xa_store(&xa, idx, xa_mk_value(idx), GFP_KERNEL) == NULL);
	CHECK(!xa_marked(&xa, XA_MARK_0));
	xa_set_mark(&xa, 3, XA_MARK_0);
	xa_set_mark(&xa, 297, XA_MARK_0);
	xa_set_mark(&xa, 150, XA_MARK_1);
	xa_set_mark(&xa, 4, XA_MARK_0);	/* empty: no-op */
	CHECK(xa_marked(&xa, XA_MARK_0));
	CHECK(xa_get_mark(&xa, 3, XA_MARK_0));
	CHECK(!xa_get_mark(&xa, 3, XA_MARK_1));
	CHECK(!xa_get_mark(&xa, 4, XA_MARK_0));
	n = 0;
	xa_for_each_marked(&xa, idx, e, XA_MARK_0) {
		CHECK(idx == 3 || idx == 297);
		CHECK_EQ(xa_to_value(e), idx);
		n++;
	}
	CHECK_EQ(n, 2);
	idx = 4;
	e = xa_find(&xa, &idx, ULONG_MAX, XA_MARK_0);
	CHECK_EQ(idx, 297);
	xa_clear_mark(&xa, 297, XA_MARK_0);
	CHECK(!xa_get_mark(&xa, 297, XA_MARK_0));
	/* Erase clears marks; re-store does not resurrect them. */
	(void) xa_erase(&xa, 3);
	CHECK(!xa_marked(&xa, XA_MARK_0));
	(void) xa_store(&xa, 3, xa_mk_value(3), GFP_KERNEL);
	CHECK(!xa_get_mark(&xa, 3, XA_MARK_0));
	/* Replace keeps marks. */
	(void) xa_store(&xa, 150, xa_mk_value(1), GFP_KERNEL);
	CHECK(xa_get_mark(&xa, 150, XA_MARK_1));
	/* Growing the tree keeps marks reachable. */
	(void) xa_store(&xa, 1UL << 20, xa_mk_value(5), GFP_KERNEL);
	xa_set_mark(&xa, 1UL << 20, XA_MARK_2);
	n = 0;
	xa_for_each_marked(&xa, idx, e, XA_MARK_1)
		n++;
	CHECK_EQ(n, 1);
	n = 0;
	xa_for_each_marked(&xa, idx, e, XA_MARK_2)
		n += idx == (1UL << 20);
	CHECK_EQ(n, 1);
	xa_destroy(&xa);
}

static atomic_t mp_allocs;

static void *
mp_alloc(gfp_t gfp, void *data)
{
	(void) data;
	if (atomic_read(&mp_allocs) >= 4 && !(gfp & __GFP_DIRECT_RECLAIM))
		return (NULL);	/* simulate pressure for non-sleeping tries */
	atomic_inc(&mp_allocs);
	return (kmalloc(32, gfp));
}

static void
mp_free(void *e, void *data)
{
	(void) data;
	atomic_dec(&mp_allocs);
	kfree(e);
}

TEST(mempool)
{
	mempool_t *mp;
	void *e[6];
	int i;

	atomic_set(&mp_allocs, 0);
	mp = mempool_create(2, mp_alloc, mp_free, NULL);
	CHECK(mp != NULL);
	if (mp == NULL)
		return;
	CHECK(mempool_initialized(mp));
	CHECK_EQ(mp->curr_nr, 2);
	for (i = 0; i < 4; i++)
		e[i] = mempool_alloc(mp, GFP_KERNEL);
	/* 4 allocated (2 via allocator, then the 2 reserve elements). */
	for (i = 0; i < 4; i++)
		CHECK(e[i] != NULL);
	CHECK_EQ(mp->curr_nr, 0);
	CHECK(mempool_alloc(mp, GFP_ATOMIC) == NULL);
	for (i = 0; i < 4; i++)
		mempool_free(e[i], mp);
	CHECK_EQ(mp->curr_nr, 2);
	CHECK_EQ(mempool_resize(mp, 3), 0);
	mempool_destroy(mp);
	CHECK_EQ(atomic_read(&mp_allocs), 0);

	mp = mempool_create_kmalloc_pool(1, 64);
	e[0] = mempool_alloc(mp, GFP_KERNEL);
	CHECK(e[0] != NULL);
	mempool_free(e[0], mp);
	mempool_destroy(mp);
}

static struct srcu_struct srcu_dom;
static int *srcu_ptr;
static atomic_t srcu_bad;
static atomic_t srcu_stop;

static void
srcu_reader(void *arg)
{
	int idx, *p;

	(void) arg;
	while (!atomic_read(&srcu_stop)) {
		idx = srcu_read_lock(&srcu_dom);
		p = READ_ONCE(srcu_ptr);
		usleep_range(50, 100);	/* readers may sleep */
		if (READ_ONCE(*p) != 42)
			atomic_inc(&srcu_bad);
		srcu_read_unlock(&srcu_dom, idx);
	}
}

static void
srcu_writer(void *arg)
{
	int i, *old, *nw;

	(void) arg;
	for (i = 0; i < 50; i++) {
		nw = kmalloc(sizeof (*nw), GFP_KERNEL);
		*nw = 42;
		old = srcu_ptr;
		WRITE_ONCE(srcu_ptr, nw);
		synchronize_srcu(&srcu_dom);
		*old = 0;	/* poison: readers must not see this */
		kfree(old);
	}
	atomic_set(&srcu_stop, 1);
}

static atomic_t srcu_role;

static void
srcu_role_fn(void *arg)
{
	if (atomic_inc_return(&srcu_role) == 1)
		srcu_writer(arg);
	else
		srcu_reader(arg);
}

TEST(srcu)
{
	DEFINE_STATIC_SRCU(s2);
	int idx;

	CHECK_EQ(init_srcu_struct(&srcu_dom), 0);
	idx = srcu_read_lock(&s2);
	/* Another domain's reader does not block this domain. */
	synchronize_srcu(&srcu_dom);
	srcu_read_unlock(&s2, idx);
	synchronize_srcu(&s2);

	srcu_ptr = kmalloc(sizeof (int), GFP_KERNEL);
	*srcu_ptr = 42;
	atomic_set(&srcu_bad, 0);
	atomic_set(&srcu_stop, 0);
	atomic_set(&srcu_role, 0);
	harness_run_threads(5, srcu_role_fn, NULL);
	CHECK_EQ(atomic_read(&srcu_bad), 0);
	kfree(srcu_ptr);
	cleanup_srcu_struct(&srcu_dom);
}

static int nb_order[4], nb_n;

static int
nb_fn(struct notifier_block *nb, unsigned long action, void *data)
{
	(void) data;
	nb_order[nb_n++] = nb->priority;
	return (action == 7 && nb->priority == 5 ? NOTIFY_STOP : NOTIFY_OK);
}

struct lnode {
	int			v;
	struct llist_node	ln;
};

DECLARE_EWMA(test, 4, 8)

#define	LKPI_TEST_OPT_ON	1

TEST(p3_helpers)
{
	BLOCKING_NOTIFIER_HEAD(bh);
	struct atomic_notifier_head ah;
	struct notifier_block a = { nb_fn, NULL, 1 }, b = { nb_fn, NULL, 5 },
	    c = { nb_fn, NULL, 3 };
	LLIST_HEAD(lh);
	struct lnode n[3], *pos, *tmp;
	struct llist_node *all;
	struct ewma_test e;
	DEFINE_XARRAY(rt);
	u32 regs[4] = { 0 };
	struct iosys_map m = IOSYS_MAP_INIT_VADDR(regs);
	int i, sum;

	CHECK_EQ(blocking_notifier_chain_register(&bh, &a), 0);
	CHECK_EQ(blocking_notifier_chain_register(&bh, &b), 0);
	CHECK_EQ(blocking_notifier_chain_register(&bh, &c), 0);
	CHECK_EQ(blocking_notifier_chain_register(&bh, &c), -EEXIST);
	nb_n = 0;
	CHECK_EQ(blocking_notifier_call_chain(&bh, 1, NULL), NOTIFY_OK);
	CHECK_EQ(nb_n, 3);
	CHECK_EQ(nb_order[0], 5);
	CHECK_EQ(nb_order[2], 1);
	nb_n = 0;
	CHECK_EQ(blocking_notifier_call_chain(&bh, 7, NULL), NOTIFY_STOP);
	CHECK_EQ(nb_n, 1);
	CHECK_EQ(blocking_notifier_chain_unregister(&bh, &b), 0);
	CHECK_EQ(blocking_notifier_chain_unregister(&bh, &b), -ENOENT);
	ATOMIC_INIT_NOTIFIER_HEAD(&ah);
	CHECK_EQ(atomic_notifier_chain_register(&ah, &a), 0);
	nb_n = 0;
	(void) atomic_notifier_call_chain(&ah, 0, NULL);
	CHECK_EQ(nb_n, 1);
	CHECK_EQ(atomic_notifier_chain_unregister(&ah, &a), 0);
	CHECK_EQ(notifier_to_errno(notifier_from_errno(-EINVAL)), -EINVAL);

	for (i = 0; i < 3; i++) {
		n[i].v = i;
		CHECK_EQ(llist_add(&n[i].ln, &lh), i == 0);
	}
	all = llist_del_all(&lh);
	CHECK(llist_empty(&lh));
	sum = 0;
	llist_for_each_entry_safe(pos, tmp, all, ln)
		sum = sum * 10 + pos->v;
	CHECK_EQ(sum, 210);		/* newest first */
	all = llist_reverse_order(all);
	CHECK(llist_entry(all, struct lnode, ln)->v == 0);

	ewma_test_init(&e);
	ewma_test_add(&e, 100);
	CHECK_EQ(ewma_test_read(&e), 100);
	for (i = 0; i < 50; i++)
		ewma_test_add(&e, 20);
	CHECK(ewma_test_read(&e) >= 20 && ewma_test_read(&e) < 25);

	CHECK(isdigit('7') && !isdigit('a') && isxdigit('F') && isspace('\t'));
	CHECK_EQ(tolower('Q'), 'q');
	CHECK_EQ(toupper('q'), 'Q');
	CHECK(get_random_u32_below(10) < 10);
	(void) get_random_u64();

	CHECK_EQ(radix_tree_insert(&rt, 5, &e), 0);
	CHECK(radix_tree_lookup(&rt, 5) == &e);
	CHECK(radix_tree_delete(&rt, 5) == &e);
	xa_destroy(&rt);

	iosys_map_wr(&m, 4, u32, 0xabcd);
	CHECK_EQ(regs[1], 0xabcd);
	CHECK_EQ(iosys_map_rd(&m, 4, u32), 0xabcd);
	iosys_map_memset(&m, 8, 0xff, 4);
	CHECK_EQ(regs[2], 0xffffffff);
	iosys_map_incr(&m, 4);
	CHECK_EQ(iosys_map_rd(&m, 0, u32), 0xabcd);
	CHECK(iosys_map_is_set(&m));
	iosys_map_clear(&m);
	CHECK(iosys_map_is_null(&m));

	CHECK(strcmp(str_yes_no(true), "yes") == 0);
	CHECK(strcmp(str_enabled_disabled(false), "disabled") == 0);
	CHECK_EQ(array_index_nospec(3, 5), 3);
	CHECK_EQ(array_index_nospec(7, 5), 0);
	CHECK_EQ(CIRC_CNT(5, 2, 8), 3);
	CHECK_EQ(CIRC_SPACE(5, 2, 8), 4);
	CHECK_EQ(IS_ENABLED(LKPI_TEST_OPT_ON), 1);
	CHECK_EQ(IS_ENABLED(LKPI_TEST_OPT_OFF), 0);
#if IS_ENABLED(LKPI_TEST_OPT_ON)
	CHECK(1);
#else
	CHECK(0);
#endif
}

static int cleanup_frees;

static void
cleanup_free_int(int *p)
{
	cleanup_frees++;
	kfree(p);
}

DEFINE_FREE(test_int, int *, if (_T != NULL) cleanup_free_int(_T))

struct test_cnt {
	int	*ctr;
};

DEFINE_CLASS(test_cnt, struct test_cnt, (*_T.ctr)--,
    ({ (*c)++; (struct test_cnt){ c }; }), int *c)

static DEFINE_STATIC_KEY_FALSE(test_key);

static int *
cleanup_keep(void)
{
	int *p __free(test_int) = kmalloc(sizeof (int), GFP_KERNEL);

	return_ptr(p);
}

TEST(cleanup_classes)
{
	int ctr = 0, *kept;
	char arr[MAX_T(int, 3, 5)];

	cleanup_frees = 0;
	{
		int *p __free(test_int) = kmalloc(sizeof (int), GFP_KERNEL);

		CHECK(p != NULL);
	}
	CHECK_EQ(cleanup_frees, 1);
	kept = cleanup_keep();
	CHECK(kept != NULL);
	CHECK_EQ(cleanup_frees, 1);
	kfree(kept);
	{
		CLASS(test_cnt, c)(&ctr);

		CHECK_EQ(ctr, 1);
		CHECK(c.ctr == &ctr);
	}
	CHECK_EQ(ctr, 0);
	CHECK_EQ((int)sizeof (arr), 5);
	CHECK(!static_branch_likely(&test_key));
	static_branch_enable(&test_key);
	CHECK(static_branch_unlikely(&test_key));
	BUILD_BUG_ON(sizeof (int) != 4);
	BUILD_BUG_ON(sizeof (arr) != 5 && ctr > 0);
	CHECK(typecheck(int, ctr));
}

LKPI_SUITE(misc)
{
	RUN(cleanup_classes);
	RUN(uaccess);
	RUN(module_params);
	RUN(ww_mutex_basic);
	RUN(ww_mutex_stress);
	RUN(semaphore);
	RUN(kstrtox);
	RUN(sort_bsearch_hashtable);
	RUN(xarray_marks);
	RUN(mempool);
	RUN(srcu);
	RUN(p3_helpers);
}
