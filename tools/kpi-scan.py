#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Scan the imported DRM/i915 tree for the Linux kernel API it uses and compare with include/ (the KPI).

Scope: every compiled file of the Gen9 build (sets "core" and "link" in third_party/IMPORT_FILES, in drm/, i915/
and gpl/) plus every imported header they reach through #include.  Preprocessor conditionals are resolved where
they depend only on CONFIG_* (drm/include/generated/autoconf.h) and a few compiler/OS macros; anything else keeps
all branches.

What counts as "Linux API": an identifier the scanned files use but no imported file defines.  Kinds:
  call    identifier followed by "(" (function or function-like macro)
  tag     struct/union/enum tag
  macro   ALL_CAPS identifier not followed by "(" (constants, object-like macros)
  type    typedef-like names (u32, __le16, *_t, ...)
  ident   other identifiers that include/, FreeBSD's LinuxKPI or a Linux linux/ asm/ acpi/ header define as an
          object-like macro, typedef or enumerator (jiffies, __iomem, ...); other lowercase identifiers are
          assumed to be locals and dropped
Each name is mapped to the header that defines it in include/ (ours, macro-expanded in the check-kmod illumos
configuration; "illumos:" = visible through the stub illumos headers), in upstream Linux v6.14 (with that
header's SPDX licence), in FreeBSD's LinuxKPI and in illumos sys/.  Heuristic, text-based scan: no real
preprocessing, so token-pasted names, macro arguments and members can produce false entries, and APIs used only
through macros defined in KPI headers are not seen.  Counts are files, not call sites, unless stated.

It also maps the boundary between gpl/ (the separate GPL-2.0 module) and drm/ + i915/.

Usage: tools/kpi-scan.py [--freebsd DIR] [--illumos DIR] [--linux-ref DIR] [--out DIR]
  --linux-ref defaults to $REF/linux-hdrs, created with scripts/import-linux.sh -R when missing.
Writes: tools/kpi-requirements.tsv, tools/kpi-requirements.json, tools/kpi-headers.tsv,
        tools/kpi-config-usage.tsv, tools/kpi-unresolved-includes.tsv, tools/gpl-boundary.tsv,
        tools/gpl-headers.tsv
"""
import argparse, collections, json, os, re, sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TOKEN_RE = re.compile(r'[A-Za-z_]\w*|0[xX][0-9a-fA-F]+[uUlL]*|\d[\w.]*|->|\.\.\.|&&|\|\||[-+*/%&|^!=<>]=|<<|>>|##|\S')
STRIP_RE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
IDENT_RE = re.compile(r'^[A-Za-z_]\w*$')
CAPS_RE = re.compile(r'^_*[A-Z][A-Z0-9_]*$')
TYPE_RE = re.compile(r'^(?:__)?(?:[us](?:8|16|32|64|128)|le(?:16|32|64)|be(?:16|32|64)|wsum|sum16)$|_t$')

C_WORDS = set('''auto break case char const continue default do double else enum extern float for goto if inline int
long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while
_Bool _Complex _Static_assert _Generic _Alignof _Alignas _Noreturn _Thread_local __attribute__ __attribute
__typeof__ __typeof typeof __extension__ asm __asm__ __asm __volatile__ __volatile __inline__ __inline __restrict__
__restrict __const__ __signed__ __alignof__ __label__ __auto_type defined __func__ __FUNCTION__ __PRETTY_FUNCTION__
__LINE__ __FILE__ __DATE__ __TIME__ __COUNTER__ __VA_ARGS__ __VA_OPT__ __has_attribute __has_builtin __has_include
__has_feature __GNUC__ __GNUC_MINOR__ __clang__ __STDC_VERSION__ __x86_64__ __i386__ __amd64__ __aarch64__
__powerpc__ __riscv __LP64__ __CHAR_BIT__ __SIZEOF_LONG__ __SIZEOF_POINTER__ __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
__ORDER_BIG_ENDIAN__ __OPTIMIZE__ __CHECKER__ __cplusplus main'''.split())

# Values for preprocessor conditionals on illumos amd64 (gcc).  Anything else is unknown.
KNOWN_MACROS = {'__linux__': None, '__linux': None, 'linux': None, '__FreeBSD__': None, '__OpenBSD__': None,
                '__NetBSD__': None, '__DragonFly__': None, '__clang__': None, '__CHECKER__': None,
                '__cplusplus': None, '__i386__': None, '__aarch64__': None, '__powerpc__': None, '__riscv': None,
                '__x86_64__': 1, '__amd64__': 1, '__amd64': 1, '__KERNEL__': 1, '_KERNEL': 1, '__illumos__': 1,
                '__sun': 1, '__GNUC__': 13, '__LP64__': 1, 'LINUXKPI_VERSION': 61400, '__KERNEL_STRICT_NAMES': None}


def strip(text):
    """Remove comments (keeping line count) and keep string literals as-is."""
    def rep(m):
        s = m.group(0)
        if s.startswith('/*'):
            return ' ' + '\n' * s.count('\n')
        if s.startswith('//'):
            return ' '
        return s
    return STRIP_RE.sub(rep, text)


def blank_strings(text):
    return STRIP_RE.sub(lambda m: '""' if m.group(0)[0] in '"\'' else ' ', text)


def logical_lines(text):
    return text.replace('\\\n', ' ').split('\n')


class CondEval:
    def __init__(self, config):
        self.config = config  # name -> value (int or str) for defined CONFIG_*

    def value(self, name):
        if name.startswith('CONFIG_'):
            v = self.config.get(name)
            return (1 if not isinstance(v, int) else v) if v is not None else 0
        if name in KNOWN_MACROS:
            v = KNOWN_MACROS[name]
            return 0 if v is None else v
        return None

    def is_defined(self, name):
        if name.startswith('CONFIG_'):
            return name in self.config
        if name in KNOWN_MACROS:
            return KNOWN_MACROS[name] is not None
        return None

    def evaluate(self, expr):
        """Return True/False when determined, None when it depends on unknown macros."""
        e = re.sub(r'\b(IS_ENABLED|IS_BUILTIN|IS_REACHABLE)\s*\(\s*(CONFIG_\w+)\s*\)',
                   lambda m: '1' if m.group(2) in self.config else '0', expr)
        e = re.sub(r'\bIS_MODULE\s*\(\s*CONFIG_\w+\s*\)', '0', e)
        unknown = []

        def dfn(m):
            name = m.group(1) or m.group(2)
            d = self.is_defined(name)
            if d is None:
                unknown.append(name)
                return '__U%d' % (len(unknown) - 1)
            return '1' if d else '0'
        e = re.sub(r'\bdefined\s*\(\s*(\w+)\s*\)|\bdefined\s+(\w+)', dfn, e)
        out = []
        for t in TOKEN_RE.findall(e):
            if t.startswith('__U') and t[3:].isdigit():
                out.append(t)
            elif IDENT_RE.match(t):
                v = self.value(t)
                if v is None or isinstance(v, str):
                    unknown.append(t)
                    out.append('__U%d' % (len(unknown) - 1))
                else:
                    out.append(str(v))
            elif re.match(r'^\d', t) or t.startswith('0x') or t.startswith('0X'):
                out.append(re.sub(r'[uUlL]+$', '', t))
            elif t == '&&':
                out.append(' and ')
            elif t == '||':
                out.append(' or ')
            elif t == '!':
                out.append(' not ')
            elif t in ('?', ':'):
                return None
            else:
                out.append(t)
        py = ' '.join(out)
        results = set()
        for fill in (0, 1):
            env = {'__U%d' % i: fill for i in range(len(unknown))}
            try:
                results.add(bool(eval(py, {'__builtins__': {}}, env)))
            except Exception:
                return None
        return results.pop() if len(results) == 1 else None


def prune(lines, ev):
    """unifdef-style: drop lines in branches that are false for sure. Returns kept lines (others blanked)."""
    out = []
    # stack entries: [parent_active, branch_taken(True/False/None-unknown), cur_active]
    stack = []
    active = True
    for ln in lines:
        s = ln.strip()
        m = re.match(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', s)
        if m:
            kw, rest = m.group(1), m.group(2).strip()
            if kw in ('if', 'ifdef', 'ifndef'):
                if kw == 'if':
                    r = ev.evaluate(rest)
                else:
                    name = rest.split()[0] if rest else ''
                    d = ev.is_defined(name)
                    r = None if d is None else (d if kw == 'ifdef' else not d)
                stack.append([active, r, None])
                cur = active and (r is not False)
                stack[-1][2] = cur
                taken = r
                stack[-1][1] = taken
                active = cur
            elif kw == 'elif' and stack:
                parent, taken, _ = stack[-1]
                if taken is True:
                    cur = False
                else:
                    r = ev.evaluate(rest)
                    cur = parent and (r is not False)
                    if r is True and taken is False:
                        stack[-1][1] = True
                    elif r is None or taken is None:
                        stack[-1][1] = None if stack[-1][1] is not True else True
                stack[-1][2] = cur
                active = cur
            elif kw == 'else' and stack:
                parent, taken, _ = stack[-1]
                cur = parent and (taken is not True)
                stack[-1][2] = cur
                active = cur
            elif kw == 'endif' and stack:
                active = stack.pop()[0]
            out.append(ln if active or kw in ('endif',) else '')
            continue
        out.append(ln if active else '')
    return out


class FileScan:
    __slots__ = ('path', 'includes', 'uses', 'defs', 'configs', 'exports', 'paste')

    def __init__(self, path):
        self.path = path
        self.includes = []          # (kind '<' or '"', name)
        self.uses = collections.Counter()   # (kind, name) -> count
        self.defs = {}              # (ns, name) -> kind: macro fmacro func sfunc var svar typedef enum tag
        self.configs = set()
        self.exports = set()        # EXPORT_SYMBOL*(name)
        self.paste = set()          # token-paste prefixes seen in #define bodies (X##)


def scan_text(path, text, ev, collect_uses=True):
    fs = FileScan(path)
    text = strip(text)
    for c in re.findall(r'\bCONFIG_\w+', text):
        fs.configs.add(c)
    fs.exports.update(re.findall(r'\bEXPORT_SYMBOL(?:_GPL|_NS|_NS_GPL)?\s*\(\s*(\w+)', text))
    raw_lines = logical_lines(text)
    lines = prune(raw_lines, ev) if ev else raw_lines
    code = []
    for ln in lines:
        s = ln.strip()
        if s.startswith('#'):
            m = re.match(r'#\s*include\s*([<"])([^>"]+)[>"]', s)
            if m:
                fs.includes.append((m.group(1), m.group(2).strip()))
                continue
            m = re.match(r'#\s*define\s+([A-Za-z_]\w*)(\(([^)]*)\))?(.*)', s)
            if m:
                fs.defs[('id', m.group(1))] = 'fmacro' if m.group(2) else 'macro'
                if collect_uses:
                    params = set(p.strip() for p in (m.group(3) or '').split(',')) | {'__VA_ARGS__'}
                    body = blank_strings(m.group(4))
                    for frag in re.findall(r'([A-Za-z_]\w*)\s*##', body):
                        if frag not in params and len(frag) >= 3:
                            fs.paste.add(frag)
                    scan_tokens(fs, TOKEN_RE.findall(body), params, in_macro=True)
            continue
        code.append(ln)
    toks = TOKEN_RE.findall(re.sub(r'extern\s*""\s*\{', ' ', blank_strings('\n'.join(code))))
    scan_tokens(fs, toks, set(), in_macro=False, collect_uses=collect_uses)
    return fs


def scan_tokens(fs, toks, params, in_macro, collect_uses=True):
    brace = 0
    paren = 0
    n = len(toks)
    typedef_stmt = False
    enum_body = []   # brace levels that are enum bodies
    pending_enum = False
    last_ident_top = None
    decl_static = False
    for i, t in enumerate(toks):
        prev = toks[i - 1] if i else ';'
        nxt = toks[i + 1] if i + 1 < n else ''
        if t == '{':
            brace += 1
            if pending_enum:
                enum_body.append(brace)
                pending_enum = False
            continue
        if t == '}':
            if enum_body and enum_body[-1] == brace:
                enum_body.pop()
            brace = max(0, brace - 1)
            if brace == 0:
                decl_static = False
            continue
        if t == '(':
            paren += 1
            continue
        if t == ')':
            paren = max(0, paren - 1)
            continue
        if t == ';':
            if typedef_stmt and brace == 0 and last_ident_top:
                fs.defs[('id', last_ident_top)] = 'typedef'
            if brace == 0:
                typedef_stmt = False
                pending_enum = False
                decl_static = False
            last_ident_top = None
            continue
        if not IDENT_RE.match(t):
            continue
        if t in params:
            continue
        if t == 'static' and brace == 0 and paren == 0:
            decl_static = True
            continue
        if t == 'typedef' and not in_macro:
            typedef_stmt = True
            continue
        if t in ('struct', 'union', 'enum'):
            if IDENT_RE.match(nxt):
                after = toks[i + 2] if i + 2 < n else ''
                if after == '{':
                    fs.defs[('tag', nxt)] = 'tag'
                elif collect_uses:
                    fs.uses[('tag', nxt)] += 1
            if t == 'enum':
                pending_enum = True
            continue
        if prev in ('struct', 'union', 'enum'):
            continue
        if prev in ('.', '->'):
            continue
        if t in C_WORDS or t.startswith('__builtin_') or t.startswith('CONFIG_'):
            continue
        # enumerators
        if enum_body and enum_body[-1] == brace and prev in ('{', ',') and nxt in ('=', ',', '}'):
            fs.defs[('id', t)] = 'enum'
            continue
        if typedef_stmt and brace == 0:
            if prev == '*' and toks[i - 2] == '(' and nxt == ')':
                fs.defs[('id', t)] = 'typedef'
                last_ident_top = None
            elif paren == 0:
                last_ident_top = t
        prev_is_type = IDENT_RE.match(prev) is not None and prev not in ('return', 'else', 'case', 'goto', 'do') \
            or prev == '*'
        if not in_macro and brace == 0 and paren == 0:
            if nxt == '(' and prev_is_type:
                fs.defs[('id', t)] = 'sfunc' if decl_static else 'func'   # function declaration/definition
                continue
            if nxt in ('=', ';', '[', ',') and prev_is_type and not typedef_stmt:
                fs.defs[('id', t)] = 'svar' if decl_static else 'var'     # file-scope variable
                continue
        if not collect_uses:
            continue
        if nxt == '(':
            fs.uses[('call', t)] += 1
        elif CAPS_RE.match(t) and len(t) > 1:
            fs.uses[('macro', t)] += 1
        elif TYPE_RE.search(t):
            fs.uses[('type', t)] += 1
        else:
            fs.uses[('ident', t)] += 1


def load_config(path):
    cfg = {}
    if not os.path.exists(path):
        return cfg
    for ln in open(path):
        m = re.match(r'#define\s+(CONFIG_\w+)\s+(.*)', ln)
        if m:
            v = m.group(2).strip()
            cfg[m.group(1)] = int(v, 0) if re.match(r'^-?(0x[0-9a-fA-F]+|\d+)$', v) else v
    return cfg


# Reference headers (include/, FreeBSD LinuxKPI, illumos): only compiler/OS macros are resolved.
class _RefEval(CondEval):
    def value(self, name):
        return None if name.startswith('CONFIG_') else super().value(name)

    def is_defined(self, name):
        return None if name.startswith('CONFIG_') else super().is_defined(name)


REF_EV = _RefEval({})


def header_defs(root, subdirs=None):
    """(ns, name) -> {header rel path: kind} for definitions in headers below root, plus the header list."""
    defs = collections.defaultdict(dict)
    headers = set()
    if not os.path.isdir(root):
        return defs, headers
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith('.h'):
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, root)
            headers.add(rel)
            if subdirs and rel.split('/')[0] not in subdirs:
                continue
            fs = scan_text(rel, open(p, errors='replace').read(), REF_EV, collect_uses=False)
            for k, kind in fs.defs.items():
                defs[k][rel] = kind
    return defs, headers


KMOD_FLAGS = ['-std=gnu11', '-m64', '-ffreestanding', '-nostdinc', '-D_KERNEL', '-D_SYSCALL32',
              '-D_SYSCALL32_IMPL', '-D_ELF64', '-D_DDI_STRICT', '-Dsun', '-D__sun', '-D__SVR4']


def expanded_defs(include, stubs):
    """Definitions include/ provides after macro expansion, in the illumos-kernel configuration of
    tests/Makefile (check-kmod), against the stub illumos headers.  This sees the functions that KPI
    generator macros (e.g. __LKPI_ATOMIC_OPS) create.  Returns ((ns,name) -> {header: kind}) or None."""
    import subprocess, tempfile
    hdrs = sorted(os.path.relpath(os.path.join(dp, f), include) for dp, _, fs in os.walk(include)
                  for f in fs if f.endswith('.h') and dp.split(os.sep)[-1] in ('linux', 'asm'))
    tu = ''.join(f'#include <{h}>\n' for h in hdrs)
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
        f.write('#include <sys/types.h>\n#include <sys/ddi.h>\n#include <sys/sysmacros.h>\n' + tu)
        tmp = f.name
    try:
        out = subprocess.run(['gcc', '-E', '-dD', *KMOD_FLAGS, '-I', include, '-I', stubs, tmp],
                             capture_output=True, text=True)
    except OSError:
        return None
    finally:
        os.unlink(tmp)
    if out.returncode != 0:
        sys.stderr.write('kpi-scan: expanded include/ scan failed, falling back to raw headers:\n'
                         + out.stderr[:2000])
        return None
    chunks = collections.defaultdict(list)
    cur = None
    for ln in out.stdout.split('\n'):
        m = re.match(r'# \d+ "([^"]+)"', ln)
        if m:
            cur = m.group(1)
            continue
        if cur:
            chunks[cur].append(ln)
    defs = collections.defaultdict(dict)
    inc = os.path.abspath(include) + os.sep
    stb = os.path.abspath(stubs) + os.sep
    for fn, lines in chunks.items():
        af = os.path.abspath(fn)
        if af.startswith(inc):
            rel = af[len(inc):]
        elif af.startswith(stb):
            rel = 'illumos:' + af[len(stb):]   # illumos name visible through include/ (stub headers)
        else:
            continue
        fs = scan_text(rel, '\n'.join(lines), None, collect_uses=False)
        for k, kind in fs.defs.items():
            defs[k][rel] = kind
    return defs


TREES = ('drm', 'i915', 'gpl')
I915_REL = 'drivers/gpu/drm/i915/'


def linux_ref_headers(ref):
    """(ns,name) -> {linux header: kind}, header -> SPDX, from an extraction of upstream include trees.
    Paths are mapped to the name used in #include (<linux/x.h>, <asm/x.h>, ...)."""
    defs = collections.defaultdict(dict)
    lic = {}
    maps = [('include/uapi/', ''), ('arch/x86/include/uapi/', ''), ('arch/x86/include/generated/', ''),
            ('arch/x86/include/', ''), ('include/', '')]
    for dp, _, fns in os.walk(ref):
        for fn in fns:
            if not fn.endswith('.h'):
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, ref)
            name = None
            for pre, rep in maps:
                if rel.startswith(pre):
                    name = rep + rel[len(pre):]
                    break
            if name is None or name.startswith(('drm/', 'uapi/drm/')):
                continue
            text = open(p, errors='replace').read()
            m = re.search(r'SPDX-License-Identifier:\s*([^*\n]*)', text[:3000])
            lic.setdefault(name, m.group(1).strip().rstrip('*/').strip() if m else 'no notice (GPL-2.0 default)')
            fs = scan_text(name, text, REF_EV, collect_uses=False)
            for k, kind in fs.defs.items():
                defs[k].setdefault(name, kind)
    return defs, lic


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--freebsd', default=os.path.expanduser('~/ref/freebsd/sys/compat/linuxkpi'))
    ap.add_argument('--illumos', default=os.path.expanduser('~/ref/illumos'))
    ap.add_argument('--linux-ref', default=None,
                    help='extraction of upstream include/, arch/x86/include (default $REF/linux-hdrs, '
                         'created with scripts/import-linux.sh -R when missing)')
    ap.add_argument('--include', default=os.path.join(TOP, 'include'))
    ap.add_argument('--out', default=os.path.join(TOP, 'tools'))
    a = ap.parse_args()
    for attr, sub in (('freebsd', 'freebsd/sys/compat/linuxkpi'), ('illumos', 'illumos')):
        if not os.path.isdir(getattr(a, attr)) and os.path.isdir('/home/user/ref/' + sub):
            setattr(a, attr, '/home/user/ref/' + sub)
        if not os.path.isdir(getattr(a, attr)):
            sys.exit(f'kpi-scan: reference tree for --{attr} not found (run scripts/fetch-refs.sh)')
    ref_root = os.environ.get('REF') or (os.path.expanduser('~/ref') if os.path.isdir(os.path.expanduser('~/ref/linux'))
                                         else '/home/user/ref')
    if a.linux_ref is None:
        a.linux_ref = os.path.join(ref_root, 'linux-hdrs')
    if not os.path.isdir(a.linux_ref):
        import subprocess
        subprocess.run([os.path.join(TOP, 'scripts/import-linux.sh'), '-R', a.linux_ref, 'include/linux',
                        'include/asm-generic', 'include/uapi/linux', 'include/uapi/asm-generic', 'include/acpi',
                        'include/video', 'include/trace', 'include/media', 'include/net', 'include/crypto',
                        'include/xen', 'include/kunit', 'include/vdso', 'arch/x86/include'],
                       check=True, env=dict(os.environ, REF=ref_root))

    files = {}   # path -> set
    lic_of = {}
    for ln in open(os.path.join(TOP, 'third_party/IMPORT_FILES')):
        if ln.startswith('#'):
            continue
        blob, path, st, lic = ln.rstrip('\n').split('\t')
        files[path] = st
        lic_of[path] = lic
    cfg = load_config(os.path.join(TOP, 'drm/include/generated/autoconf.h'))
    ev = CondEval(cfg)

    scans = {}
    for path, st in files.items():
        if st == 'aux' or not path.endswith(('.c', '.h')):
            continue
        scans[path] = scan_text(path, open(os.path.join(TOP, path), errors='replace').read(), ev)
    tree_defs = set()
    paste = set()
    for fs in scans.values():
        tree_defs |= set(fs.defs)
        paste |= fs.paste
    # Definitions that exist only in branches that are false for the Gen9 configuration (raw, unpruned scan).
    tree_defs_raw = set()
    for path in scans:
        tree_defs_raw |= set(scan_text(path, open(os.path.join(TOP, path), errors='replace').read(), None,
                                       collect_uses=False).defs)
    paste_prefixes = tuple(sorted(paste))

    def split(p):
        t, rest = p.split('/', 1)
        return t, rest

    def resolve(src, kind, name):
        _, rest = split(src)
        cands = []
        if kind == '"':
            d = os.path.dirname(rest)
            for t in TREES:
                cands.append(os.path.normpath(f'{t}/{d}/{name}'))
            if rest.startswith(I915_REL):
                for t in TREES:
                    cands += [os.path.normpath(f'{t}/{I915_REL}{name}'), f'{t}/{I915_REL}display/{name}']
            if rest.startswith('drivers/gpu/drm/'):
                for t in TREES:
                    cands.append(f'{t}/drivers/gpu/drm/{name}')
        for t in TREES:
            cands += [f'{t}/include/{name}', f'{t}/include/uapi/{name}']
        for c in cands:
            if c in scans:
                return c
        return None

    roots = [p for p, st in files.items() if st in ('core', 'link') and p.endswith('.c')]
    closure = set(roots)
    work = list(roots)
    ext_includes = collections.defaultdict(set)
    unresolved = collections.defaultdict(set)
    inc_edges = collections.defaultdict(set)   # header -> including files (within tree)
    while work:
        p = work.pop()
        for kind, name in scans[p].includes:
            r = resolve(p, kind, name)
            if r:
                inc_edges[r].add(p)
                if r not in closure:
                    closure.add(r)
                    work.append(r)
            elif kind == '"' or name.endswith('.c'):
                unresolved[name].add(p)
            else:
                ext_includes[name].add(p)

    def category(p):
        st = files.get(p, '?')
        return st if st in ('core', 'link') else 'hdr'

    def tree(p):
        return p.split('/', 1)[0]

    # Reference definitions.
    our_raw, our_headers = header_defs(a.include)
    exp = expanded_defs(a.include, os.path.join(TOP, 'tests/stubs/illumos'))
    our_defs = collections.defaultdict(dict)
    for src in (our_raw, exp or {}):
        for k, v in src.items():
            our_defs[k].update(v)
    lx_defs, lx_lic = linux_ref_headers(a.linux_ref)
    LINUXISH = ('linux', 'asm', 'asm-generic', 'acpi', 'video', 'uapi')
    fb_defs, fb_headers = header_defs(os.path.join(a.freebsd, 'common/include'), LINUXISH)
    _, fb_dummy = header_defs(os.path.join(a.freebsd, 'dummy/include'))
    ill_defs = collections.defaultdict(dict)
    for d in ('common/sys', 'intel/sys', 'i86pc/sys'):
        root = os.path.join(a.illumos, 'usr/src/uts', d)
        for k, v in header_defs(root)[0].items():
            ill_defs[k].update({'sys/' + h: kd for h, kd in v.items()})
    # Lowercase non-call identifiers count as API only if include/ or FreeBSD define them, or a Linux linux/, asm/
    # or acpi/ header defines them as an object-like macro, typedef or enumerator (others are locals/members).
    api_universe = set(our_defs) | set(fb_defs) | {
        k for k, v in lx_defs.items()
        if len(k[1]) >= 5 and any(kd in ('macro', 'typedef', 'enum') and h.startswith(('linux/', 'asm/', 'acpi/'))
                                  and not h.startswith('linux/decompress/') for h, kd in v.items())}

    usage = {}
    for p in sorted(closure):
        for (kind, name), cnt in scans[p].uses.items():
            ns = 'tag' if kind == 'tag' else 'id'
            if (ns, name) in tree_defs:
                continue
            if kind == 'ident' and (ns, name) not in api_universe:
                continue
            usage.setdefault((kind, name), collections.Counter())[p] += cnt

    KRANK = {'typedef': 0, 'tag': 0, 'enum': 0, 'macro': 0, 'fmacro': 0, 'func': 1, 'sfunc': 1, 'var': 2, 'svar': 2}
    PREF = ('linux/types.h', 'linux/string.h', 'linux/kernel.h', 'linux/errno.h', 'asm-generic/errno-base.h')

    def pick(hs):
        if not hs:
            return ''
        return sorted(hs, key=lambda h: (h not in PREF, KRANK.get(hs[h], 3), not h.startswith('linux/'),
                                         h.startswith('uapi/'), h.count('/'), h))[0]

    rows = []
    for (kind, name), u in usage.items():
        ns = 'tag' if kind == 'tag' else 'id'
        ours, lx = our_defs.get((ns, name), {}), lx_defs.get((ns, name), {})
        fb, il = fb_defs.get((ns, name), {}), ill_defs.get((ns, name), {})
        status = ('provided' if ours else 'missing' if (lx or fb) else 'illumos-sys' if il
                  else 'tree-conditional' if (ns, name) in tree_defs_raw
                  else 'tree-generated' if (name.startswith(paste_prefixes)
                                            or any((ns, pf + name) in tree_defs for pf in paste_prefixes))
                  else 'missing-unknown')
        c = collections.Counter(category(p) for p in u)
        lh = pick(lx)
        rows.append({'kind': kind, 'name': name, 'status': status, 'include_header': pick(ours),
                     'linux_header': lh, 'linux_header_licence': lx_lic.get(lh, ''), 'freebsd_header': pick(fb),
                     'illumos_header': pick(il), 'files_core': c['core'], 'files_link': c['link'],
                     'files_hdr': c['hdr'], 'files_gpl': sum(1 for p in u if tree(p) == 'gpl'),
                     'files_total': len(u), 'uses_total': sum(u.values()), 'files': sorted(u)})
    rows.sort(key=lambda r: (-r['files_core'], -r['files_total'], r['kind'], r['name']))
    os.makedirs(a.out, exist_ok=True)
    cols = ('kind', 'name', 'status', 'include_header', 'linux_header', 'linux_header_licence', 'freebsd_header',
            'illumos_header', 'files_core', 'files_link', 'files_hdr', 'files_gpl', 'files_total', 'uses_total')
    with open(os.path.join(a.out, 'kpi-requirements.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py; see its docstring for method and limits.\n')
        f.write('# status: provided (defined in include/), missing (defined by a Linux v6.14 or FreeBSD LinuxKPI header, '
                'not by include/), illumos-sys (only illumos sys/ headers define the name: errno, libc-like; include/ '
                'must expose it with Linux semantics), tree-conditional (defined in the imported tree only under '
                'CONFIG choices that are off), tree-generated (probably made by token pasting in an imported macro), '
                'missing-unknown (in no reference; review by hand).  files_gpl counts users in gpl/.\n')
        f.write('\t'.join(cols) + '\tfiles\n')
        for r in rows:
            f.write('\t'.join(str(r[k]) for k in cols) + '\t' + ' '.join(r['files']) + '\n')
    with open(os.path.join(a.out, 'kpi-requirements.json'), 'w') as f:
        json.dump({'generator': 'tools/kpi-scan.py', 'scanned_files': sorted(closure),
                   'file_sets': {p: category(p) for p in sorted(closure)}, 'symbols': rows}, f, indent=0)

    with open(os.path.join(a.out, 'kpi-headers.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py.  Angle-bracket includes not satisfied by the imported tree.\n')
        f.write('# status: provided (include/ has it), missing, freebsd-dummy (FreeBSD satisfies it with an empty '
                'header).  linux_licence: SPDX of the upstream header (GPL ones must be written clean-room for '
                'include/; MIT/BSD/dual ones could be imported).\n')
        f.write('header\tstatus\tlinux_licence\tfiles_core\tfiles_link\tfiles_hdr\tfiles_total\tfiles\n')
        hrows = []
        for h, fl in ext_includes.items():
            s = ('provided' if h in our_headers else 'freebsd-dummy' if (h in fb_dummy and h not in fb_headers)
                 else 'missing')
            c = collections.Counter(category(p) for p in fl)
            hrows.append((-c['core'], -len(fl), h, s, c, fl))
        for _, _, h, s, c, fl in sorted(hrows):
            f.write(f"{h}\t{s}\t{lx_lic.get(h, '')}\t{c['core']}\t{c['link']}\t{c['hdr']}\t{len(fl)}\t"
                    f"{' '.join(sorted(fl))}\n")

    with open(os.path.join(a.out, 'kpi-unresolved-includes.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py.  Quoted includes in the Gen9 closure that resolve to no imported '
                'file (after CONFIG pruning).  Empty means the import is include-closed.\n')
        f.write('include\tfiles\n')
        for h in sorted(unresolved):
            f.write(f"{h}\t{' '.join(sorted(unresolved[h]))}\n")

    cfg_files = collections.defaultdict(set)
    for p in closure:
        for c in scans[p].configs:
            cfg_files[c].add(p)
    with open(os.path.join(a.out, 'kpi-config-usage.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py.  CONFIG_* symbols referenced by the Gen9 closure and their value in '
                'drm/include/generated/autoconf.h ("unset" = not set).\n')
        f.write('symbol\tvalue\tfiles_total\tfiles\n')
        for c in sorted(cfg_files, key=lambda c: (-len(cfg_files[c]), c)):
            f.write(f"{c}\t{cfg.get(c, 'unset')}\t{len(cfg_files[c])}\t{' '.join(sorted(cfg_files[c]))}\n")

    # GPL module boundary: symbols defined in one side and used by the other, plus GPL headers included by
    # drm/ and i915/ files.
    defs_by_file = {p: scans[p].defs for p in closure}
    definer = collections.defaultdict(set)   # (ns,name) -> files defining it (non-static)
    for p, d in defs_by_file.items():
        for k, kind in d.items():
            if kind not in ('sfunc', 'svar'):
                definer[k].add(p)
    brows = []
    for (ns, name), dfs in definer.items():
        for side in ('gpl', 'main'):
            src = sorted(p for p in dfs if (tree(p) == 'gpl') == (side == 'gpl'))
            if not src:
                continue
            if any((tree(p) == 'gpl') != (side == 'gpl') for p in dfs):
                continue   # defined on both sides (e.g. prototype in a main header): skip ambiguous
            users = sorted(p for p in closure if (tree(p) == 'gpl') != (side == 'gpl') and
                           any(scans[p].uses.get((k, name)) for k in (('tag',) if ns == 'tag' else
                                                                       ('call', 'macro', 'type', 'ident'))))
            if not users:
                continue
            kinds = {defs_by_file[p][(ns, name)] for p in src}
            exported = any(name in scans[p].exports for p in src)
            via = 'header' if all(p.endswith('.h') for p in src) else 'function/variable'
            # Drop locals/members that the top-level heuristics mistook for definitions.
            if via == 'function/variable':
                cfiles = [p for p in src if p.endswith('.c')]
                ckinds = {defs_by_file[p][(ns, name)] for p in cfiles}
                if name.startswith('__') or TYPE_RE.search(name) or name == 'bool' or \
                        not ('func' in ckinds or ('var' in ckinds and (exported or '_' in name))):
                    continue
            elif ns == 'id' and not ('_' in name or name.isupper()) or len(name) < 4:
                continue
            brows.append(('gpl->main' if side == 'gpl' else 'main->gpl', name, '/'.join(sorted(kinds)), via,
                          'yes' if exported else 'no', ' '.join(src), len(users), ' '.join(users)))
    brows.sort(key=lambda r: (r[0], r[3] != 'function/variable', -r[6], r[1]))
    with open(os.path.join(a.out, 'gpl-boundary.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py.  Symbols crossing the boundary between gpl/ (GPL-2.0 module) and '
                'drm/ + i915/ in the Gen9 closure.\n# direction gpl->main: defined in gpl/, used by drm/ or i915/ '
                '(the GPL module must export it, or it is header-only code compiled into the other module); '
                'main->gpl: defined in drm/ or i915/, used by gpl/.  via=header means a macro, inline, type or '
                'prototype from a header: compiled into the user, no symbol crosses at link time.\n')
        f.write('direction\tsymbol\tkind\tvia\tupstream_EXPORT_SYMBOL\tdefined_in\tusers\tuser_files\n')
        for r in brows:
            f.write('\t'.join(str(x) for x in r) + '\n')
    with open(os.path.join(a.out, 'gpl-headers.tsv'), 'w') as f:
        f.write('# Generated by tools/kpi-scan.py.  GPL-only headers in gpl/ that drm/ or i915/ files include '
                'directly.\n')
        f.write('header\tlicence\tmain_includers\tfiles\n')
        for h in sorted(inc_edges):
            if tree(h) != 'gpl' or not h.endswith('.h'):
                continue
            inc = sorted(p for p in inc_edges[h] if tree(p) != 'gpl')
            if inc:
                f.write(f"{h}\t{lic_of.get(h, '')}\t{len(inc)}\t{' '.join(inc)}\n")

    st = collections.Counter(r['status'] for r in rows)
    stc = collections.Counter(r['status'] for r in rows if r['files_core'])
    print(f'scanned {len(closure)} files ({sum(1 for p in closure if category(p)=="core")} core, '
          f'{sum(1 for p in closure if category(p)=="link")} link, '
          f'{sum(1 for p in closure if category(p)=="hdr")} headers; '
          f'{sum(1 for p in closure if tree(p)=="gpl")} in gpl/)')
    print(f'symbols: {len(rows)} {dict(st)}')
    print(f'used by core files: {dict(stc)}')
    print(f'external headers: {len(ext_includes)}; unresolved quoted includes: {len(unresolved)}; '
          f'boundary rows: {len(brows)}')



if __name__ == '__main__':
    main()
