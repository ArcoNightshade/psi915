/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for the RCU emulation (<linux/rcupdate.h>), the RCU list
 * variants (<linux/rculist.h>), kfree_rcu() and SLAB_TYPESAFE_BY_RCU.
 * Under ASan, freeing anything before a grace period is a reported
 * use-after-free, which the reader/updater stress tests rely on.
 */

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linuxkpi/subsys.h>

#include "harness.h"

TEST(read_lock_nesting)
{
	CHECK(!rcu_read_lock_held());
	rcu_read_lock();
	CHECK(rcu_read_lock_held());
	rcu_read_lock();
	rcu_read_lock_bh();
	rcu_read_unlock_bh();
	rcu_read_unlock();
	CHECK(rcu_read_lock_held());
	rcu_read_unlock();
	CHECK(!rcu_read_lock_held());
	synchronize_rcu();
	synchronize_rcu_expedited();
}

TEST(synchronize_inside_reader_warns)
{
	unsigned int before = linuxkpi_warn_count;
	void (*saved)(int, const char *) = lkpi_printk_hook;

	lkpi_printk_hook = NULL;
	rcu_read_lock();
	linuxkpi_loglevel = 0;		/* silence the expected warning */
	synchronize_rcu();		/* must not deadlock */
	linuxkpi_loglevel = 8;
	rcu_read_unlock();
	CHECK_EQ(linuxkpi_warn_count, before + 1);
	lkpi_printk_hook = saved;
}

/* synchronize_rcu() must wait for a reader that started before it. */
static struct completion reader_in;
static volatile int reader_done;

static void
slow_reader(void *arg)
{
	(void) arg;
	rcu_read_lock();
	complete(&reader_in);
	lkpi_plat_sleep_ticks((long)msecs_to_jiffies(50));
	reader_done = 1;
	rcu_read_unlock();
}

static void
sync_waiter(void *arg)
{
	static atomic_t turn;

	if (atomic_fetch_inc(&turn) % 2 == 0) {
		slow_reader(arg);
		return;
	}
	wait_for_completion(&reader_in);
	synchronize_rcu();
	CHECK_EQ(reader_done, 1);
}

TEST(synchronize_waits_for_reader)
{
	init_completion(&reader_in);
	reader_done = 0;
	harness_run_threads(2, sync_waiter, NULL);
	CHECK_EQ(reader_done, 1);
	lkpi_completion_destroy(&reader_in);
}

TEST(poll_state)
{
	unsigned long cookie;

	cookie = get_state_synchronize_rcu();
	CHECK(poll_state_synchronize_rcu(cookie));	/* no readers */
	rcu_read_lock();
	cookie = start_poll_synchronize_rcu();
	rcu_read_unlock();
	CHECK(poll_state_synchronize_rcu(cookie));
	cond_synchronize_rcu(cookie);
}

/* Holding a read section, a new cookie is not complete for others. */
static struct completion hold_in, hold_release;

static void
holding_reader(void *arg)
{
	(void) arg;
	rcu_read_lock();
	complete(&hold_in);
	wait_for_completion(&hold_release);
	rcu_read_unlock();
}

static void
poll_checker(void *arg)
{
	static atomic_t turn;
	unsigned long cookie;

	if (atomic_fetch_inc(&turn) % 2 == 0) {
		holding_reader(arg);
		return;
	}
	wait_for_completion(&hold_in);
	cookie = get_state_synchronize_rcu();
	CHECK(!poll_state_synchronize_rcu(cookie));
	complete(&hold_release);
	cond_synchronize_rcu(cookie);
	CHECK(poll_state_synchronize_rcu(cookie));
}

TEST(poll_state_with_reader)
{
	init_completion(&hold_in);
	init_completion(&hold_release);
	harness_run_threads(2, poll_checker, NULL);
	lkpi_completion_destroy(&hold_in);
	lkpi_completion_destroy(&hold_release);
}

struct cbobj {
	struct rcu_head	rh;
	int		v;
};

static atomic_t cb_runs;

static void
cb_func(struct rcu_head *rh)
{
	struct cbobj *o = container_of(rh, struct cbobj, rh);

	CHECK_EQ(o->v, 7);
	atomic_inc(&cb_runs);
	kfree(o);
}

TEST(call_rcu_and_barrier)
{
	long live0 = atomic_long_read(&lkpi_kmalloc_live);
	struct cbobj *o;
	int i;

	atomic_set(&cb_runs, 0);
	for (i = 0; i < 100; i++) {
		o = kmalloc(sizeof (*o), GFP_KERNEL);
		o->v = 7;
		call_rcu(&o->rh, cb_func);
	}
	rcu_barrier();
	CHECK_EQ(atomic_read(&cb_runs), 100);

	for (i = 0; i < 50; i++) {
		o = kmalloc(sizeof (*o), GFP_KERNEL);
		kfree_rcu(o, rh);
	}
	o = NULL;
	kfree_rcu(o, rh);		/* NULL is ignored */
	o = kmalloc(sizeof (*o), GFP_KERNEL);
	kfree_rcu_mightsleep(o);
	o = kvmalloc(sizeof (*o), GFP_KERNEL);
	kvfree_rcu(o, rh);
	rcu_barrier();
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

struct rnode {
	struct list_head	node;
	struct hlist_node	hnode;
	struct rcu_head		rh;
	int			key;
	unsigned int		magic;
};

#define	RMAGIC		0x52435521U
#define	NKEYS		16
#define	NREADERS	4

static LIST_HEAD(rlist);
static HLIST_HEAD(rhash);
static DEFINE_SPINLOCK(rlock);
static atomic_t r_stop, r_errors, r_turn;
static atomic_long_t r_lookups;

static void
rnode_free(struct rcu_head *rh)
{
	struct rnode *n = container_of(rh, struct rnode, rh);

	n->magic = 0xdeadU;	/* would be seen by a too-early reader */
	kfree(n);
}

static void
rcu_list_thread(void *arg)
{
	int me = atomic_fetch_inc(&r_turn);
	struct rnode *n, *old, *pos;
	int i, keys;

	(void) arg;
	if (me == 0) {
		/* Updater: replace random entries, free them after a GP. */
		for (i = 0; i < 3000; i++) {
			n = kmalloc(sizeof (*n), GFP_KERNEL);
			n->key = i % NKEYS;
			n->magic = RMAGIC;
			spin_lock(&rlock);
			old = NULL;
			list_for_each_entry(pos, &rlist, node) {
				if (pos->key == n->key) {
					old = pos;
					break;
				}
			}
			list_replace_rcu(&old->node, &n->node);
			hlist_replace_rcu(&old->hnode, &n->hnode);
			spin_unlock(&rlock);
			if (i % 2)
				call_rcu(&old->rh, rnode_free);
			else {
				synchronize_rcu();
				old->magic = 0xdeadU;
				kfree(old);
			}
		}
		atomic_set(&r_stop, 1);
		return;
	}
	/*
	 * do/while: each reader looks up at least once, even if the updater
	 * finished before it was scheduled (heavily loaded hosts).
	 */
	do {
		rcu_read_lock();
		keys = 0;
		list_for_each_entry_rcu(pos, &rlist, node) {
			if (READ_ONCE(pos->magic) != RMAGIC)
				atomic_inc(&r_errors);
			keys++;
		}
		if (keys != NKEYS)
			atomic_inc(&r_errors);
		hlist_for_each_entry_rcu(pos, &rhash, hnode) {
			if (READ_ONCE(pos->magic) != RMAGIC)
				atomic_inc(&r_errors);
		}
		rcu_read_unlock();
		atomic_long_inc(&r_lookups);
	} while (!atomic_read(&r_stop));
}

static void
rcu_list_run(bool force_overflow)
{
	struct rnode *n, *tmp;
	int i;

	INIT_LIST_HEAD(&rlist);
	INIT_HLIST_HEAD(&rhash);
	for (i = 0; i < NKEYS; i++) {
		n = kmalloc(sizeof (*n), GFP_KERNEL);
		n->key = i;
		n->magic = RMAGIC;
		list_add_tail_rcu(&n->node, &rlist);
		hlist_add_head_rcu(&n->hnode, &rhash);
	}
	atomic_set(&r_stop, 0);
	atomic_set(&r_errors, 0);
	atomic_set(&r_turn, 0);
	atomic_long_set(&r_lookups, 0);
	lkpi_rcu_test_force_overflow = force_overflow;
	harness_run_threads(1 + NREADERS, rcu_list_thread, NULL);
	lkpi_rcu_test_force_overflow = 0;
	CHECK_EQ(atomic_read(&r_errors), 0);
	CHECK(atomic_long_read(&r_lookups) > 0);
	rcu_barrier();
	list_for_each_entry_safe(n, tmp, &rlist, node) {
		list_del_rcu(&n->node);
		hlist_del_init_rcu(&n->hnode);
		kfree_rcu(n, rh);
	}
	CHECK(list_empty(&rlist));
	CHECK(hlist_empty(&rhash));
	rcu_barrier();
}

TEST(rcu_list_stress)
{
	rcu_list_run(false);
}

TEST(rcu_list_stress_overflow_slots)
{
	rcu_list_run(true);
}

TEST(rculist_api)
{
	LIST_HEAD(head);
	HLIST_HEAD(hh);
	struct rnode a = { .key = 1 }, b = { .key = 2 }, c = { .key = 3 };
	struct rnode d = { .key = 4 }, *pos;
	int sum = 0;

	CHECK(list_first_or_null_rcu(&head, struct rnode, node) == NULL);
	list_add_rcu(&b.node, &head);
	list_add_rcu(&a.node, &head);
	list_add_tail_rcu(&c.node, &head);
	rcu_read_lock();
	CHECK(list_first_or_null_rcu(&head, struct rnode, node) == &a);
	CHECK(list_next_or_null_rcu(&head, &a.node, struct rnode, node) == &b);
	CHECK(list_next_or_null_rcu(&head, &c.node, struct rnode, node) == NULL);
	list_for_each_entry_rcu(pos, &head, node, true)
		sum = sum * 10 + pos->key;
	rcu_read_unlock();
	CHECK_EQ(sum, 123);
	list_replace_rcu(&b.node, &d.node);
	list_del_rcu(&a.node);
	sum = 0;
	list_for_each_entry_rcu(pos, &head, node)
		sum = sum * 10 + pos->key;
	CHECK_EQ(sum, 43);
	pos = &d;
	list_for_each_entry_continue_rcu(pos, &head, node)
		sum += 100;
	CHECK_EQ(sum, 143);

	hlist_add_head_rcu(&a.hnode, &hh);
	hlist_add_behind_rcu(&c.hnode, &a.hnode);
	hlist_add_before_rcu(&b.hnode, &c.hnode);
	hlist_add_tail_rcu(&d.hnode, &hh);
	sum = 0;
	hlist_for_each_entry_rcu(pos, &hh, hnode)
		sum = sum * 10 + pos->key;
	CHECK_EQ(sum, 1234);
	hlist_del_rcu(&b.hnode);
	hlist_del_init_rcu(&d.hnode);
	CHECK(hlist_unhashed(&d.hnode));
	sum = 0;
	hlist_for_each_entry_rcu(pos, &hh, hnode)
		sum = sum * 10 + pos->key;
	CHECK_EQ(sum, 13);
}

TEST(pointer_publication)
{
	struct rnode a = { .key = 1 }, b = { .key = 2 };
	struct rnode *gp = NULL, *p;

	RCU_INIT_POINTER(gp, &a);
	CHECK(rcu_access_pointer(gp) == &a);
	rcu_read_lock();
	p = rcu_dereference(gp);
	CHECK(p->key == 1);
	rcu_read_unlock();
	rcu_assign_pointer(gp, &b);
	p = rcu_dereference_protected(gp, 1);
	CHECK(p == &b);
	p = rcu_replace_pointer(gp, &a, true);
	CHECK(p == &b && gp == &a);
	rcu_assign_pointer(gp, NULL);
	CHECK(rcu_dereference_check(gp, 1) == NULL);
}

/* SLAB_TYPESAFE_BY_RCU: freed objects stay readable until a GP. */
struct tsobj {
	int	v;
};

static struct kmem_cache *ts_cache;
static struct tsobj *volatile ts_shared;
static struct completion ts_in, ts_freed;
static atomic_t ts_turn;

static void
ts_thread(void *arg)
{
	struct tsobj *o;

	(void) arg;
	if (atomic_fetch_inc(&ts_turn) % 2 == 0) {
		rcu_read_lock();
		o = ts_shared;
		complete(&ts_in);
		wait_for_completion(&ts_freed);
		/* Still mapped memory: ASan would flag an early free. */
		CHECK_EQ(READ_ONCE(o->v), 99);
		rcu_read_unlock();
		return;
	}
	wait_for_completion(&ts_in);
	kmem_cache_free(ts_cache, ts_shared);
	complete(&ts_freed);
}

TEST(slab_typesafe_by_rcu)
{
	ts_cache = kmem_cache_create("lkpi_test_ts", sizeof (struct tsobj), 0,
	    SLAB_TYPESAFE_BY_RCU, NULL);
	CHECK(ts_cache != NULL);
	CHECK_EQ(kmem_cache_size(ts_cache), sizeof (struct tsobj));
	ts_shared = kmem_cache_alloc(ts_cache, GFP_KERNEL);
	ts_shared->v = 99;
	init_completion(&ts_in);
	init_completion(&ts_freed);
	atomic_set(&ts_turn, 0);
	harness_run_threads(2, ts_thread, NULL);
	kmem_cache_destroy(ts_cache);	/* waits for the deferred free */
	lkpi_completion_destroy(&ts_in);
	lkpi_completion_destroy(&ts_freed);
}

TEST(fini_and_restart)
{
	struct cbobj *o;

	lkpi_rcu_fini();
	CHECK_EQ(lkpi_rcu_init(), 0);
	o = kmalloc(sizeof (*o), GFP_KERNEL);
	o->v = 7;
	atomic_set(&cb_runs, 0);
	call_rcu(&o->rh, cb_func);
	rcu_barrier();
	CHECK_EQ(atomic_read(&cb_runs), 1);
#ifndef LKPI_KTEST
	lkpi_rcu_fini();	/* the kernel module owns the thread */
#endif
}

LKPI_SUITE(rcu)
{
	RUN(read_lock_nesting);
	RUN(synchronize_inside_reader_warns);
	RUN(synchronize_waits_for_reader);
	RUN(poll_state);
	RUN(poll_state_with_reader);
	RUN(call_rcu_and_barrier);
	RUN(rcu_list_stress);
	RUN(rcu_list_stress_overflow_slots);
	RUN(rculist_api);
	RUN(pointer_publication);
	RUN(slab_typesafe_by_rcu);
	RUN(fini_and_restart);
}
