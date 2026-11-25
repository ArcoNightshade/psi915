# drmglue illumos header stubs

A copy of `tests/stubs/illumos` (as of the merge of 2026-11-25) extended with the
DDI, devmap, vnode, vfs, file, uio, poll, taskq and gfx_private declarations the
illumos side of drmglue uses. Used only by `make -C drmglue check-kmod` to
type-check `src/*_illumos.c` with kernel flags on a non-illumos host.

They are hand-written stand-ins, not copies of illumos headers, and are not
ABI-accurate: structure layouts (vnode_t, file_t, uio_t, vattr_t, taskq_ent_t,
pollhead_t, cb_ops) contain only the members drmglue touches, in an arbitrary
order. Signatures follow illumos-gate f71cb08e8c53 as read from the sources.
Passing `check-kmod` proves nothing about building or running on illumos.

Deliberate differences from `tests/stubs/illumos`:
- `dev_info_t` is `struct dev_info` (as in illumos `<sys/dditypes.h>`), not a pointer type.
- `<sys/types.h>` gains dev_t, major_t, minor_t, offset_t, u_offset_t, pfn_t,
  cred_t and friends; `<sys/file.h>` gains file_t and the descriptor functions.
