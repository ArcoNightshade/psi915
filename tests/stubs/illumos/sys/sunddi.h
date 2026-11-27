/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_SUNDDI_H
#define	_STUB_SYS_SUNDDI_H
#include <sys/types.h>
extern size_t strnlen(const char *, size_t);
extern int ddi_copyin(const void *, void *, size_t, int);
extern int ddi_copyout(const void *, void *, size_t, int);
extern pid_t ddi_get_pid(void);
typedef struct __dev_info *dev_info_t;
#define	DDI_DEV_T_ANY		((dev_t)-2)
#define	DDI_DEV_T_NONE		((dev_t)-1)
#define	DDI_PROP_DONTPASS	0x0001
#define	DDI_PROP_CANSLEEP	0x0000
#define	DDI_PROP_SUCCESS	0
#define	DDI_SUCCESS		0
#define	DDI_FAILURE		(-1)
extern int ddi_prop_exists(dev_t, dev_info_t *, uint_t, char *);
extern int ddi_prop_get_int(dev_t, dev_info_t *, uint_t, char *, int);
extern int ddi_prop_lookup_string(dev_t, dev_info_t *, uint_t, char *,
    char **);
extern void ddi_prop_free(void *);
#include <sys/dditypes.h>
#define	DDI_WALK_CONTINUE	0
#define	DDI_WALK_PRUNECHILD	(-1)
#define	DDI_WALK_TERMINATE	(-3)
extern int ddi_prop_lookup_int_array(dev_t, dev_info_t *, uint_t, char *,
    int **, uint_t *);
extern int pci_config_setup(dev_info_t *, ddi_acc_handle_t *);
extern void pci_config_teardown(ddi_acc_handle_t *);
extern uint8_t pci_config_get8(ddi_acc_handle_t, off_t);
extern uint16_t pci_config_get16(ddi_acc_handle_t, off_t);
extern uint32_t pci_config_get32(ddi_acc_handle_t, off_t);
extern void pci_config_put8(ddi_acc_handle_t, off_t, uint8_t);
extern void pci_config_put16(ddi_acc_handle_t, off_t, uint16_t);
extern void pci_config_put32(ddi_acc_handle_t, off_t, uint32_t);
extern void ddi_walk_devs(dev_info_t *, int (*)(dev_info_t *, void *), void *);
extern dev_info_t *ddi_root_node(void);
extern dev_info_t *ddi_get_parent(dev_info_t *);
extern dev_info_t *ddi_get_child(dev_info_t *);
extern dev_info_t *ddi_get_next_sibling(dev_info_t *);
extern char *ddi_node_name(dev_info_t *);
extern int ddi_regs_map_setup(dev_info_t *, uint_t, caddr_t *, offset_t,
    offset_t, ddi_device_acc_attr_t *, ddi_acc_handle_t *);
extern void ddi_regs_map_free(ddi_acc_handle_t *);
#endif
