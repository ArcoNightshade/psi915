/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * CPU identification and cache maintenance (see <asm/cpufeature.h>,
 * <asm/cacheflush.h>). Clean-room, from the Intel SDM's CPUID and CLFLUSH
 * descriptions.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/cpufeature.h>
#include <asm/cacheflush.h>
#include <asm/cpu_device_id.h>
#include <asm/fpu/api.h>
#include <linuxkpi/plat_dev.h>

static struct cpuinfo_x86 lkpi_cpu;
static int lkpi_cpu_ready;

static void
lkpi_cpuid(u32 leaf, u32 sub, u32 *a, u32 *b, u32 *c, u32 *d)
{
	__asm__ __volatile__("cpuid" : "=a" (*a), "=b" (*b), "=c" (*c),
	    "=d" (*d) : "0" (leaf), "2" (sub));
}

static void
lkpi_cpu_identify(struct cpuinfo_x86 *ci)
{
	u32 a, b, c, d, max, family, model;
	unsigned int i;

	memset(ci, 0, sizeof (*ci));
	lkpi_cpuid(0, 0, &max, &b, &c, &d);
	memcpy(ci->x86_vendor_id, &b, 4);
	memcpy(ci->x86_vendor_id + 4, &d, 4);
	memcpy(ci->x86_vendor_id + 8, &c, 4);
	if (strcmp(ci->x86_vendor_id, "GenuineIntel") == 0)
		ci->x86_vendor = X86_VENDOR_INTEL;
	else if (strcmp(ci->x86_vendor_id, "AuthenticAMD") == 0)
		ci->x86_vendor = X86_VENDOR_AMD;
	else
		ci->x86_vendor = X86_VENDOR_UNKNOWN;
	if (max >= 1) {
		lkpi_cpuid(1, 0, &a, &b, &c, &d);
		family = (a >> 8) & 0xf;
		model = (a >> 4) & 0xf;
		if (family == 0xf)
			family += (a >> 20) & 0xff;
		if (family == 0x6 || family >= 0xf)
			model |= ((a >> 16) & 0xf) << 4;
		ci->x86 = (u8)family;
		ci->x86_model = (u8)model;
		ci->x86_stepping = (u8)(a & 0xf);
		ci->x86_clflush_size = (u16)(((b >> 8) & 0xff) * 8);
		ci->x86_capability[0] = d;
		ci->x86_capability[4] = c;
#ifdef LINUXKPI_ILLUMOS_KERNEL
		/*
		 * Kernel SSE use (kernel_fpu_begin) is not supported yet, so
		 * SSE4.1 is hidden: i915 and DRM then never take their
		 * MOVNTDQA copy paths (i915_memcpy_from_wc() fails and the
		 * callers fall back to plain copies).
		 */
		ci->x86_capability[4] &= ~(1U << (X86_FEATURE_XMM4_1 % 32));
#endif
	}
	if (max >= 7) {
		lkpi_cpuid(7, 0, &a, &b, &c, &d);
		ci->x86_capability[9] = b;
	}
	if (ci->x86_clflush_size == 0)
		ci->x86_clflush_size = 64;
	ci->x86_cache_alignment = ci->x86_clflush_size;
	lkpi_cpuid(0x80000000, 0, &max, &b, &c, &d);
	if (max >= 0x80000004) {
		for (i = 0; i < 3; i++) {
			lkpi_cpuid(0x80000002 + i, 0, &a, &b, &c, &d);
			memcpy(ci->x86_model_id + i * 16, &a, 4);
			memcpy(ci->x86_model_id + i * 16 + 4, &b, 4);
			memcpy(ci->x86_model_id + i * 16 + 8, &c, 4);
			memcpy(ci->x86_model_id + i * 16 + 12, &d, 4);
		}
	}
	if (max >= 0x80000008) {
		lkpi_cpuid(0x80000008, 0, &a, &b, &c, &d);
		ci->x86_phys_bits = (u8)(a & 0xff);
		ci->x86_virt_bits = (u8)((a >> 8) & 0xff);
	} else {
		ci->x86_phys_bits = 36;
		ci->x86_virt_bits = 48;
	}
}

/* Idempotent and race-free in effect: every caller computes the same. */
const struct cpuinfo_x86 *
lkpi_boot_cpu_data(void)
{
	if (!__atomic_load_n(&lkpi_cpu_ready, __ATOMIC_ACQUIRE)) {
		struct cpuinfo_x86 ci;

		lkpi_cpu_identify(&ci);
		lkpi_cpu = ci;
		__atomic_store_n(&lkpi_cpu_ready, 1, __ATOMIC_RELEASE);
	}
	return (&lkpi_cpu);
}

void
clflush_cache_range(void *addr, unsigned int size)
{
	unsigned long line = boot_cpu_data.x86_clflush_size;
	char *p = (char *)((uintptr_t)addr & ~(line - 1));
	char *end = (char *)addr + size;
	bool opt = boot_cpu_has(X86_FEATURE_CLFLUSHOPT);

	if (size == 0)
		return;
	__asm__ __volatile__("mfence" : : : "memory");
	for (; p < end; p += line) {
		if (opt)
			clflushopt(p);
		else
			clflush(p);
	}
	__asm__ __volatile__("mfence" : : : "memory");
}

int
wbinvd_on_all_cpus(void)
{
	lkpi_plat_wbinvd();
	return (0);
}

/*
 * Returns the first entry matching the boot CPU's vendor, family and
 * model (and feature bit, when given); entries with X86_*_ANY match
 * anything. The table ends with an all-zero entry (flags == 0).
 */
const struct x86_cpu_id *
x86_match_cpu(const struct x86_cpu_id *match)
{
	const struct cpuinfo_x86 *c = &boot_cpu_data;
	const struct x86_cpu_id *m;

	for (m = match; m->flags & X86_CPU_ID_FLAG_ENTRY_VALID; m++) {
		if (m->vendor != X86_VENDOR_ANY && m->vendor != c->x86_vendor)
			continue;
		if (m->family != X86_FAMILY_ANY && m->family != c->x86)
			continue;
		if (m->model != X86_MODEL_ANY && m->model != c->x86_model)
			continue;
		if (m->feature != X86_FEATURE_ANY && !boot_cpu_has(m->feature))
			continue;
		return (m);
	}
	return (NULL);
}

void
lkpi_kernel_fpu_begin(void)
{
#ifdef LINUXKPI_ILLUMOS_KERNEL
	lkpi_plat_panic("linuxkpi: kernel_fpu_begin() is not supported");
#endif
}

void
lkpi_kernel_fpu_end(void)
{
}
