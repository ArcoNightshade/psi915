#!/usr/bin/awk -f
#
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
#
# uapi_cmp.awk: compare two uapi_dump outputs.
#
#   awk -f uapi_cmp.awk [-v mode=sides|abi] [-v strict=1] REF.txt OTHER.txt
#
# mode=sides (default): REF = kernel-side dump, OTHER = user-side dump.
#   FAIL  ioctl nr/type/direction differ, struct offset differs, or an ABI
#         constant differs (these break the ABI outright);
#   WARN  ioctl argument size differs (Linux drm_ioctl zero-extends/truncates,
#         so this works if the kernel glue keeps that behaviour), struct size
#         differs, or userland uses an ioctl/constant the kernel headers lack;
#   INFO  kernel-only ioctls/constants (newer kernel uAPI; harmless).
# mode=abi: REF = LP64 dump, OTHER = ILP32 dump of the same side. Lists every
#   struct, offset and ioctl argument size that differs: each one needs a
#   32-bit compat conversion in the kernel (ddi_model_convert_from) or must be
#   refused for 32-bit processes.
#
# strict=1 turns WARN into FAIL. Exit status: 0 ok, 1 FAIL, 2 usage/input.
# POSIX awk only (illumos /usr/bin/awk, nawk, gawk, mawk).

BEGIN {
	FS = "\t"
	if (mode == "") mode = "sides"
	nfail = nwarn = ninfo = 0
}

FNR == 1 { fileno++ }
/^#/ { next }

fileno == 1 { rec(1); next }
fileno == 2 { rec(2); next }

function rec(f,    key) {
	if ($1 == "H") { hdr[f, $2] = $3; return }
	if ($1 == "E") {
		key = $2
		has[f, "E", key] = 1
		dir[f, key] = $3; typ[f, key] = $4; nr[f, key] = $5
		sz[f, key] = $6; val[f, key] = $7; flg[f, key] = $10
		if (f == 1) order[++norder] = "E" SUBSEP key
		else if (!((1, "E", key) in has)) extra[++nextra] = "E" SUBSEP key
		return
	}
	if ($1 == "C" || $1 == "O" || $1 == "S") {
		key = $2
		has[f, $1, key] = 1
		v[f, $1, key] = $3
		if (f == 1) order[++norder] = $1 SUBSEP key
		else if (!((1, $1, key) in has)) extra[++nextra] = $1 SUBSEP key
		return
	}
	# A / CA (absent) records need no action
}

function report(level, msg) {
	if (level == "WARN" && strict) level = "FAIL"
	printf("%s\t%s\n", level, msg)
	if (level == "FAIL") nfail++
	else if (level == "WARN") nwarn++
	else ninfo++
}

END {
	if (fileno != 2) {
		print "usage: awk -f uapi_cmp.awk REF.txt OTHER.txt" > "/dev/stderr"
		exit 2
	}
	printf("# compare (%s): %s/%s/%s vs %s/%s/%s\n", mode,
	    hdr[1, "side"], hdr[1, "enc"], hdr[1, "abi"],
	    hdr[2, "side"], hdr[2, "enc"], hdr[2, "abi"])

	for (i = 1; i <= norder; i++) {
		split(order[i], p, SUBSEP); t = p[1]; k = p[2]
		if (!((2, t, k) in has)) {
			if (mode == "sides")
				report("INFO", t " " k ": kernel side only")
			continue
		}
		if (t == "E") {
			if (nr[1, k] != nr[2, k] || typ[1, k] != typ[2, k])
				report("FAIL", "E " k ": number differs (type/nr " \
				    typ[1, k] "/" nr[1, k] " vs " typ[2, k] "/" nr[2, k] ")")
			if (dir[1, k] != dir[2, k])
				report("FAIL", "E " k ": direction differs (" \
				    dir[1, k] " vs " dir[2, k] ")")
			if (sz[1, k] != sz[2, k]) {
				if (mode == "abi")
					report("FAIL", "E " k ": argument size " sz[1, k] \
					    " (LP64) vs " sz[2, k] " (ILP32): needs compat ioctl")
				else
					report("WARN", "E " k ": argument size " sz[1, k] \
					    " (kernel) vs " sz[2, k] " (user)")
			}
			continue
		}
		if (v[1, t, k] != v[2, t, k]) {
			lvl = (t == "S" && mode == "sides") ? "WARN" : "FAIL"
			# DRM_CLOEXEC is O_CLOEXEC of whichever system compiles it:
			# report, but the real check is the H O_CLOEXEC lines.
			if (t == "C" && k == "DRM_CLOEXEC") lvl = "WARN"
			report(lvl, t " " k ": " v[1, t, k] " vs " v[2, t, k])
		}
	}
	if (mode == "sides") {
		for (i = 1; i <= nextra; i++) {
			split(extra[i], p, SUBSEP)
			report("WARN", p[1] " " p[2] ": used by userland headers, " \
			    "absent from kernel headers")
		}
		if (hdr[1, "O_CLOEXEC"] != hdr[2, "O_CLOEXEC"])
			report("WARN", "O_CLOEXEC differs (" hdr[1, "O_CLOEXEC"] \
			    " vs " hdr[2, "O_CLOEXEC"] "): the kernel must test " \
			    "DRM_CLOEXEC/DRM_RDWR in PRIME/syncobj flags with the " \
			    "native illumos values")
	}
	printf("# result: %d FAIL, %d WARN, %d INFO\n", nfail, nwarn, ninfo)
	exit (nfail ? 1 : 0)
}
