#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
"""
P3 compile loop: syntax-check every imported DRM/i915 translation unit
against the KPI headers and record the first errors per file, so KPI
progress is measurable.

Each .c file of the selected import sets (third_party/IMPORT_FILES, default
core + link) is compiled with gcc -fsyntax-only in the illumos kernel
configuration used by `make check-kmod` (stub illumos headers, -D_KERNEL
-D__sun, plus -D__KERNEL__ and the generated autoconf.h, as kbuild would),
with include/ (KPI) first, then drm/include, drm/include/uapi,
i915/include, gpl/include and the directory of the file and of its driver
root (kbuild's -I$(src)).

Output (default directory tests/out/p3):
  results.tsv   file, set, status (ok|fail|timeout), errors, first error
  summary.txt   totals per tree/set, most frequent missing headers and
                undeclared identifiers (the KPI work list)

Usage: scripts/p3-compile-check.py [-j N] [-s core,link] [-o DIR] [-f REGEX]
Exit status is 0 even when files fail; it is a measuring tool.
"""

import argparse
import collections
import concurrent.futures
import os
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

KFLAGS = [
    '-std=gnu11', '-fsyntax-only', '-m64', '-mcmodel=kernel', '-mno-red-zone',
    '-ffreestanding', '-mno-mmx', '-mno-sse', '-fno-pic', '-w',
    '-fmax-errors=20', '-nostdinc',
    '-D_KERNEL', '-D_SYSCALL32', '-D_SYSCALL32_IMPL', '-D_ELF64',
    '-D_DDI_STRICT', '-Dsun', '-D__sun', '-D__SVR4', '-D__KERNEL__',
]


def includes(path):
    d = os.path.dirname(path)
    inc = [
        os.path.join(TOP, 'include'),
        os.path.join(TOP, 'drm/include'),
        os.path.join(TOP, 'drm/include/uapi'),
        os.path.join(TOP, 'i915/include'),
        os.path.join(TOP, 'gpl/include'),
        d,
    ]
    for root in ('i915/drivers/gpu/drm/i915', 'drm/drivers/gpu/drm'):
        r = os.path.join(TOP, root)
        if path.startswith(r) and r not in inc:
            inc.append(r)
    inc.append(os.path.join(TOP, 'tests/stubs/illumos'))
    flags = []
    for i in inc:
        flags += ['-I', i]
    # As kbuild: linux/kconfig.h (which includes generated/autoconf.h).
    flags += ['-include', os.path.join(TOP, 'include/linux/kconfig.h')]
    return flags


ERR_RE = re.compile(r'^(?P<file>[^:]+):(?P<line>\d+):(?:\d+:)? (?:fatal )?error: (?P<msg>.*)$')


def check(path, timeout):
    cmd = ['gcc'] + KFLAGS + includes(path) + [path]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                           env=dict(os.environ, LC_ALL='C'))
    except subprocess.TimeoutExpired:
        return ('timeout', [], '')
    errs = []
    for line in p.stderr.splitlines():
        m = ERR_RE.match(line)
        if m:
            errs.append((os.path.relpath(m.group('file'), TOP), m.group('msg')))
    status = 'ok' if p.returncode == 0 else 'fail'
    return (status, errs, p.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-j', type=int, default=os.cpu_count() or 2)
    ap.add_argument('-s', default='core,link', help='import sets')
    ap.add_argument('-o', default=os.path.join(TOP, 'tests/out/p3'))
    ap.add_argument('-f', default=None, help='only files matching this regex')
    ap.add_argument('-t', type=int, default=120, help='per-file timeout (s)')
    a = ap.parse_args()
    sets = set(a.s.split(','))
    files = []
    with open(os.path.join(TOP, 'third_party/IMPORT_FILES')) as f:
        for line in f:
            if line.startswith('#'):
                continue
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 3 or not parts[1].endswith('.c') or parts[2] not in sets:
                continue
            if a.f and not re.search(a.f, parts[1]):
                continue
            files.append((parts[1], parts[2]))
    os.makedirs(a.o, exist_ok=True)
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.j) as ex:
        futs = {ex.submit(check, os.path.join(TOP, p), a.t): (p, s) for p, s in files}
        for fu in concurrent.futures.as_completed(futs):
            results[futs[fu]] = fu.result()

    missing_hdr = collections.Counter()
    undeclared = collections.Counter()
    first_kind = collections.Counter()
    totals = collections.defaultdict(lambda: [0, 0])
    with open(os.path.join(a.o, 'results.tsv'), 'w') as out:
        out.write('file\tset\tstatus\terrors\tfirst_error\n')
        for (p, s) in sorted(results):
            status, errs, _ = results[(p, s)]
            key = p.split('/')[0] + ' ' + s
            totals[key][0] += 1
            if status == 'ok':
                totals[key][1] += 1
            first = '%s: %s' % errs[0] if errs else ''
            out.write('%s\t%s\t%s\t%d\t%s\n' % (p, s, status, len(errs), first))
            for i, (ef, msg) in enumerate(errs):
                m = re.search(r"([\w./-]+\.h): No such file", msg)
                if m:
                    missing_hdr[m.group(1)] += 1
                m = re.search(r"'(\w+)' undeclared|implicit declaration of function '(\w+)'|unknown type name '(\w+)'|storage size of '(\w+)' isn't known|invalid use of undefined type '([\w ]+)'", msg)
                if m:
                    undeclared[next(g for g in m.groups() if g)] += 1
                if i == 0:
                    first_kind[re.sub(r"'[^']*'", "'X'", msg)[:60]] += 1
    nok = sum(v[1] for v in totals.values())
    with open(os.path.join(a.o, 'summary.txt'), 'w') as out:
        out.write('P3 compile loop: %d/%d translation units compile (-fsyntax-only)\n\n'
                  % (nok, len(results)))
        for k in sorted(totals):
            out.write('  %-12s %4d/%4d ok\n' % (k, totals[k][1], totals[k][0]))
        out.write('\nMissing headers (files whose first errors include them):\n')
        for h, n in missing_hdr.most_common(60):
            out.write('  %4d  %s\n' % (n, h))
        out.write('\nUndeclared identifiers/types (occurrences in first errors):\n')
        for h, n in undeclared.most_common(80):
            out.write('  %4d  %s\n' % (n, h))
        out.write('\nFirst-error kinds:\n')
        for h, n in first_kind.most_common(30):
            out.write('  %4d  %s\n' % (n, h))
    sys.stdout.write(open(os.path.join(a.o, 'summary.txt')).read())
    return 0


if __name__ == '__main__':
    sys.exit(main())
