/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. CPU identity and cross-calls. smp_processor_id() is the
 * sequential id of the CPU the caller ran on when it asked (a hint: the
 * thread may migrate at any time, as with raw_smp_processor_id()).
 * on_each_cpu() runs the function once on the calling CPU only; KPI
 * consumers use it for cache maintenance, where the x86 instructions used
 * (wbinvd excepted, see <asm/cacheflush.h>) are coherent across CPUs.
 */
#ifndef _LINUXKPI_LINUX_SMP_H_
#define	_LINUXKPI_LINUX_SMP_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>

#define	smp_processor_id()		((int)lkpi_plat_cpu_id())
#define	raw_smp_processor_id()		((int)lkpi_plat_cpu_id())
#define	get_cpu()			((int)lkpi_plat_cpu_id())
#define	put_cpu()			do { } while (0)
#define	num_online_cpus()		((unsigned int)lkpi_plat_ncpus())
#define	num_possible_cpus()		((unsigned int)lkpi_plat_ncpus())
#define	num_present_cpus()		((unsigned int)lkpi_plat_ncpus())
#define	nr_cpu_ids			((unsigned int)lkpi_plat_ncpus())

typedef void (*smp_call_func_t)(void *info);

static inline int
on_each_cpu(smp_call_func_t func, void *info, int wait)
{
	(void) wait;
	func(info);
	return (0);
}

#endif /* _LINUXKPI_LINUX_SMP_H_ */
