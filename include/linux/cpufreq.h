/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No cpufreq: frequencies are unknown (0).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_CPUFREQ_H_
#define	_LINUXKPI_INCLUDE_LINUX_CPUFREQ_H_

/* No cpufreq driver: no policy is ever returned. */
struct cpufreq_cpuinfo {
	unsigned int	max_freq;
	unsigned int	min_freq;
	unsigned int	transition_latency;
};

struct cpufreq_policy {
	unsigned int		cpu;
	unsigned int		cur;
	unsigned int		min;
	unsigned int		max;
	struct cpufreq_cpuinfo	cpuinfo;
};

#define	cpufreq_cpu_get(cpu)	((void)(cpu), (struct cpufreq_policy *)0)
#define	cpufreq_cpu_put(p)	do { (void)(p); } while (0)
#define	cpufreq_quick_get_max(cpu)	(0U)
#define	cpufreq_get(cpu)		(0U)

#endif /* _LINUXKPI_INCLUDE_LINUX_CPUFREQ_H_ */
