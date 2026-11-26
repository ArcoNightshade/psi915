/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * uapi_dump: print the DRM/i915/sync_file/dma-buf uAPI as one header set sees
 * it (ioctl numbers, argument sizes, field offsets, ABI constants), so that
 * the kernel-side headers (what the LinuxKPI/i915 port decodes) can be
 * compared with the userland-side headers (what libdrm/Mesa send) by
 * uapi_cmp.awk. It also checks the illumos ioctl encoding contract that the
 * kernel ioctl glue must implement (see docs/09-userland-stack.md).
 *
 * Build sides (see Makefile):
 *   kernel:  -DUAPI_SIDE_KERNEL -D__KERNEL__ -Icompat/kpi -I<kernel uapi/drm>
 *            header values come out in the Linux layout on every host.
 *   user:    -DUAPI_SIDE_USER -I<libdrm include dir> [-I../compat/include]
 *            header values come out in the host's native layout
 *            (Linux on Linux, <sys/ioccom.h> on illumos).
 *
 * Output: one tab-separated record per line; '#' lines are comments.
 * Exit status: 0 ok, 1 if a self-check failed.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#if defined(UAPI_SIDE_KERNEL) == defined(UAPI_SIDE_USER)
#error "define exactly one of UAPI_SIDE_KERNEL or UAPI_SIDE_USER"
#endif

#include "i915_drm.h"		/* includes drm.h and drm_mode.h */
#ifdef UAPI_HAVE_SYNC_FILE
#include <linux/sync_file.h>
#endif
#ifdef UAPI_HAVE_DMA_BUF
#include <linux/dma-buf.h>
#endif

/* Direction as written in the header macro (Linux meaning). */
enum { D_NONE = 0, D_W = 1, D_R = 2, D_WR = 3 };

static int failures;
static int warnings;

static const char *
dirname_of(int d)
{
	switch (d) {
	case D_NONE: return ("none");
	case D_W: return ("w");
	case D_R: return ("r");
	default: return ("wr");
	}
}

/* Linux generic layout (x86): dir<<30 | size<<16 (14 bits) | type<<8 | nr */
static uint32_t
enc_linux(int dir, unsigned type, unsigned nr, size_t size)
{
	return (((uint32_t)dir << 30) | (((uint32_t)size & 0x3fffu) << 16) |
	    ((type & 0xffu) << 8) | (nr & 0xffu));
}

/*
 * illumos <sys/ioccom.h>: IOC_VOID 0x20000000, IOC_OUT 0x40000000 (kernel to
 * user, Linux "read"), IOC_IN 0x80000000 (user to kernel, Linux "write"),
 * size masked to IOCPARM_MASK (0xff).
 */
#define	IL_IOC_VOID	0x20000000u
#define	IL_IOC_OUT	0x40000000u
#define	IL_IOC_IN	0x80000000u
#define	IL_IOCPARM_MASK	0xffu

static uint32_t
enc_illumos(int dir, unsigned type, unsigned nr, size_t size)
{
	uint32_t v = ((type & 0xffu) << 8) | (nr & 0xffu);

	if (dir == D_NONE)
		return (v | IL_IOC_VOID);
	if (dir & D_W)
		v |= IL_IOC_IN;
	if (dir & D_R)
		v |= IL_IOC_OUT;
	return (v | (((uint32_t)size & IL_IOCPARM_MASK) << 16));
}

/*
 * Proposed decode contract for the illumos kernel ioctl glue, applied to a
 * userland (native illumos) command word:
 *   nr = cmd & 0xff, type = (cmd >> 8) & 0xff,
 *   copy-in  iff cmd & IOC_IN, copy-out iff cmd & IOC_OUT,
 *   user size = (cmd >> 16) & 0xff, and if the kernel's own argument size is
 *   >= 256 the user size is ambiguous, so the kernel uses its table size when
 *   (ksize & 0xff) == usize.
 * Returns 0 if the command decodes to the expected ioctl.
 */
static int
decode_illumos_ok(uint32_t cmd, int dir, unsigned type, unsigned nr,
    size_t ksize)
{
	int ddir = 0;

	if ((cmd & 0xffu) != (nr & 0xffu) || ((cmd >> 8) & 0xffu) != type)
		return (-1);
	if (cmd & IL_IOC_IN)
		ddir |= D_W;
	if (cmd & IL_IOC_OUT)
		ddir |= D_R;
	if (ddir != dir)
		return (-1);
	if (dir == D_NONE)
		return ((cmd & IL_IOC_VOID) ? 0 : -1);
	if (((cmd >> 16) & IL_IOCPARM_MASK) != (ksize & IL_IOCPARM_MASK))
		return (-1);
	return (0);
}

#if defined(UAPI_SIDE_KERNEL)
#define	SIDE		"kernel"
#define	NATIVE_ENC	"linux"
#define	native_enc	enc_linux
#elif defined(__linux__)
#define	SIDE		"user"
#define	NATIVE_ENC	"linux"
#define	native_enc	enc_linux
#elif defined(__sun)
#define	SIDE		"user"
#define	NATIVE_ENC	"illumos"
#define	native_enc	enc_illumos
#else
#error "user side: unsupported host (expected Linux or illumos)"
#endif

static void
ioctl_entry(const char *name, int dir, unsigned type, unsigned nr,
    size_t size, uint32_t hdr, const char *argtype)
{
	uint32_t lin = enc_linux(dir, type, nr, size);
	uint32_t ill = enc_illumos(dir, type, nr, size);
	uint32_t nat = native_enc(dir, type, nr, size);
	char flags[128] = "";

	if (size > IL_IOCPARM_MASK)
		strcat(flags, "TRUNC_ILLUMOS,");
	if (size > 0x3fff)
		strcat(flags, "TRUNC_LINUX,");
	if (hdr != nat) {
		strcat(flags, "HDR_MISMATCH,");
		failures++;
		printf("# FAIL %s: header value 0x%08x != computed %s "
		    "encoding 0x%08x\n", name, hdr, NATIVE_ENC, nat);
	}
	if (decode_illumos_ok(ill, dir, type, nr, size) != 0) {
		strcat(flags, "DECODE_FAIL,");
		failures++;
		printf("# FAIL %s: illumos decode contract does not round-trip\n",
		    name);
	}
	if (flags[0] == '\0')
		strcpy(flags, "-");
	else
		flags[strlen(flags) - 1] = '\0';
	printf("E\t%s\t%s\t0x%02x\t0x%02x\t%zu\t0x%08x\t0x%08x\t0x%08x\t%s\t%s\n",
	    name, dirname_of(dir), type & 0xffu, nr & 0xffu, size, hdr, lin,
	    ill, flags, argtype[0] ? argtype : "-");
}

#define	E(name, dir, type, nr, size, argtype)				\
	ioctl_entry(#name, dir, (unsigned)(type), (unsigned)(nr),	\
	    (size_t)(size), (uint32_t)(unsigned long)(name), argtype);
#define	A(name)		printf("A\t%s\n", #name);
#define	C(name)		printf("C\t%s\t0x%llx\n", #name,		\
			    (unsigned long long)(name));
#define	CA(name)	printf("CA\t%s\n", #name);

/* Struct layout records. */
#define	O(s, f)		printf("O\t%s.%s\t%zu\n", #s, #f, offsetof(s, f));
#define	S(s)		printf("S\t%s\t%zu\n", #s, sizeof (s));

static void
dump_layouts(void)
{
	/* Core: legacy structs with pointers/longs (ILP32 compat hazards). */
	S(struct drm_version)
	O(struct drm_version, version_major)
	O(struct drm_version, name_len)
	O(struct drm_version, name)
	O(struct drm_version, date_len)
	O(struct drm_version, date)
	O(struct drm_version, desc_len)
	O(struct drm_version, desc)
	S(struct drm_unique)
	O(struct drm_unique, unique_len)
	O(struct drm_unique, unique)
	S(struct drm_set_version)
	S(union drm_wait_vblank)
	O(union drm_wait_vblank, request.type)
	O(union drm_wait_vblank, request.sequence)
	O(union drm_wait_vblank, request.signal)
	O(union drm_wait_vblank, reply.tval_sec)
	O(union drm_wait_vblank, reply.tval_usec)

	/* Core GEM / caps / PRIME. */
	S(struct drm_gem_close)
	S(struct drm_gem_flink)
	S(struct drm_gem_open)
	O(struct drm_gem_open, size)
	S(struct drm_get_cap)
	O(struct drm_get_cap, value)
	S(struct drm_set_client_cap)
	S(struct drm_prime_handle)
	O(struct drm_prime_handle, flags)
	O(struct drm_prime_handle, fd)

#ifdef DRM_IOCTL_SYNCOBJ_CREATE
	S(struct drm_syncobj_create)
	S(struct drm_syncobj_destroy)
	S(struct drm_syncobj_handle)
	O(struct drm_syncobj_handle, flags)
	O(struct drm_syncobj_handle, fd)
	S(struct drm_syncobj_wait)
	O(struct drm_syncobj_wait, handles)
	O(struct drm_syncobj_wait, timeout_nsec)
	O(struct drm_syncobj_wait, count_handles)
	O(struct drm_syncobj_wait, flags)
	O(struct drm_syncobj_wait, first_signaled)
	S(struct drm_syncobj_array)
#endif
#ifdef DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT
	S(struct drm_syncobj_timeline_wait)
	O(struct drm_syncobj_timeline_wait, handles)
	O(struct drm_syncobj_timeline_wait, points)
	O(struct drm_syncobj_timeline_wait, timeout_nsec)
	O(struct drm_syncobj_timeline_wait, count_handles)
	O(struct drm_syncobj_timeline_wait, flags)
	O(struct drm_syncobj_timeline_wait, first_signaled)
	S(struct drm_syncobj_timeline_array)
	S(struct drm_syncobj_transfer)
	O(struct drm_syncobj_transfer, src_point)
	O(struct drm_syncobj_transfer, dst_point)
	O(struct drm_syncobj_transfer, flags)
#endif

	/* Events delivered by read(2) on the DRM fd. */
	S(struct drm_event)
	S(struct drm_event_vblank)
	O(struct drm_event_vblank, user_data)
	O(struct drm_event_vblank, tv_sec)
	O(struct drm_event_vblank, tv_usec)
	O(struct drm_event_vblank, sequence)
	O(struct drm_event_vblank, crtc_id)
#ifdef DRM_EVENT_CRTC_SEQUENCE
	S(struct drm_event_crtc_sequence)
	O(struct drm_event_crtc_sequence, user_data)
	O(struct drm_event_crtc_sequence, time_ns)
	O(struct drm_event_crtc_sequence, sequence)
#endif

	/* KMS used by libdrm/modesetting/kmscube. */
	S(struct drm_mode_modeinfo)
	S(struct drm_mode_crtc)
	O(struct drm_mode_crtc, set_connectors_ptr)
	O(struct drm_mode_crtc, mode)
	S(struct drm_mode_card_res)
	S(struct drm_mode_get_connector)
	S(struct drm_mode_get_encoder)
	S(struct drm_mode_get_plane)
	S(struct drm_mode_get_property)
	S(struct drm_mode_obj_get_properties)
	S(struct drm_mode_fb_cmd)
	S(struct drm_mode_fb_cmd2)
	O(struct drm_mode_fb_cmd2, pixel_format)
	O(struct drm_mode_fb_cmd2, flags)
	O(struct drm_mode_fb_cmd2, handles)
	O(struct drm_mode_fb_cmd2, pitches)
	O(struct drm_mode_fb_cmd2, offsets)
	O(struct drm_mode_fb_cmd2, modifier)
	S(struct drm_mode_crtc_page_flip)
	O(struct drm_mode_crtc_page_flip, user_data)
	S(struct drm_mode_atomic)
	O(struct drm_mode_atomic, objs_ptr)
	O(struct drm_mode_atomic, count_props_ptr)
	O(struct drm_mode_atomic, props_ptr)
	O(struct drm_mode_atomic, prop_values_ptr)
	O(struct drm_mode_atomic, user_data)
	S(struct drm_mode_create_dumb)
	O(struct drm_mode_create_dumb, handle)
	O(struct drm_mode_create_dumb, pitch)
	O(struct drm_mode_create_dumb, size)
	S(struct drm_mode_map_dumb)
	O(struct drm_mode_map_dumb, offset)
	S(struct drm_mode_cursor2)

	/* i915 (Gen9 paths used by iris/anv/libdrm_intel/igt). */
	S(struct drm_i915_getparam)
	O(struct drm_i915_getparam, value)
	S(struct drm_i915_gem_execbuffer2)
	O(struct drm_i915_gem_execbuffer2, buffers_ptr)
	O(struct drm_i915_gem_execbuffer2, buffer_count)
	O(struct drm_i915_gem_execbuffer2, batch_start_offset)
	O(struct drm_i915_gem_execbuffer2, batch_len)
	O(struct drm_i915_gem_execbuffer2, num_cliprects)
	O(struct drm_i915_gem_execbuffer2, cliprects_ptr)
	O(struct drm_i915_gem_execbuffer2, flags)
	O(struct drm_i915_gem_execbuffer2, rsvd1)
	O(struct drm_i915_gem_execbuffer2, rsvd2)
	S(struct drm_i915_gem_exec_object2)
	O(struct drm_i915_gem_exec_object2, relocs_ptr)
	O(struct drm_i915_gem_exec_object2, alignment)
	O(struct drm_i915_gem_exec_object2, offset)
	O(struct drm_i915_gem_exec_object2, flags)
	S(struct drm_i915_gem_relocation_entry)
	S(struct drm_i915_gem_exec_fence)
	S(struct drm_i915_gem_create)
	S(struct drm_i915_gem_mmap)
	O(struct drm_i915_gem_mmap, addr_ptr)
	O(struct drm_i915_gem_mmap, flags)
	S(struct drm_i915_gem_mmap_gtt)
	S(struct drm_i915_gem_set_domain)
	S(struct drm_i915_gem_set_tiling)
	S(struct drm_i915_gem_get_tiling)
	S(struct drm_i915_gem_caching)
	S(struct drm_i915_gem_busy)
	S(struct drm_i915_gem_wait)
	O(struct drm_i915_gem_wait, timeout_ns)
	S(struct drm_i915_gem_madvise)
	S(struct drm_i915_gem_userptr)
	S(struct drm_i915_gem_context_create)
	S(struct drm_i915_gem_context_destroy)
	S(struct drm_i915_gem_context_param)
	O(struct drm_i915_gem_context_param, param)
	O(struct drm_i915_gem_context_param, value)
	S(struct drm_i915_reg_read)
	S(struct drm_i915_reset_stats)
#ifdef DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT
	S(struct drm_i915_gem_context_create_ext)
	O(struct drm_i915_gem_context_create_ext, extensions)
	S(struct i915_user_extension)
	S(struct drm_i915_gem_context_create_ext_setparam)
#endif
#ifdef DRM_IOCTL_I915_QUERY
	S(struct drm_i915_query)
	O(struct drm_i915_query, items_ptr)
	S(struct drm_i915_query_item)
	O(struct drm_i915_query_item, data_ptr)
	S(struct drm_i915_query_topology_info)
#endif
#ifdef DRM_I915_QUERY_ENGINE_INFO
	S(struct drm_i915_engine_info)
	S(struct drm_i915_query_engine_info)
#endif
#ifdef DRM_IOCTL_I915_GEM_VM_CREATE
	S(struct drm_i915_gem_vm_control)
#endif
#ifdef DRM_IOCTL_I915_GEM_MMAP_OFFSET
	S(struct drm_i915_gem_mmap_offset)
	O(struct drm_i915_gem_mmap_offset, offset)
	O(struct drm_i915_gem_mmap_offset, flags)
	O(struct drm_i915_gem_mmap_offset, extensions)
#endif
#ifdef DRM_IOCTL_I915_GEM_CREATE_EXT
	S(struct drm_i915_gem_create_ext)
	O(struct drm_i915_gem_create_ext, flags)
	O(struct drm_i915_gem_create_ext, extensions)
#endif
#ifdef I915_EXEC_USE_EXTENSIONS
	S(struct drm_i915_gem_execbuffer_ext_timeline_fences)
	O(struct drm_i915_gem_execbuffer_ext_timeline_fences, fence_count)
	O(struct drm_i915_gem_execbuffer_ext_timeline_fences, handles_ptr)
	O(struct drm_i915_gem_execbuffer_ext_timeline_fences, values_ptr)
#endif

#ifdef UAPI_HAVE_SYNC_FILE
	S(struct sync_merge_data)
	O(struct sync_merge_data, fd2)
	O(struct sync_merge_data, fence)
	O(struct sync_merge_data, flags)
	S(struct sync_fence_info)
	S(struct sync_file_info)
	O(struct sync_file_info, num_fences)
	O(struct sync_file_info, sync_fence_info)
#endif
#ifdef UAPI_HAVE_DMA_BUF
	S(struct dma_buf_sync)
#ifdef DMA_BUF_IOCTL_EXPORT_SYNC_FILE
	S(struct dma_buf_export_sync_file)
	O(struct dma_buf_export_sync_file, fd)
#endif
#endif
}

int
main(void)
{
	printf("# uapi_dump: DRM/i915 uAPI as seen by one header set\n");
	printf("H\tside\t%s\n", SIDE);
	printf("H\tenc\t%s\n", NATIVE_ENC);
	printf("H\tabi\t%s\n", sizeof (long) == 8 ? "lp64" : "ilp32");
	printf("H\tsync_file\t%s\n",
#ifdef UAPI_HAVE_SYNC_FILE
	    "yes"
#else
	    "no"
#endif
	    );
	printf("H\tdma_buf\t%s\n",
#ifdef UAPI_HAVE_DMA_BUF
	    "yes"
#else
	    "no"
#endif
	    );
	/*
	 * open(2) flags that travel inside DRM ioctls (DRM_CLOEXEC/DRM_RDWR in
	 * PRIME and syncobj handle-to-fd). Kernel side: the Linux x86 values the
	 * imported driver code was written against; user side: host values.
	 */
#ifdef UAPI_SIDE_KERNEL
	printf("H\tO_CLOEXEC\t0x%x\n", 0x80000u);	/* Linux 02000000 */
	printf("H\tO_RDWR\t0x%x\n", 0x2u);
	printf("H\tO_NONBLOCK\t0x%x\n", 0x800u);	/* Linux 04000 */
#else
	printf("H\tO_CLOEXEC\t0x%x\n", (unsigned)O_CLOEXEC);
	printf("H\tO_RDWR\t0x%x\n", (unsigned)O_RDWR);
	printf("H\tO_NONBLOCK\t0x%x\n", (unsigned)O_NONBLOCK);
#endif

#define	UAPI_WANT_IOCTLS
#define	UAPI_WANT_CONSTS
#include "uapi_list.h"

	dump_layouts();

	printf("# summary: %d failure(s), %d warning(s)\n", failures, warnings);
	return (failures ? 1 : 0);
}
