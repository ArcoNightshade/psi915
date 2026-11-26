/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reproducer for docs/12 finding M2: user-sized kvmalloc() requests become
 * KM_SLEEP. i915_gem_execbuffer2_ioctl() allocates
 *   kvmalloc_array(count + 2, eb_element_size(), __GFP_NOWARN | GFP_KERNEL)
 * with count from userspace (renderD128 is mode 0666). On Linux an
 * unsatisfiable request fails (vmalloc returns NULL). Here it is accepted
 * up to LKPI_KVMALLOC_MAX (16 GiB) and mapped to KM_SLEEP, and kmem_alloc
 * with KM_SLEEP never fails: it waits for memory that never comes while
 * reaping every cache in the system.
 *
 * This program only evaluates the mapping (it does not allocate).
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <stdio.h>

int
main(void)
{
	gfp_t gfp = __GFP_NOWARN | GFP_KERNEL;
	size_t want = (size_t)8 << 30;	/* 8 GiB, e.g. 2^28 exec objects */
	int plat = lkpi_gfp_to_plat(gfp);

	printf("request %zu bytes, limit %lu, plat flag %d (SLEEP=%d)\n",
	    want, (unsigned long)LKPI_KVMALLOC_MAX, plat, LKPI_PLAT_SLEEP);
	if (want <= LKPI_KVMALLOC_MAX && plat == LKPI_PLAT_SLEEP) {
		printf("FAIL: unprivileged 8 GiB request would be a KM_SLEEP "
		    "kmem_alloc (never fails, blocks)\n");
		return (1);
	}
	printf("ok\n");
	return (0);
}
