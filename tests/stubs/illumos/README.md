# illumos kernel header stubs

Minimal, hand-written stand-ins for a few illumos kernel headers, used ONLY to
syntax- and compile-check the kernel-side sources (`src/`) and the kernel
configuration of `include/linux/*.h` on a Linux host (`make check-kmod`).

They declare just enough (types, `modlinkage`, `cmn_err`, `ASSERT`) for the
compiler to type-check our code. They are not copies of illumos headers, are
not ABI-accurate, and an object compiled against them is not a usable kernel
module. Passing `make check-kmod` does NOT mean the module builds or loads on
illumos; only a real build on illumos (`gmake -C build`) shows that.

Where an illumos header is known to define something our headers override
(e.g. `min`/`max` in `<sys/ddi.h>`, `ARRAY_SIZE` in `<sys/sysmacros.h>`,
`__packed` in `<sys/ccompile.h>`), the stub defines it the same way so the
override path is exercised.
