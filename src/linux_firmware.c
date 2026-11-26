/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Firmware loader (see <linux/firmware.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linuxkpi/plat_dev.h>

struct lkpi_fw {
	struct firmware	fw;
	void		*buf;
	size_t		buflen;
	bool		own;
};

static int
lkpi_request_fw(const struct firmware **fwp, const char *name,
    struct device *dev, bool warn, void *ubuf, size_t usize)
{
	struct lkpi_fw *f;
	char path[256];
	void *buf;
	size_t len;
	int r;

	*fwp = NULL;
	if (name == NULL || name[0] == '\0' || strstr(name, "..") != NULL)
		return (-EINVAL);
	(void) snprintf(path, sizeof (path), "%s/%s", LKPI_FIRMWARE_DIR,
	    name);
	r = lkpi_plat_read_file(path, &buf, &len);
	if (r != 0) {
		if (warn)
			dev_warn(dev, "firmware %s: error %d\n", path, r);
		return (r);
	}
	f = kzalloc(sizeof (*f), GFP_KERNEL);
	if (f == NULL) {
		lkpi_plat_free(buf, len > 0 ? len : 1);
		return (-ENOMEM);
	}
	if (ubuf != NULL) {
		if (len > usize) {
			lkpi_plat_free(buf, len > 0 ? len : 1);
			kfree(f);
			return (-EFBIG);
		}
		memcpy(ubuf, buf, len);
		lkpi_plat_free(buf, len > 0 ? len : 1);
		f->fw.data = ubuf;
	} else {
		f->buf = buf;
		f->buflen = len > 0 ? len : 1;
		f->own = true;
		f->fw.data = buf;
	}
	f->fw.size = len;
	f->fw.priv = f;
	*fwp = &f->fw;
	return (0);
}

int
request_firmware(const struct firmware **fw, const char *name,
    struct device *dev)
{
	return (lkpi_request_fw(fw, name, dev, true, NULL, 0));
}

int
firmware_request_nowarn(const struct firmware **fw, const char *name,
    struct device *dev)
{
	return (lkpi_request_fw(fw, name, dev, false, NULL, 0));
}

int
request_firmware_direct(const struct firmware **fw, const char *name,
    struct device *dev)
{
	return (lkpi_request_fw(fw, name, dev, false, NULL, 0));
}

int
request_firmware_into_buf(const struct firmware **fw, const char *name,
    struct device *dev, void *buf, size_t size)
{
	return (lkpi_request_fw(fw, name, dev, true, buf, size));
}

void
release_firmware(const struct firmware *fw)
{
	struct lkpi_fw *f;

	if (fw == NULL)
		return;
	f = fw->priv;
	if (f->own)
		lkpi_plat_free(f->buf, f->buflen);
	kfree(f);
}

struct lkpi_fw_async {
	struct work_struct	work;
	const char		*name;
	struct device		*dev;
	void			*context;
	void			(*cont)(const struct firmware *, void *);
};

static void
lkpi_fw_async_fn(struct work_struct *w)
{
	struct lkpi_fw_async *a = container_of(w, struct lkpi_fw_async, work);
	const struct firmware *fw;

	if (lkpi_request_fw(&fw, a->name, a->dev, true, NULL, 0) != 0)
		fw = NULL;
	a->cont(fw, a->context);
	put_device(a->dev);
	kfree(a->name);
	kfree(a);
}

int
request_firmware_nowait(struct lkpi_module *module, bool uevent,
    const char *name, struct device *dev, gfp_t gfp, void *context,
    void (*cont)(const struct firmware *fw, void *context))
{
	struct lkpi_fw_async *a;

	(void) module;
	(void) uevent;
	a = kzalloc(sizeof (*a), gfp);
	if (a == NULL)
		return (-ENOMEM);
	a->name = kstrdup(name, gfp);
	if (a->name == NULL) {
		kfree(a);
		return (-ENOMEM);
	}
	a->dev = get_device(dev);
	a->context = context;
	a->cont = cont;
	INIT_WORK(&a->work, lkpi_fw_async_fn);
	(void) schedule_work(&a->work);
	return (0);
}
