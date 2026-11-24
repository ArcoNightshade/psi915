/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * memory-barrier API (Documentation/memory-barriers.txt semantics);
 * cross-checked against FreeBSD's LinuxKPI asm/barrier.h (BSD-2-Clause).
 *
 * amd64 is TSO: loads are not reordered with loads, stores not with stores,
 * so smp_rmb()/smp_wmb() only need to stop the compiler. smp_mb() needs a
 * real fence (store->load ordering). __atomic_thread_fence(SEQ_CST) emits
 * "lock orq $0,(%rsp)" with -mno-sse (kernel) and mfence otherwise; both are
 * full barriers. The mandatory barriers mb/rmb/wmb, used to order accesses
 * to device memory (WC mappings), use the real fence instructions.
 */
#ifndef _LINUXKPI_ASM_BARRIER_H_
#define	_LINUXKPI_ASM_BARRIER_H_

#include <linux/compiler.h>

#if defined(__x86_64__) || defined(__amd64)

#define	mb()		__asm__ __volatile__("mfence" : : : "memory")
#define	rmb()		__asm__ __volatile__("lfence" : : : "memory")
#define	wmb()		__asm__ __volatile__("sfence" : : : "memory")
#define	smp_mb()	__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define	smp_rmb()	barrier()
#define	smp_wmb()	barrier()
/* Locked RMW instructions are already full barriers on x86. */
#define	smp_mb__before_atomic()	barrier()
#define	smp_mb__after_atomic()	barrier()

#else	/* generic, used only for userland tests on other hosts */

#define	mb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define	rmb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define	wmb()		__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define	smp_mb()	__atomic_thread_fence(__ATOMIC_SEQ_CST)
#define	smp_rmb()	__atomic_thread_fence(__ATOMIC_ACQUIRE)
#define	smp_wmb()	__atomic_thread_fence(__ATOMIC_RELEASE)
#define	smp_mb__before_atomic()	smp_mb()
#define	smp_mb__after_atomic()	smp_mb()

#endif

/* Coherent DMA memory is ordered like normal memory on x86. */
#define	dma_rmb()	smp_rmb()
#define	dma_wmb()	smp_wmb()
#define	__smp_mb()	smp_mb()
#define	virt_mb()	smp_mb()
#define	smp_read_barrier_depends()	do { } while (0)
#define	read_barrier_depends()		do { } while (0)

/* Upgrade a control dependency to acquire ordering. */
#define	smp_acquire__after_ctrl_dep()	smp_rmb()

#define	smp_store_mb(var, value) do {					\
	WRITE_ONCE(var, value);						\
	smp_mb();							\
} while (0)

#define	smp_load_acquire(p)	__atomic_load_n((p), __ATOMIC_ACQUIRE)
#define	smp_store_release(p, v)						\
	__atomic_store_n((p), (v), __ATOMIC_RELEASE)

#endif /* _LINUXKPI_ASM_BARRIER_H_ */
