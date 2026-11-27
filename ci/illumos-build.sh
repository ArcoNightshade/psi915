#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Build the kernel module set inside an illumos VM (CI). Runs under sh.
# Full output goes to ci-build.log; the console gets the environment, a
# condensed list of errors (so CI logs stay readable) and the result.
set -u
log=ci-build.log
: > "$log"
{
echo "== environment"; uname -a; head -3 /etc/release 2>/dev/null
command -v gcc gmake ld ctfconvert ctfmerge 2>&1
gcc --version 2>&1 | head -1
ls /usr/include/sys/ddi.h /usr/include/sys/sunddi.h 2>&1
} 2>&1 | tee -a "$log"
rc=0
{
echo "== gmake kmod (linuxkpi, lkpi_ktest, lkpi_toy)"
gmake -k kmod || rc=1
echo "== gfx modules (g1, drm, g2, i915)"
sh scripts/build-gfx.sh all -k -j4 || rc=1
} >> "$log" 2>&1
echo "== top error messages (count, message; file paths stripped; full log in ci-build.log)"
grep -E "error:|undefined reference" "$log" | sed -e 's/^[0-9T:.Z-]* //' -e 's/^[^ ]*: //' -e "s/[A-Za-z0-9_./-]*\.[ch]:[0-9]*:[0-9]*: //" | sort | uniq -c | sort -rn | head -60
echo "== files with most errors"
grep -E "error:" "$log" | sed -e 's/^[0-9T:.Z-]* //' | grep -o '^[^: ]*\.[ch]' | sort | uniq -c | sort -rn | head -25
echo "== first 30 raw error lines"
grep -E "error:" "$log" | sed -e 's/^[0-9T:.Z-]* //' | head -30
echo "== link / make / ctf problems (non-compile)"
grep -n -E "(^|[^a-z])ld: |fatal:|Undefined|undefined symbol|ctfconvert|ctfmerge|CTF|\*\*\* |Error [0-9]+|No rule|cannot|not found" "$log" | grep -v "error:" | sed 's/^[0-9]*://' | sort | uniq -c | sort -rn | head -40
echo "== last 40 lines of log"
tail -40 "$log"
echo "== error count: $(grep -c 'error:' "$log")  warning count: $(grep -c 'warning:' "$log")"
echo "== built objects: $(find build -name '*.o' 2>/dev/null | wc -l)  modules:"
find build -type f \( -path '*/obj64/*' -o -path '*/debug64/*' \) ! -name '*.o' ! -name '*.d' 2>/dev/null | head -20
echo "== make failures and linker/CTF messages (first 40)"
grep -E '\*\*\*|fatal:|^ld: |ctfconvert|ctfmerge|undefined symbol|Undefined' "$log" | sed -e 's/^[0-9T:.Z-]* //' | head -40
echo "== last 40 log lines"
tail -40 "$log"
echo "== result rc=$rc"
exit $rc
