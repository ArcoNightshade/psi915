/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos module glue for toy_edu (kernel only). Install with
 *   add_drv -i '"pci1234,11e8"' lkpi_toy
 * after copying build/lkpi_toy/obj64/lkpi_toy to /usr/kernel/drv/amd64.
 * UNTESTED ON ILLUMOS.
 */

#include <linuxkpi/ddi.h>

LKPI_PCI_DRIVER_MODULE(lkpi_toy, "LinuxKPI toy (QEMU edu) driver", NULL,
    lkpi_initcall_toy_init, lkpi_exitcall_toy_exit)
