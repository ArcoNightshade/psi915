# third_party

Inputs and records for the scripted import of the Linux DRM stack from upstream Linux into `drm/`, `i915/` and
`gpl/`. The tag is pinned in IMPORT_PIN.

| Path | Contents |
|------|----------|
| IMPORT_PIN | Upstream repository, tag, commit (and annotated tag object), Linux base, LINUXKPI_VERSION |
| manifest/drm.list, i915.list, gpl.list | Every upstream file under the `scope` directories, classified with a set and a reason (longest-prefix match). The list a file is in decides its tree. gpl.list holds the GPL-only files, which form a separate GPL-2.0 module (docs/08 section 5) |
| config/gen9.config | Kconfig-style CONFIG_* choices for the Gen9 target; generates drm/include/generated/autoconf.h |
| patches/series, patches/*.patch | Quilt-style patch series applied in order after extraction; every change to an imported file lives here |
| IMPORT_FILES | Generated: blob id, destination, set and detected licence of each imported pristine file |
| LICENSES/README.md | Licence summary, the GPL module boundary and open licence questions |
| gpl-policy.conf | GPL module policy (G1/G2 allow-lists, allowed GPL header debts) checked by `-c` |

Sets:
- Default: core, link, hdr, uapi, extra, aux.
- Optional, imported with `-s`: debugfs, fbdev, compat, capture, perf, selftest, gen12, hwmon, gvt, kunit,
  edidfw, auxdev, cec.
- exclude: never imported.

Workflow (see `scripts/import-linux.sh -h`):
```
scripts/import-linux.sh -m          # check the manifest covers every file in the pinned tag
scripts/import-linux.sh             # (re)import: drm/, i915/, gpl/, IMPORT_FILES
scripts/import-linux.sh -c          # verify committed trees == pin + manifest + patches + config
scripts/import-linux.sh -s gen12    # also import Gen12-only files (PXP, DP tunnel)
scripts/import-linux.sh -R DIR include/linux   # extract upstream reference files (not committed)
REF=/home/user/ref scripts/import-linux.sh     # reuse the clone at $REF/linux (default $HOME/ref/linux)
```
To re-pin: edit IMPORT_PIN, run `-m`, add every UNCLASSIFIED file to a manifest, then refresh the patches.
The script refuses to put a GPL-only file outside gpl/.

psi915's BSD replacements for GPL-only files live in drm_bsd/ (own MANIFEST), not here.
drm-kmod and OpenBSD remain reference trees only (scripts/fetch-refs.sh). Nothing is imported from them.
Imported files are never edited in place. No GPL-only Linux source may be placed in `include/` or `src/`.
