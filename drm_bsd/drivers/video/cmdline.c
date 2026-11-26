/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * Per-output video mode options for psi915: clean-room replacement for Linux drivers/video/cmdline.c
 * (GPL-2.0), written from the interface's documented behaviour: video_get_options(name) returns
 * the option string given for that output, or NULL.  Not derived from the Linux file.
 *
 * Linux takes these from "video=<output>:<options>" on the kernel command line.  illumos has no
 * such command line; the illumos glue passes options (e.g. from i915.conf) through
 * video_bsd_set_option() before the DRM device registers its connectors.  A NULL name sets the
 * fallback option returned for outputs without their own entry.
 */

#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include <video/cmdline.h>

#define	VIDEO_BSD_MAX_OPTIONS	16

struct video_bsd_option {
	char	*name;		/* output name, e.g. "eDP-1"; NULL for the fallback */
	char	*option;
};

static DEFINE_MUTEX(video_bsd_lock);
static struct video_bsd_option video_bsd_options[VIDEO_BSD_MAX_OPTIONS];
static unsigned int video_bsd_noptions;
static char *video_bsd_fallback;

/* psi915 extension: record an option.  Returns 0, -ENOMEM or -ENOSPC.  Not for interrupt context. */
int
video_bsd_set_option(const char *name, const char *option)
{
	char *o, *n = NULL;
	int ret = 0;

	o = kstrdup(option, GFP_KERNEL);
	if (o == NULL)
		return (-ENOMEM);
	if (name != NULL && (n = kstrdup(name, GFP_KERNEL)) == NULL) {
		kfree(o);
		return (-ENOMEM);
	}
	mutex_lock(&video_bsd_lock);
	if (name == NULL) {
		kfree(video_bsd_fallback);
		video_bsd_fallback = o;
	} else if (video_bsd_noptions < VIDEO_BSD_MAX_OPTIONS) {
		video_bsd_options[video_bsd_noptions].name = n;
		video_bsd_options[video_bsd_noptions].option = o;
		video_bsd_noptions++;
	} else {
		kfree(n);
		kfree(o);
		ret = -ENOSPC;
	}
	mutex_unlock(&video_bsd_lock);
	return (ret);
}
EXPORT_SYMBOL(video_bsd_set_option);

/*
 * Look up the option for an output.  Returns true when the output has its own entry.  *option
 * receives that entry, or the fallback (possibly NULL) when there is none.  is_of (device-tree
 * naming) does not apply on illumos and is ignored.
 */
bool
__video_get_options(const char *name, const char **option, bool is_of)
{
	const char *found = NULL;
	bool named = false;
	unsigned int i;

	mutex_lock(&video_bsd_lock);
	for (i = 0; name != NULL && i < video_bsd_noptions; i++) {
		if (strcmp(video_bsd_options[i].name, name) == 0) {
			found = video_bsd_options[i].option;
			named = true;
			break;
		}
	}
	if (!named)
		found = video_bsd_fallback;
	mutex_unlock(&video_bsd_lock);
	if (option != NULL)
		*option = found;
	return (named);
}
EXPORT_SYMBOL(__video_get_options);

const char *
video_get_options(const char *name)
{
	const char *option = NULL;

	(void) __video_get_options(name, &option, false);
	return (option);
}
EXPORT_SYMBOL(video_get_options);
