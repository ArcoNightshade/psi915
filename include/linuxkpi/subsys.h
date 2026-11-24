/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Start-up and tear-down of KPI subsystems that own threads or global
 * state. The kernel module calls these from _init/_fini
 * (src/linuxkpi_mod.c); userland test programs start them lazily or call
 * them directly.
 */
#ifndef _LINUXKPI_SUBSYS_H_
#define	_LINUXKPI_SUBSYS_H_

/* RCU callback thread (src/linux_rcu.c). 0 on success. */
int lkpi_rcu_init(void);
/* Waits for pending call_rcu() callbacks, then stops the thread. */
void lkpi_rcu_fini(void);

#endif /* _LINUXKPI_SUBSYS_H_ */
