/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Internal, userland only: fake devices for the unit tests
 * (src/plat_userland_dev.c implements <linuxkpi/plat_dev.h> with them).
 *
 * A test fills a struct lkpi_fake_pci (identity, config space, BAR
 * buffers with made-up physical addresses) and registers it; the PCI code
 * then sees it as a dev_info_t. Device behaviour is simulated by test
 * code, typically a thread that watches the BAR memory, uses
 * lkpi_fake_dma_kva() to reach DMA buffers by bus address (bus addresses
 * come from a fake IOMMU, deliberately unrelated to kernel addresses) and
 * calls lkpi_fake_raise_irq().
 */
#ifndef _LINUXKPI_FAKEDEV_H_
#define	_LINUXKPI_FAKEDEV_H_

#include <linuxkpi/plat_dev.h>

#ifndef LINUXKPI_USERLAND
#error "fake devices exist only in the userland test environment"
#endif

#define	LKPI_FAKE_MAX_VEC	8

struct lkpi_fake_pci {
	struct lkpi_plat_pci_info info;
	u8		cfg[4096];
	void		*bar_mem[LKPI_PCI_NUM_BARS];
	const char	*name;
	unsigned int	intr_types;	/* LKPI_INTR_* supported */
	int		max_vec;
	/* runtime state (managed by the backend) */
	bool		(*vec_fn[LKPI_FAKE_MAX_VEC])(void *);
	void		*vec_arg[LKPI_FAKE_MAX_VEC];
	bool		vec_enabled[LKPI_FAKE_MAX_VEC];
	int		vec_alloc;
	int		claimed;	/* handler return count (claimed) */
	struct lkpi_fake_pci *next;
};

void lkpi_fake_pci_add(struct lkpi_fake_pci *f);
void lkpi_fake_pci_remove(struct lkpi_fake_pci *f);
/* Deliver vector vec as an interrupt; returns whether it was claimed. */
bool lkpi_fake_raise_irq(struct lkpi_fake_pci *f, int vec);
/* Kernel address behind a DMA bus range, or NULL if not mapped. */
void *lkpi_fake_dma_kva(u64 bus, size_t len);
/* Number of live DMA bindings (leak checks). */
int lkpi_fake_dma_live(void);
/* Directory request_firmware() reads from. */
extern const char *lkpi_fake_firmware_dir;
/* DMI strings by LKPI_DMI_* index (NULL = unknown). */
extern const char *lkpi_fake_dmi[32];
/* Last uevent delivered. */
extern char lkpi_fake_uevent_action[32];
extern char lkpi_fake_uevent_path[128];
extern char lkpi_fake_uevent_env0[128];
extern int lkpi_fake_uevent_count;

#endif /* _LINUXKPI_FAKEDEV_H_ */
