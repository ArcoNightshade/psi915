/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux firmware loader API
 * (src/linux_firmware.c). request_firmware(&fw, "i915/kbl_dmc_ver1_04.bin",
 * dev) reads /usr/lib/firmware/<name> whole with krtld's kobj file
 * functions (kobj_open_file), which works from attach(9E) once the root
 * file system is mounted. There is no user-space helper and no fallback
 * path; _nowait requests complete on the system workqueue.
 */
#ifndef _LINUXKPI_LINUX_FIRMWARE_H_
#define	_LINUXKPI_LINUX_FIRMWARE_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/gfp.h>

#define	LKPI_FIRMWARE_DIR	"/usr/lib/firmware"

struct device;
struct lkpi_module;

struct firmware {
	size_t		size;
	const u8	*data;
	void		*priv;
};

int request_firmware(const struct firmware **fw, const char *name,
    struct device *dev);
int firmware_request_nowarn(const struct firmware **fw, const char *name,
    struct device *dev);
int request_firmware_direct(const struct firmware **fw, const char *name,
    struct device *dev);
int request_firmware_into_buf(const struct firmware **fw, const char *name,
    struct device *dev, void *buf, size_t size);
int request_firmware_nowait(struct lkpi_module *module, bool uevent,
    const char *name, struct device *dev, gfp_t gfp, void *context,
    void (*cont)(const struct firmware *fw, void *context));
void release_firmware(const struct firmware *fw);
#define	firmware_request_platform(fw, n, d)	request_firmware(fw, n, d)

#endif /* _LINUXKPI_LINUX_FIRMWARE_H_ */
