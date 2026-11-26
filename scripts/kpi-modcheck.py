#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The psi915 Authors. All rights reserved.
"""
Module-load risk checks for the graphics module set that a syntax check
cannot catch (docs/11). Works on the objects that scripts/kpi-linkcheck.sh
builds (HOST_STUBS=1; run it first) and on tests/out/kmod_*.o for linuxkpi
(make check-kmod). NOT an illumos link; host gcc/binutils.

Checks:
  1. order   every undefined symbol of a module is defined by the module
             itself or by a module earlier in the load order
             linuxkpi <- linuxkpi_gpl_g1 <- linuxkpi_drm <- linuxkpi_gpl_g2
             <- i915 (or is an illumos symbol from the allowlists);
             references to a later module are errors (krtld resolves only
             against modules already loaded, via MODDEPS)
  2. dups    global symbols defined by more than one module (krtld binds
             the first one found: silent wrong binding); 2b lists weak
             symbols (an unresolved weak reference stays 0 at modload)
  3. illumos global symbols we define that illumos also defines in the
             kernel (unix, genunix and the misc modules we depend on);
             derived from an illumos-gate tree (-I, default
             ../ref/illumos): a module's own references bind locally, but
             another module's reference may bind to the illumos one, and
             the duplicate confuses mdb/DTrace
  4. relocs  relocation types krtld does not handle for kmods (no GOT, no
             TLS; R_X86_64_32 truncates kernel addresses silently)
  5. size    text/data/bss per module
  6. prefix  globals of linuxkpi that are neither Linux API names (present
             in a Linux v6.14 header tree, -L, default ../ref/linux-hdrs)
             nor lkpi_/linuxkpi_/drmglue_ prefixed

Output: tests/out/modcheck/report.txt (+ one file per check).
Usage: scripts/kpi-modcheck.py [-I illumos-src] [-L linux-hdrs]
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORDER = ['linuxkpi', 'linuxkpi_gpl_g1', 'drm', 'linuxkpi_gpl_g2', 'i915']
OK_RELOCS = {'R_X86_64_NONE', 'R_X86_64_64', 'R_X86_64_PC32',
             'R_X86_64_PLT32', 'R_X86_64_32S', 'R_X86_64_PC64',
             'R_X86_64_PC16', 'R_X86_64_PC8'}
PREFIXES = ('lkpi_', 'linuxkpi_', '__lkpi', 'drmglue_', '_init', '_fini',
            '_info')


def objs_of(mod):
    if mod == 'linuxkpi':
        return sorted(glob.glob(os.path.join(TOP, 'tests/out/kmod_*.o')))
    return sorted(glob.glob(os.path.join(TOP, 'build', mod, 'obj64/objs/**/*.o'),
                            recursive=True))


def nm(objs, args):
    if not objs:
        return []
    out = subprocess.run(['nm'] + args + objs, capture_output=True,
                         text=True).stdout
    return out.splitlines()


def defined(objs):
    syms = collections.defaultdict(list)
    cur = None
    for line in nm(objs, ['-g', '--defined-only', '-A']):
        f, _, rest = line.rpartition(':')
        parts = rest.split()
        if len(parts) == 3:
            syms[parts[2]].append(os.path.relpath(f.split(':')[0], TOP))
    return syms


def undefined(objs):
    syms = collections.defaultdict(list)
    for line in nm(objs, ['-u', '-A']):
        f, _, rest = line.rpartition(':')
        parts = rest.split()
        if parts:
            syms[parts[-1]].append(os.path.relpath(f.split(':')[0], TOP))
    return syms


def allowed():
    s = set()
    for p in ('tests/kmod_allowed_syms.txt', 'drmglue/kmod_allowed_syms.txt'):
        with open(os.path.join(TOP, p)) as f:
            for line in f:
                if line.strip() and not line.startswith('#'):
                    s.add(line.split()[0])
    return s


FUNC_RE = re.compile(r'^([A-Za-z_]\w*)\s*\(')
VAR_RE = re.compile(r'^(?!static\b|extern\b|typedef\b|struct\s+\w+\s*\{|#)'
                    r'[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*(\[[^\]]*\])*\s*(=[^;]*)?;\s*$')


def illumos_syms(root):
    """Global functions and variables defined in kernel sources."""
    dirs = ['uts/common', 'uts/intel', 'uts/i86pc', 'common']
    syms = {}
    for d in dirs:
        for path in glob.glob(os.path.join(root, 'usr/src', d, '**/*.c'),
                              recursive=True):
            try:
                with open(path, errors='replace') as f:
                    lines = f.read().split('\n')
            except OSError:
                continue
            depth = 0
            prev = ''
            for line in lines:
                if depth == 0:
                    m = FUNC_RE.match(line)
                    if m and 'static' not in prev and not prev.rstrip().endswith(';') \
                            and prev.strip() and not prev.startswith('#'):
                        syms.setdefault(m.group(1), os.path.relpath(path, root))
                    else:
                        m = VAR_RE.match(line)
                        if m and '(' not in line:
                            syms.setdefault(m.group(2), os.path.relpath(path, root))
                depth += line.count('{') - line.count('}')
                if depth < 0:
                    depth = 0
                if line.strip():
                    prev = line
    # Kernel interfaces declared extern in kernel headers.
    for d in ['uts/common', 'uts/intel', 'uts/i86pc']:
        for path in glob.glob(os.path.join(root, 'usr/src', d, '**/*.h'),
                              recursive=True):
            try:
                with open(path, errors='replace') as f:
                    text = f.read()
            except OSError:
                continue
            for m in re.finditer(r'^extern\b[^;(]*?\b(\w+)\s*(?:\(|;|\[)',
                                 text, re.M):
                syms.setdefault(m.group(1), os.path.relpath(path, root))
    # Assembly entry points (string and copy routines, locks, ...).
    for d in dirs:
        for path in glob.glob(os.path.join(root, 'usr/src', d, '**/*.[sS]'),
                              recursive=True):
            try:
                with open(path, errors='replace') as f:
                    text = f.read()
            except OSError:
                continue
            for m in re.finditer(r'\b(?:ENTRY|ENTRY_NP|ALTENTRY|ENTRY2|ENTRY_NP2)'
                                 r'\(\s*(\w+)(?:\s*,\s*(\w+))?\s*\)', text):
                for g in m.groups():
                    if g:
                        syms.setdefault(g, os.path.relpath(path, root))
    return syms


def linux_identifiers(root):
    ids = set()
    for path in glob.glob(os.path.join(root, 'include/**/*.h'), recursive=True) + \
            glob.glob(os.path.join(root, 'arch/x86/include/**/*.h'), recursive=True):
        try:
            with open(path, errors='replace') as f:
                ids.update(re.findall(r'\b[A-Za-z_]\w*\b', f.read()))
        except OSError:
            pass
    return ids


def relocs(objs):
    types = collections.defaultdict(set)
    for o in objs:
        out = subprocess.run(['readelf', '-rW', o], capture_output=True,
                             text=True).stdout
        sect = ''
        for line in out.splitlines():
            ms = re.match(r"Relocation section '([^']+)'", line)
            if ms:
                sect = ms.group(1)
                continue
            # Debug sections are not loaded (offsets, not addresses).
            if sect.startswith(('.rela.debug', '.rel.debug')):
                continue
            m = re.search(r'\b(R_X86_64_\w+)\b', line)
            if m:
                types[m.group(1)].add('%s [%s]' % (os.path.relpath(o, TOP), sect))
    return types


def size(objs):
    t = d = b = 0
    out = subprocess.run(['size'] + objs, capture_output=True, text=True).stdout
    for line in out.splitlines()[1:]:
        p = line.split()
        if len(p) >= 3 and p[0].isdigit():
            t += int(p[0]); d += int(p[1]); b += int(p[2])
    return t, d, b


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-I', default=os.path.join(TOP, '..', 'ref', 'illumos'))
    ap.add_argument('-L', default=os.path.join(TOP, '..', 'ref', 'linux-hdrs'))
    a = ap.parse_args()
    outd = os.path.join(TOP, 'tests/out/modcheck')
    os.makedirs(outd, exist_ok=True)
    rep = []
    objs = {m: objs_of(m) for m in ORDER}
    defs = {m: defined(objs[m]) for m in ORDER}
    undefs = {m: undefined(objs[m]) for m in ORDER}
    allow = allowed()

    # 1. load order
    bad = []
    for i, m in enumerate(ORDER):
        avail = set(defs[m])
        for e in ORDER[:i]:
            avail |= set(defs[e])
        later = set()
        for e in ORDER[i + 1:]:
            later |= set(defs[e])
        for s, users in sorted(undefs[m].items()):
            if s in avail or s in allow:
                continue
            where = 'LATER MODULE' if s in later else 'nowhere'
            bad.append('%s\t%s\t%s\t%s' % (m, s, where, users[0]))
    with open(os.path.join(outd, 'order.tsv'), 'w') as f:
        f.write('\n'.join(bad) + '\n')
    later_refs = [b for b in bad if 'LATER MODULE' in b]
    rep.append('1. load order: %d unresolved (%d defined only in a later module)'
               % (len(bad), len(later_refs)))
    rep += ['   ' + b for b in later_refs[:40]]

    # 2. duplicates across modules
    owner = collections.defaultdict(list)
    for m in ORDER:
        for s in defs[m]:
            # Module entry points exist once per module by design; the
            # stub-header check object is not part of any kmod.
            if s in ('_init', '_fini', '_info', 'kmod_headers_check'):
                continue
            owner[s].append(m)
    dups = sorted((s, ms) for s, ms in owner.items() if len(ms) > 1)
    with open(os.path.join(outd, 'dups.tsv'), 'w') as f:
        for s, ms in dups:
            f.write('%s\t%s\n' % (s, ' '.join(ms)))
    rep.append('2. globals defined by more than one module: %d' % len(dups))
    rep += ['   %s: %s' % (s, ' '.join(ms)) for s, ms in dups[:40]]

    # 2b. weak symbols: krtld leaves an unresolved weak reference at 0
    weak = []
    for m in ORDER:
        for line in nm(objs[m], ['-A']):
            f, _, rest = line.rpartition(':')
            p = rest.split()
            if len(p) >= 2 and p[-2] in ('w', 'v', 'W', 'V'):
                weak.append('%s\t%s\t%s\t%s' % (m, p[-2], p[-1],
                            os.path.relpath(f.split(':')[0], TOP)))
    with open(os.path.join(outd, 'weak.tsv'), 'w') as f:
        f.write('\n'.join(weak) + '\n')
    rep.append('2b. weak symbols (w/v undefined weak, W/V defined weak): %d'
               % len(weak))
    rep += ['   ' + w for w in weak[:40]]

    # 3. illumos clashes
    if os.path.isdir(a.I):
        isyms = illumos_syms(a.I)
        clash = sorted(s for s in owner if s in isyms)
        with open(os.path.join(outd, 'illumos-clash.tsv'), 'w') as f:
            for s in clash:
                f.write('%s\t%s\t%s\n' % (s, ' '.join(owner[s]), isyms[s]))
        rep.append('3. globals also defined in illumos kernel sources: %d '
                   '(heuristic over %s; review by hand)' % (len(clash), a.I))
        rep += ['   %s: ours %s, illumos %s' % (s, ' '.join(owner[s]), isyms[s])
                for s in clash]
        # 3b. illumos symbols we use that the tree does not define (typo,
        # macro, or code in usr/src/common, which this tree may lack).
        used = set()
        for m in ORDER:
            used |= set(undefs[m])
        kpi = set(defs['linuxkpi'])
        miss = sorted(x for x in used & allow if x not in isyms and x not in kpi)
        with open(os.path.join(outd, 'illumos-missing.tsv'), 'w') as f:
            f.write('\n'.join(miss) + '\n')
        rep.append('3b. allowlisted illumos symbols referenced but not found in '
                   'the illumos tree: %d' % len(miss))
        rep.append('   ' + ' '.join(miss))
    else:
        rep.append('3. illumos clash check skipped (no tree at %s)' % a.I)

    # 4. relocations
    rep.append('4. relocation types krtld may mishandle (outside %s):'
               % ' '.join(sorted(OK_RELOCS)))
    for m in ORDER:
        types = relocs(objs[m])
        badt = {t: fs for t, fs in types.items() if t not in OK_RELOCS}
        rep.append('   %s: %s' % (m, ', '.join('%s (%d objects, e.g. %s)'
                   % (t, len(fs), sorted(fs)[0]) for t, fs in sorted(badt.items()))
                   or 'none'))

    # 5. size
    rep.append('5. size (host objects, before ld -r; text/data/bss bytes):')
    for m in ORDER:
        t, d, b = size(objs[m])
        rep.append('   %-16s %9d %9d %9d  (%d objects)' % (m, t, d, b, len(objs[m])))

    # 6. prefix audit of linuxkpi exports
    if os.path.isdir(a.L):
        lids = linux_identifiers(a.L)
        odd = sorted(s for s in defs['linuxkpi']
                     if not s.startswith(PREFIXES) and s not in lids
                     and s != 'kmod_headers_check')
        with open(os.path.join(outd, 'prefix.tsv'), 'w') as f:
            for s in odd:
                f.write('%s\t%s\n' % (s, defs['linuxkpi'][s][0]))
        rep.append('6. linuxkpi globals that are neither Linux API names nor '
                   'lkpi_/linuxkpi_/drmglue_: %d' % len(odd))
        rep += ['   %s (%s)' % (s, defs['linuxkpi'][s][0]) for s in odd]
    else:
        rep.append('6. prefix audit skipped (no Linux headers at %s)' % a.L)

    with open(os.path.join(outd, 'report.txt'), 'w') as f:
        f.write('\n'.join(rep) + '\n')
    print('\n'.join(rep))
    return 0


if __name__ == '__main__':
    sys.exit(main())
