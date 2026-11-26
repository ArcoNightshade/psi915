/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * i915 GEM, request and context trace points.
 * psi915 builds with CONFIG_TRACEPOINTS off, so every trace point is a no-op.  This header replaces the
 * GPL-2.0 upstream trace header.  It lists the trace points the MIT code calls, by name, taken from the
 * call sites, and is not derived from the upstream header.  The arguments go to an empty variadic inline
 * function, so they are evaluated as with upstream's disabled trace points and no unused-variable
 * warnings appear.
 * Generated list; regenerate when the import is re-pinned.
 */

#ifndef _PSI915_I915_TRACE_H_
#define _PSI915_I915_TRACE_H_

#ifndef PSI915_TRACE_NOOP
static inline void psi915_trace_noop(int unused, ...) { (void)unused; }
#define PSI915_TRACE_NOOP(...)	psi915_trace_noop(0, ##__VA_ARGS__)
#endif

#ifndef trace_i915_context_create
#define trace_i915_context_create(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_context_free
#define trace_i915_context_free(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_evict
#define trace_i915_gem_evict(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_evict_node
#define trace_i915_gem_evict_node(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_evict_vm
#define trace_i915_gem_evict_vm(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_clflush
#define trace_i915_gem_object_clflush(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_create
#define trace_i915_gem_object_create(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_destroy
#define trace_i915_gem_object_destroy(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_fault
#define trace_i915_gem_object_fault(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_pread
#define trace_i915_gem_object_pread(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_object_pwrite
#define trace_i915_gem_object_pwrite(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_gem_shrink
#define trace_i915_gem_shrink(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_ppgtt_create
#define trace_i915_ppgtt_create(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_ppgtt_release
#define trace_i915_ppgtt_release(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_reg_rw
#define trace_i915_reg_rw(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_add
#define trace_i915_request_add(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_execute
#define trace_i915_request_execute(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_guc_submit
#define trace_i915_request_guc_submit(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_in
#define trace_i915_request_in(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_out
#define trace_i915_request_out(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_queue
#define trace_i915_request_queue(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_retire
#define trace_i915_request_retire(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_submit
#define trace_i915_request_submit(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_wait_begin
#define trace_i915_request_wait_begin(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_request_wait_end
#define trace_i915_request_wait_end(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_vma_bind
#define trace_i915_vma_bind(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_i915_vma_unbind
#define trace_i915_vma_unbind(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_ban
#define trace_intel_context_ban(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_create
#define trace_intel_context_create(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_deregister
#define trace_intel_context_deregister(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_deregister_done
#define trace_intel_context_deregister_done(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_do_pin
#define trace_intel_context_do_pin(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_do_unpin
#define trace_intel_context_do_unpin(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_fence_release
#define trace_intel_context_fence_release(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_free
#define trace_intel_context_free(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_register
#define trace_intel_context_register(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_reset
#define trace_intel_context_reset(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_sched_disable
#define trace_intel_context_sched_disable(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_sched_done
#define trace_intel_context_sched_done(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_sched_enable
#define trace_intel_context_sched_enable(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_set_prio
#define trace_intel_context_set_prio(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif
#ifndef trace_intel_context_steal_guc_id
#define trace_intel_context_steal_guc_id(...)	PSI915_TRACE_NOOP(__VA_ARGS__)
#endif

#endif /* _PSI915_I915_TRACE_H_ */
