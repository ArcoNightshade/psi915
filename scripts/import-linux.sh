#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
#
# Reproducible import of the Linux DRM core, display helpers, TTM, scheduler, dma-buf and i915 from the
# upstream Linux tag pinned in third_party/IMPORT_PIN into drm/, i915/ and gpl/.
#
#   1. resolve every upstream file below a manifest "scope" against third_party/manifest/{drm,i915,gpl}.list
#      (unclassified files abort; the manifest file decides the destination tree);
#   2. extract the selected files from the pinned commit with git archive (never from a working tree);
#   3. refuse GPL-only files outside gpl/ (licence from the pristine file's SPDX tag);
#   4. apply third_party/patches/series in order (quilt layout, -p1 relative to the repository root);
#   5. generate drm/include/generated/autoconf.h from third_party/config/gen9.config;
#   6. replace drm/, i915/, gpl/ and rewrite third_party/IMPORT_FILES (blob id, set, licence per file).
#
# Usage: scripts/import-linux.sh [-s set[,set]] [-S sets] [-P] [-n] [-c] [-m] [-l] [-R dir path...]
#   -s  add optional sets (debugfs fbdev compat capture perf selftest gen12 hwmon gvt kunit edidfw auxdev cec
#       replaced)
#   -S  replace the default set list (core,link,hdr,uapi,extra,aux)
#   -P  do not apply patches (pristine upstream; for making new patches)
#   -n  dry run: print "dest set path" for the selection and exit
#   -c  check: rebuild in a temporary directory and compare with the committed trees and IMPORT_FILES, then
#       run the five GPL-boundary checks of third_party/gpl-policy.conf (docs/08 section 5)
#   -m  manifest check only (classification complete, no stale entries)
#   -l  list "set<TAB>path" (or UNCLASSIFIED) for every upstream file in scope, without failing
#   -R  extract the given upstream paths (files or directories) from the pinned commit into dir and exit;
#       for reference reading only (e.g. -R ~/ref/linux-hdrs include/linux), never for committing
# Environment: REF (default $HOME/ref) or LINUX_SRC (default $REF/linux): shallow blobless clone to use or
#   create.  Requires git, tar, GNU patch (PATCH=gpatch on illumos), awk, sed, sort; bash.
set -euo pipefail
export LC_ALL=C

TOP=$(cd "$(dirname "$0")/.." && pwd)
TP=$TOP/third_party
TREES="drm i915 gpl"
SETS=core,link,hdr,uapi,extra,aux
ADD=; NOPATCH=0; DRY=0; CHECK=0; MONLY=0; LIST=0; REFOUT=
while getopts s:S:PncmlR:h o; do
	case $o in
	s) ADD=$OPTARG ;; S) SETS=$OPTARG ;; P) NOPATCH=1 ;; n) DRY=1 ;; c) CHECK=1 ;; m) MONLY=1 ;;
	l) LIST=1 ;; R) REFOUT=$OPTARG ;;
	*) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 2 ;;
	esac
done
shift $((OPTIND - 1))
[ -n "$ADD" ] && SETS=$SETS,$ADD

die() { echo "import-linux: $*" >&2; exit 1; }
pin() { sed -n "s/^$1=//p" "$TP/IMPORT_PIN" | head -1; }
URL=$(pin UPSTREAM_URL); TAG=$(pin UPSTREAM_TAG); COMMIT=$(pin UPSTREAM_COMMIT)
[ -n "$URL" ] && [ -n "$TAG" ] && [ -n "$COMMIT" ] || die "IMPORT_PIN lacks UPSTREAM_URL/TAG/COMMIT"
REF=${REF:-$HOME/ref}
SRC=${LINUX_SRC:-$REF/linux}

W=$(mktemp -d "${TMPDIR:-/tmp}/psi915-import.XXXXXX")
trap 'rm -rf "$W"' EXIT

# --- 1. upstream source -------------------------------------------------------------------------------
if [ ! -d "$SRC/.git" ]; then
	echo "import-linux: cloning $URL $TAG into $SRC (shallow, blobless)" >&2
	git clone --quiet --depth 1 --filter=blob:none --no-checkout --branch "$TAG" "$URL" "$SRC"
fi
if ! git -C "$SRC" cat-file -e "$COMMIT^{commit}" 2>/dev/null; then
	git -C "$SRC" fetch --quiet --depth 1 --filter=blob:none origin "refs/tags/$TAG:refs/tags/$TAG" \
		|| die "cannot fetch $TAG"
fi
got=$(git -C "$SRC" rev-parse "$COMMIT^{commit}" 2>/dev/null) || die "commit $COMMIT not in $SRC"
tagc=$(git -C "$SRC" rev-parse "refs/tags/$TAG^{commit}" 2>/dev/null || echo "")
[ -z "$tagc" ] || [ "$tagc" = "$got" ] || die "tag $TAG is $tagc, IMPORT_PIN says $COMMIT"

# Fetch the blobs listed in column $2 of file $1 in batches when the clone is partial (git would otherwise
# fetch them lazily one by one).
prefetch() {
	cut -f"$2" "$1" | GIT_NO_LAZY_FETCH=1 git -C "$SRC" cat-file --batch-check='%(objectname) %(objecttype)' \
		2>/dev/null | awk '$2=="missing"{print $1}' >"$W/missing" || true
	if [ -s "$W/missing" ]; then
		echo "import-linux: fetching $(wc -l <"$W/missing") blobs" >&2
		xargs -n 1000 git -C "$SRC" -c fetch.negotiationAlgorithm=noop fetch --quiet --no-tags \
			--no-write-fetch-head origin <"$W/missing" 2>/dev/null || true  # fallback: lazy fetch
	fi
}

if [ -n "$REFOUT" ]; then
	[ $# -gt 0 ] || die "-R needs upstream paths"
	mkdir -p "$REFOUT"
	git -C "$SRC" ls-tree -r "$COMMIT" -- "$@" | awk -F'\t' '{split($1,a," "); print a[3]}' >"$W/refblobs"
	prefetch "$W/refblobs" 1
	git -C "$SRC" archive --format=tar "$COMMIT" -- "$@" | tar -xf - -C "$REFOUT"
	echo "import-linux: extracted $(wc -l <"$W/refblobs") files into $REFOUT (reference only)" >&2
	exit 0
fi

# --- 2. manifest resolution ---------------------------------------------------------------------------
for l in $TREES; do
	[ -f "$TP/manifest/$l.list" ] || die "missing manifest $l.list"
	awk -F'\t' -v d=$l '!/^#/ && NF>=2 {print d "\t" $1 "\t" $2}' "$TP/manifest/$l.list"
done >"$W/manifest.tsv"
awk -F'\t' '$2=="scope" {print $3}' "$W/manifest.tsv" | sort -u >"$W/scopes"
[ -s "$W/scopes" ] || die "no scope lines in the manifests"
# shellcheck disable=SC2046
git -C "$SRC" ls-tree -r --full-tree "$COMMIT" -- $(cat "$W/scopes") \
	| awk -F'\t' '{split($1,a," "); print a[3] "\t" $2}' | sort -k2 >"$W/upstream.tsv"   # blob<TAB>path
# Output: dest<TAB>set<TAB>path<TAB>blob.  Longest manifest match wins; exact file entries beat prefixes.
awk -F'\t' -v list=$LIST '
	NR==FNR { if ($2=="scope") next; set[$3]=$2; dest[$3]=$1; if (seen[$3]++) dup=dup " " $3; next }
	{
		p=$2; best=""
		if (p in set) best=p
		else { q=p; while (sub(/[^\/]*\/?$/, "", q) && q != "") { if (q in set) { best=q; break } } }
		if (best=="") { if (list) print "UNCLASSIFIED\t" p; else print "UNCLASSIFIED\t" p > "/dev/stderr"; bad=1; next }
		used[best]=1
		if (list) print set[best] "\t" p; else print dest[best] "\t" set[best] "\t" p "\t" $1
	}
	END {
		if (list) exit 0
		if (dup != "") { print "DUPLICATE manifest entries:" dup > "/dev/stderr"; bad=1 }
		for (k in set) if (!(k in used)) print "STALE\t" k > "/dev/stderr"
		exit bad
	}' "$W/manifest.tsv" "$W/upstream.tsv" >"$W/resolved.tsv" \
	|| die "manifest does not classify every upstream file (see UNCLASSIFIED above)"
if [ $LIST = 1 ]; then cat "$W/resolved.tsv"; exit 0; fi
if [ $MONLY = 1 ]; then
	echo "import-linux: manifest OK: $(wc -l <"$W/resolved.tsv") upstream files classified" >&2
	exit 0
fi

awk -F'\t' -v sets="$SETS" 'BEGIN { n=split(sets, s, ","); for (i=1;i<=n;i++) want[s[i]]=1 }
	$2 != "exclude" && ($2 in want)' "$W/resolved.tsv" >"$W/selected.tsv"
[ -s "$W/selected.tsv" ] || die "empty selection for sets $SETS"
if [ $DRY = 1 ]; then cut -f1-3 "$W/selected.tsv"; exit 0; fi

# --- 3. extraction from the pinned commit -------------------------------------------------------------
prefetch "$W/selected.tsv" 4
mkdir -p "$W/up" "$W/out"
cut -f3 "$W/selected.tsv" >"$W/paths"
# shellcheck disable=SC2046
git -C "$SRC" archive --format=tar "$COMMIT" -- $(cat "$W/paths") | tar -xf - -C "$W/up"
while IFS="$(printf '\t')" read -r dest set path blob; do
	mkdir -p "$W/out/$dest/$(dirname "$path")"
	mv "$W/up/$path" "$W/out/$dest/$path"
done <"$W/selected.tsv"

# Licence of each pristine file: first SPDX tag, else a text heuristic.  Recorded, never edited.
lic() {
	local s
	s=$(head -n 40 "$1" | sed -n 's/.*SPDX-License-Identifier:[[:space:]]*\([^*]*\).*/\1/p' | head -1 | sed 's/[[:space:]]*$//')
	if [ -n "$s" ]; then echo "$s"; return; fi
	if head -n 60 "$1" | grep -q 'Permission is hereby granted, free of charge'; then echo "MIT(text)"
	elif head -n 60 "$1" | grep -q 'Permission to use, copy, modify'; then echo "HPND(text)"
	elif head -n 60 "$1" | grep -q 'GNU General Public License'; then echo "GPL(text)"
	elif head -n 60 "$1" | grep -q 'Redistribution and use in source and binary forms'; then echo "BSD(text)"
	else echo "NOASSERTION"; fi
}
{
	echo "# Generated by scripts/import-linux.sh; do not edit.  Upstream $URL $TAG ($COMMIT)"
	echo "# sets: $SETS"
	echo "# blob<TAB>destination<TAB>set<TAB>licence (pristine upstream file, before patches)"
	while IFS="$(printf '\t')" read -r dest set path blob; do
		printf '%s\t%s\t%s\t%s\n' "$blob" "$dest/$path" "$set" "$(lic "$W/out/$dest/$path")"
	done <"$W/selected.tsv" | sort -k2
} >"$W/IMPORT_FILES"

# GPL-only (no MIT/BSD alternative) code belongs in gpl/, the separate GPL-2.0 module (docs/08 section 5).
awk -F'\t' '!/^#/ && $2 !~ /^gpl\// && $4 ~ /GPL/ && $4 !~ /MIT|BSD|X11/ {print "  " $2 "\t" $4}' \
	"$W/IMPORT_FILES" >"$W/gplbad"
[ -s "$W/gplbad" ] && { cat "$W/gplbad" >&2; die "GPL-only files outside gpl/: move them to manifest/gpl.list"; }

# --- 4. patches ----------------------------------------------------------------------------------------
if [ $NOPATCH = 0 ] && [ -f "$TP/patches/series" ]; then
	while read -r p rest; do
		case $p in ''|'#'*) continue ;; esac
		[ -f "$TP/patches/$p" ] || die "series names missing patch $p"
		"${PATCH:-patch}" -s -p1 -d "$W/out" --batch --forward --no-backup-if-mismatch <"$TP/patches/$p" \
			|| die "patch $p does not apply"
	done <"$TP/patches/series"
fi

# --- 5. generated config header -----------------------------------------------------------------------
gen_autoconf() {
	echo "/* Generated by scripts/import-linux.sh from third_party/config/gen9.config; do not edit. */"
	echo "#ifndef PSI915_GENERATED_AUTOCONF_H"
	echo "#define PSI915_GENERATED_AUTOCONF_H"
	sed -n -e 's/^\(CONFIG_[A-Za-z0-9_]*\)=y$/#define \1 1/p' \
	       -e 's/^\(CONFIG_[A-Za-z0-9_]*\)=m$/#define \1_MODULE 1/p' \
	       -e 's/^\(CONFIG_[A-Za-z0-9_]*\)=\([-0-9x][0-9A-Fa-fx]*\)$/#define \1 \2/p' \
	       -e 's/^\(CONFIG_[A-Za-z0-9_]*\)=\(".*"\)$/#define \1 \2/p' \
	       -e 's/^# \(CONFIG_[A-Za-z0-9_]*\) is not set$/\/* \1 is not set *\//p' "$1"
	echo "#endif /* PSI915_GENERATED_AUTOCONF_H */"
}
mkdir -p "$W/out/drm/include/generated"
gen_autoconf "$TP/config/gen9.config" >"$W/out/drm/include/generated/autoconf.h"
for d in $TREES; do mkdir -p "$W/out/$d"; done

# --- GPL boundary checks (run by -c; docs/08 section 5, third_party/gpl-policy.conf) ------------------
# $1: root holding drm/ i915/ gpl/ as imported (the temporary rebuild).  drm_bsd/ is read from $TOP.
# Prints one line per check and returns non-zero if any check fails.
gpl_checks() {
	R=$1; P=$TP/gpl-policy.conf; C=$W/gplchk; mkdir -p "$C"; fails=0
	[ -f "$P" ] || { echo "import-linux: missing $P" >&2; return 1; }
	# Strip C comments well enough for symbol searches (kernel-doc lines start with "*").
	nocomment() { sed -e 's|//.*||' -e '/^[[:space:]]*\*/d' -e '/^[[:space:]]*\/\*/d' "$@" 2>/dev/null; }
	report() {	# number, description, file with offending lines
		if [ -s "$3" ]; then
			echo "import-linux: GPL check $1 FAILED: $2" >&2; sed 's/^/    /' "$3" >&2; fails=1
		else
			echo "import-linux: GPL check $1 OK: $2" >&2
		fi
	}
	# MIT/BSD files of the Gen9 build: drm/ and i915/ except set "extra" (not compiled), plus drm_bsd/.
	{ awk -F'\t' -v r="$R" '!/^#/ && $2 !~ /^gpl\// && $3 != "extra" && $2 ~ /\.[ch]$/ {print r "/" $2}' \
		"$W/IMPORT_FILES"; [ -d "$TOP/drm_bsd" ] && find "$TOP/drm_bsd" -name '*.[ch]'; } | sort >"$C/mitfiles"

	# 1. Licences: no GPL-only file outside gpl/; every drm_bsd file is BSD-2-Clause.
	awk -F'\t' '!/^#/ && $2 !~ /^gpl\// && $4 ~ /GPL/ && $4 !~ /MIT|BSD|X11/ {print $2 "\t" $4}' \
		"$W/IMPORT_FILES" >"$C/1"
	if [ -d "$TOP/drm_bsd" ]; then
		find "$TOP/drm_bsd" -name '*.[ch]' | sort | while read -r f; do
			head -n 3 "$f" | grep -q 'SPDX-License-Identifier: BSD-2-Clause' || echo "${f#"$TOP"/}: not BSD-2-Clause"
		done >>"$C/1"
	fi
	report 1 "GPL-only code exists only in gpl/; drm_bsd/ is BSD-2-Clause" "$C/1"

	# 2. Only the allowed GPL files (G1 dma-buf group, G2 DRM helpers) are compiled.
	awk -F'\t' '!/^#/ && $2 ~ /^gpl\/.*\.c$/ && ($3 == "core" || $3 == "link") {sub(/^gpl\//, "", $2); print $2}' \
		"$W/IMPORT_FILES" | sort >"$C/2a"
	awk '$1 == "g1" || $1 == "g2" {print $2}' "$P" | sort >"$C/2b"
	comm -23 "$C/2a" "$C/2b" | sed 's/$/: compiled GPL file not in gpl-policy.conf g1\/g2/' >"$C/2"
	report 2 "compiled GPL files are only the G1 (dma-buf) and G2 (DRM helper) sets" "$C/2"

	# 3. MIT/BSD code includes no GPL header from gpl/, except allow-header debts.
	awk '$1 == "allow-header" {print $2}' "$P" | sort >"$C/3allow"
	(cd "$R/gpl" && find . -name '*.h' | sed 's|^\./||') | sort | comm -23 - "$C/3allow" >"$C/3hdr"
	awk '{ if ($0 ~ /^include\//) print "<" substr($0, 9); else { n = split($0, a, "/"); print "\"" a[n] } }' \
		"$C/3hdr" | sort -u >"$C/3names"
	xargs grep -H -E '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]' <"$C/mitfiles" 2>/dev/null | awk -v names="$C/3names" '
		BEGIN { while ((getline l < names) > 0) want[l] = 1 }
		{
			f = $0; sub(/:.*/, "", f); inc = $0; sub(/^[^:]*:/, "", inc)
			if (match(inc, /<[^>]+>/)) key = "<" substr(inc, RSTART + 1, RLENGTH - 2)
			else if (match(inc, /"[^"]+"/)) { q = substr(inc, RSTART + 1, RLENGTH - 2); n = split(q, a, "/"); key = "\"" a[n] }
			else next
			if (key in want) print f ": includes GPL header " key
		}' | sed "s|$R/||; s|$TOP/||" >"$C/3"
	report 3 "MIT/BSD files include no GPL header (except allow-header debts)" "$C/3"

	# 4. G1 depends on the KPI only: no DRM headers and no includes outside its own directory.
	: >"$C/4"
	for g in $(awk '$1 == "g1" {print $2}' "$P"); do
		f=$R/gpl/$g; [ -f "$f" ] || continue
		grep -E '^[[:space:]]*#[[:space:]]*include' "$f" | while read -r line; do
			case $line in
			*'<drm/'*) echo "$g: $line (DRM header in G1)" ;;
			*'"'*) q=$(echo "$line" | sed 's/.*"\(.*\)".*/\1/')
			       [ -f "$(dirname "$f")/$q" ] || echo "$g: $line (outside G1)" ;;
			esac
		done >>"$C/4"
	done
	report 4 "G1 (dma-buf group) depends only on the KPI, never on drm/ or i915/" "$C/4"

	# 5. drm/ does not use G2 exports (G2 depends on drm, so drm must not depend on G2).
	for g in $(awk '$1 == "g2" {print $2}' "$P"); do
		[ -f "$R/gpl/$g" ] && sed -n 's/^EXPORT_SYMBOL[A-Z_]*(\([A-Za-z0-9_]*\)).*/\1/p' "$R/gpl/$g"
	done | sort -u >"$C/5syms"
	: >"$C/5"
	if [ -s "$C/5syms" ]; then
		grep "^$R/drm/.*\\.c\$" "$C/mitfiles" | while read -r f; do
			nocomment "$f" | grep -n -w -F -f "$C/5syms" | sed "s|^|${f#"$R"/}:|"
		done >"$C/5"
		[ -d "$TOP/drm_bsd" ] && find "$TOP/drm_bsd" -name '*.c' ! -path '*/i915/*' | sort | while read -r f; do
			nocomment "$f" | grep -n -w -F -f "$C/5syms" | sed "s|^|${f#"$TOP"/}:|"
		done >>"$C/5"
	fi
	report 5 "drm/ (and drm_bsd DRM core) never call G2 (drm_hdcp_helper, drm_gem_framebuffer_helper)" "$C/5"
	return $fails
}

# --- 6. install or check ------------------------------------------------------------------------------
if [ $CHECK = 1 ]; then
	rc=0
	for d in $TREES; do diff -r "$W/out/$d" "$TOP/$d" >&2 || rc=1; done
	cmp -s "$W/IMPORT_FILES" "$TP/IMPORT_FILES" || { echo "import-linux: IMPORT_FILES differs" >&2; rc=1; }
	gpl_checks "$W/out" || rc=1
	[ $rc = 0 ] && echo "import-linux: check OK: drm/ i915/ gpl/ match pin + manifest + patches + config; GPL policy holds" >&2
	exit $rc
fi
for d in $TREES; do rm -rf "${TOP:?}/$d"; cp -R "$W/out/$d" "$TOP/$d"; done
cp "$W/IMPORT_FILES" "$TP/IMPORT_FILES"
echo "import-linux: imported $(wc -l <"$W/selected.tsv") files (sets: $SETS)" >&2
