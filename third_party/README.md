# third_party

Inputs and records for the scripted import of the Linux DRM stack from drm-kmod into `drm/` and `i915/`.

| Path | Contents |
|------|----------|
| IMPORT_PIN | Upstream repository, branch, pinned commit, LINUXKPI_VERSION / Linux base |
| manifest/drm.list, manifest/i915.list | Every upstream file classified: set + reason (longest-prefix match). Sets: core, link, hdr, uapi, extra, aux, kpi-ref (default); debugfs, fbdev, compat, capture, perf, selftest, gen12, hwmon, lpe, auxdev (optional, `-s`); exclude |
| config/gen9.config | Kconfig-style CONFIG_* choices for the Gen9 target; generates drm/include/generated/autoconf.h |
| patches/series, patches/*.patch | Quilt-style patch series applied in order after extraction; all changes to imported files live here |
| IMPORT_FILES | Generated: blob id, destination, set and detected licence of each imported pristine file |
| LICENSES/README.md | Licence summary, GPL-only file list and replacement options |

Workflow (see `scripts/import-linux.sh -h`):
```
scripts/import-linux.sh -m          # manifest complete for the pin?
scripts/import-linux.sh             # (re)import: drm/, i915/, IMPORT_FILES
scripts/import-linux.sh -c          # verify committed trees == pin + manifest + patches + config
scripts/import-linux.sh -s gen12    # also import Gen12-only files (PXP, DP tunnel)
REF=/path/to/refs scripts/import-linux.sh   # reuse an existing drm-kmod clone at $REF/drm-kmod
```
Re-pinning: edit IMPORT_PIN, run `-m`, classify every UNCLASSIFIED file in the manifests, refresh patches.

Imported files are never edited in place. No GPL-only Linux source may be placed in `include/` or `src/`;
see docs/01-reference-sources.md, docs/06-risks-and-open-questions.md (R8) and LICENSES/README.md.
