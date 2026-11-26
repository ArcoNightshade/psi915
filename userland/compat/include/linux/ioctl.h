/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * <linux/ioctl.h> for illumos userland: deliberately maps to the NATIVE
 * illumos encoding (<sys/ioccom.h>), the same one libdrm's and Mesa's drm.h
 * use on non-Linux hosts. Every DRM, sync_file and dma-buf ioctl number in a
 * process must use one encoding; the kernel (LinuxKPI drm_ioctl glue) decodes
 * it. See docs/09-userland-stack.md, "ioctl encoding". Untested on illumos.
 */
#ifndef _PSI915_COMPAT_LINUX_IOCTL_H
#define	_PSI915_COMPAT_LINUX_IOCTL_H

#if defined(__linux__)
#error "psi915 compat <linux/ioctl.h> is for non-Linux hosts only"
#endif

#include <sys/ioccom.h>

#endif /* _PSI915_COMPAT_LINUX_IOCTL_H */
