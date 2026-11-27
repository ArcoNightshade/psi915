#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Build the kernel module set inside an illumos VM (CI). Runs under sh.
# Prints the environment first so failures can be diagnosed from the log.
set -u
echo "== environment"; uname -a; cat /etc/release 2>/dev/null | head -3
command -v gcc gmake ld ctfconvert ctfmerge 2>&1
gcc --version 2>&1 | head -1
ls /usr/include/sys/ddi.h /usr/include/sys/sunddi.h 2>&1
rc=0
echo "== gmake kmod (linuxkpi, lkpi_ktest, lkpi_toy)"
gmake kmod || rc=1
echo "== gfx modules (g1, drm, g2, i915)"
sh scripts/build-gfx.sh all || rc=1
echo "== result rc=$rc"
exit $rc
