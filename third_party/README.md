# third_party

Placeholder. Upstream imports (drm-kmod DRM core, i915, linux-firmware metadata)
will live here from P3 onwards, each in its own directory with a `PIN` file that
records the upstream repository, tag and commit, and a `patches/` series.

Nothing is imported in P0. No GPL-only Linux source may be placed in `include/`
or `src/`; see docs/01-reference-sources.md and docs/06-risks-and-open-questions.md (R8).
