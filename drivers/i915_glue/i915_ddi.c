/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos attach/detach glue of the i915 driver (kernel only).
 *
 * The Linux driver registers its pci_driver from module_init(i915_init);
 * LKPI_PCI_DRIVER_MODULE (<linuxkpi/ddi.h>) supplies modlinkage, dev_ops
 * and _init/_info/_fini around it. This file adds what a DRM driver needs
 * beyond the KPI's generic PCI glue (docs/10 R11, docs/11 sections 4-5):
 *
 *  - cb_ops: drmglue_cb_ops (clone opens, ioctl translation, poll, devmap),
 *    wrapped so that one extra minor, the console minor "text-0", is served
 *    by gfx_private's framebuffer console code exactly like vgatext(4D).
 *  - minor nodes: "text-0" (DDI_NT_DISPLAY) first, because consconfig
 *    resolves the console framebuffer path from plat_fbpath() without a
 *    minor name, which ddi_pathname_to_dev_t() maps to the FIRST minor node
 *    of the display dip; then card0 / renderD128 (DDI_NT_DISPLAY_DRM) via
 *    drmglue_attach_node() once drm_dev_register() has assigned them.
 *  - getinfo: drmglue_getinfo (its minor layout is clone << 8 | node, which
 *    the KPI's lkpi_ddi_getinfo would misread as an instance number).
 *  - driver.conf properties -> i915 module parameters, before probe.
 *  - DDI_SUSPEND / DDI_RESUME -> dev_pm_ops (lkpi_pci_detach/attach) plus
 *    gfx_private's console state.
 *  - console fallback: if the Linux probe fails, stay attached with the
 *    console minor only, so the boot console survives (vgatext cannot bind
 *    the device while the i915 alias does).
 *  - a CB_CL_PANIC callback that asks the Linux side to put the console
 *    framebuffer back on screen.
 *  - console safety (docs/13 G1, docs/11 section 5.2): gfx_private writes
 *    the console through a fixed mapping of the firmware framebuffer's
 *    aperture address, i.e. through whatever the GGTT maps there. That is
 *    safe only while i915 keeps the initial framebuffer object bound at its
 *    firmware GGTT offset. The glue keeps gfx_private in KD_GRAPHICS (no
 *    aperture writes, output to its shadow only) during probe, lets it
 *    back to KD_TEXT only if the probed device still scans out an initial
 *    framebuffer, and latches "console framebuffer lost" for good before
 *    the first open of a primary (card) node, i.e. before any process can
 *    become DRM master and modeset. Once lost: KD_TEXT is refused, resume
 *    does not copy the shadow back, console mmap is refused. Until the
 *    handover patches (R-P1..R-P3) exist, the console after the first DRM
 *    master is the serial console.
 *
 * Constraints: one i915 device per system (drmglue hosts anonymous files on
 * the first DRM dip; KBL-R laptops have one GPU). The dev_ops fields
 * attach/detach/getinfo are replaced in _init before mod_install(); a
 * LKPI_PCI_DRIVER_MODULE variant taking them as arguments is requested in
 * docs/11 (R-K3).
 *
 * UNTESTED ON ILLUMOS: compile-checked against stub headers at most.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddidevmap.h>
#include <sys/callb.h>
#include <sys/gfx_private.h>
#include <sys/visual_io.h>
#include <sys/kd.h>
#include <linuxkpi/ddi.h>

#include <linuxkpi/platform.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/glue.h>
#include <drmglue/illumos.h>

#include "i915_glue.h"

/* Console minor: node 0xfe (LKPI_NODE_MAX, never a Linux DRM minor), clone 0. */
#define	PSI915_NODE_CONSOLE	LKPI_NODE_MAX
#define	PSI915_CONSOLE_NAME	"text-0"

/*
 * Tunables (/etc/system, "set i915:<name> = <value>").
 *   psi915_nomodeset          1: video_firmware_drivers_only(); i915_init
 *                             then registers no pci_driver and every
 *                             attach is console-only.
 *   psi915_console_fallback   1: attach console-only when probe fails.
 *   psi915_enable_console     0: no console minor and no gfx_private use.
 */
int psi915_nomodeset = 0;
int psi915_console_fallback = 1;
int psi915_enable_console = 1;

struct psi915_softc {
	dev_info_t		*dip;
	void			*pdev;		/* struct pci_dev *, NULL if console-only */
	gfxp_fb_softc_ptr_t	gfxp;		/* NULL without console */
	boolean_t		cons_minor;
	/*
	 * Console minor open (tem keeps it open). Informational: the
	 * framework does not call detach(9E) while any minor of the dip is
	 * open (devi_ref), and open/close of one non-clone minor are
	 * serialised by specfs, so no lock. (Linux's mutex_init() shadows
	 * the illumos one in a file that includes KPI headers, so kmutex_t
	 * is avoided here altogether.)
	 */
	volatile boolean_t	cons_open;
	/*
	 * The aperture range gfx_private writes to may no longer be the
	 * firmware framebuffer (see the comment at the top). Set once, never
	 * cleared while attached. Read by the gfx_private setmode hook.
	 */
	volatile boolean_t	fb_lost;
	volatile boolean_t	probing;	/* Linux probe in progress */
	boolean_t		was_text;	/* gfxp mode before probe */
	int			nnodes;
	struct psi915_node	nodes[PSI915_MAX_NODES];
	callb_id_t		panic_cb;
};

static struct psi915_softc *psi915_sc;		/* the single instance */
static struct cb_ops psi915_cb_ops;		/* drmglue_cb_ops + console */

static int psi915_i915_init(void);
static void psi915_i915_exit(void);

/*
 * cb_ops is passed as NULL and set in psi915_i915_init(): the macro's
 * "(_cbops) != NULL ? (_cbops) : &lkpi_ddi_null_cb_ops" is not a constant
 * expression for gcc when _cbops is an address (R-K3).
 */
LKPI_PCI_DRIVER_MODULE(i915, "i915 (psi915, Linux 6.14 DRM)",
    NULL, psi915_i915_init, psi915_i915_exit)

/*
 * cb_ops wrappers: the console minor goes to gfx_private, everything else
 * to drmglue.
 */
static boolean_t
psi915_is_console(dev_t dev)
{
	minor_t m = getminor(dev);

	return (lkpi_minor_node((uint32_t)m) == PSI915_NODE_CONSOLE &&
	    lkpi_minor_clone((uint32_t)m) == 0);
}

/*
 * KDGETMODE / KDSETMODE on gfx_private from the kernel (FKIOCTL: data is a
 * kernel address for KDGETMODE, the mode value for KDSETMODE).
 */
static int
psi915_cons_kd(struct psi915_softc *sc, int cmd, intptr_t data)
{
	dev_t dev = makedevice(ddi_driver_major(sc->dip),
	    (minor_t)lkpi_minor_make(PSI915_NODE_CONSOLE, 0));
	int rval = 0;

	return (gfxp_fb_ioctl(dev, cmd, data, FKIOCTL, kcred, &rval,
	    sc->gfxp));
}

/*
 * Stop gfx_private writing through its aperture mapping: in KD_GRAPHICS
 * every bitmap console routine (ioctl and polled paths) updates only the
 * shadow copy (gfxp_bitmap.c: all fb writes are under mode == KD_TEXT).
 */
static void
psi915_console_quiesce(struct psi915_softc *sc)
{
	int mode = -1;

	if (sc->gfxp == NULL)
		return;
	if (psi915_cons_kd(sc, KDGETMODE, (intptr_t)&mode) == 0)
		sc->was_text = (mode == KD_TEXT);
	(void) psi915_cons_kd(sc, KDSETMODE, KD_GRAPHICS);
}

/* Undo psi915_console_quiesce() if the framebuffer is still the console's. */
static void
psi915_console_unquiesce(struct psi915_softc *sc)
{
	if (sc->gfxp == NULL || sc->fb_lost || !sc->was_text)
		return;
	/* bitmap_kdsetmode(KD_TEXT) copies the shadow back on screen. */
	(void) psi915_cons_kd(sc, KDSETMODE, KD_TEXT);
}

/*
 * Latch "the aperture range is no longer the console framebuffer" and
 * silence gfx_private for good (docs/11 section 5.2). Called before the
 * first open of a primary node: any opener may become DRM master and
 * replace the initial framebuffer, after which i915 releases its GGTT range
 * for reuse by rings, contexts and buffers (docs/13 G1).
 */
static void
psi915_console_lose(struct psi915_softc *sc, const char *why)
{
	if (sc->gfxp == NULL || sc->fb_lost)
		return;
	sc->fb_lost = B_TRUE;		/* the setmode hook refuses KD_TEXT */
	(void) psi915_cons_kd(sc, KDSETMODE, KD_GRAPHICS);
	cmn_err(CE_NOTE, "i915: framebuffer console stopped (%s); console "
	    "output continues on other console devices (serial) until reboot",
	    why);
}

static int
psi915_open(dev_t *devp, int flag, int otyp, cred_t *cr)
{
	struct psi915_softc *sc = psi915_sc;
	int r;

	if (!psi915_is_console(*devp)) {
		/* Primary node (card0..63): a possible DRM master. */
		if (sc != NULL && sc->pdev != NULL &&
		    lkpi_minor_node((uint32_t)getminor(*devp)) < 64)
			psi915_console_lose(sc, "DRM primary node opened");
		return (drmglue_cb_ops.cb_open(devp, flag, otyp, cr));
	}
	if (sc == NULL || sc->gfxp == NULL)
		return (ENXIO);
	r = gfxp_fb_open(devp, flag, otyp, cr, sc->gfxp);
	if (r == 0)
		sc->cons_open = B_TRUE;
	return (r);
}

static int
psi915_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	struct psi915_softc *sc = psi915_sc;

	if (!psi915_is_console(dev))
		return (drmglue_cb_ops.cb_close(dev, flag, otyp, cr));
	if (sc == NULL || sc->gfxp == NULL)
		return (ENXIO);
	/* close(9E) runs once, for the last close of the non-clone minor. */
	sc->cons_open = B_FALSE;
	return (gfxp_fb_close(dev, flag, otyp, cr, sc->gfxp));
}

static int
psi915_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr,
    int *rvalp)
{
	struct psi915_softc *sc = psi915_sc;

	if (!psi915_is_console(dev))
		return (drmglue_cb_ops.cb_ioctl(dev, cmd, arg, mode, cr, rvalp));
	if (sc == NULL || sc->gfxp == NULL)
		return (ENXIO);
	/*
	 * VIS_* (tem), KDSETMODE/KDGETMODE (wscons on behalf of Xorg VT
	 * switches) and FBIO* go to gfx_private, as for vgatext. KDSETMODE
	 * reaches psi915_blt_setmode() through gfxp_bm_register_fbops().
	 */
	return (gfxp_fb_ioctl(dev, cmd, arg, mode, cr, rvalp, sc->gfxp));
}

static int
psi915_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
    size_t *maplen, uint_t model)
{
	struct psi915_softc *sc = psi915_sc;

	if (!psi915_is_console(dev))
		return (drmglue_cb_ops.cb_devmap(dev, dhp, off, len, maplen,
		    model));
	if (sc == NULL || sc->gfxp == NULL)
		return (ENXIO);
	/* See psi915_segmap(). */
	if (sc->pdev != NULL || sc->fb_lost)
		return (ENODEV);
	return (gfxp_fb_devmap(dev, dhp, off, len, maplen, model, sc->gfxp));
}

/*
 * mmap(2) of the console minor goes through segmap, as for vgatext
 * (ddi_devmap_segmap -> devmap -> gfxp_fb_devmap); DRM nodes keep
 * drmglue's segmap. The console mapping is the raw aperture window at the
 * firmware framebuffer address, so it is only offered while nothing else
 * can own that GGTT range: console-only attach (no DRM device). With the
 * DRM device attached, a mapping made early would outlive the console's
 * ownership of the range (docs/13 G2), so it is refused (ENODEV).
 */
static int
psi915_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
    uint_t prot, uint_t maxprot, uint_t flags, cred_t *cr)
{
	struct psi915_softc *sc = psi915_sc;

	if (!psi915_is_console(dev))
		return (drmglue_cb_ops.cb_segmap(dev, off, as, addrp, len, prot,
		    maxprot, flags, cr));
	if (sc == NULL || sc->gfxp == NULL)
		return (ENXIO);
	if (sc->pdev != NULL || sc->fb_lost)
		return (ENODEV);
	return (ddi_devmap_segmap(dev, off, as, addrp, len, prot, maxprot,
	    flags, cr));
}

static int
psi915_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	struct psi915_softc *sc = psi915_sc;
	dev_t dev = (dev_t)(uintptr_t)arg;

	if (psi915_is_console(dev) || drmglue_getinfo(dip, cmd, arg,
	    result) != DDI_SUCCESS) {
		/* Console minor, or a DRM node that is not registered yet. */
		if (sc == NULL)
			return (DDI_FAILURE);
		switch (cmd) {
		case DDI_INFO_DEVT2DEVINFO:
			*result = sc->dip;
			return (DDI_SUCCESS);
		case DDI_INFO_DEVT2INSTANCE:
			*result = (void *)(uintptr_t)ddi_get_instance(sc->dip);
			return (DDI_SUCCESS);
		default:
			return (DDI_FAILURE);
		}
	}
	return (DDI_SUCCESS);
}

/*
 * gfx_private callback for KDSETMODE on the console minor. No argument
 * pointer is passed (struct gfxp_blt_ops), hence the single-instance
 * softc. mode is KD_TEXT or KD_GRAPHICS.
 *
 * A non-zero return makes bitmap_kdsetmode() fail before it copies the
 * shadow into the aperture, and gfx_private stays in KD_GRAPHICS (output
 * to the shadow only). KD_TEXT is refused while the Linux probe runs and
 * after the console framebuffer is lost (docs/13 G1). tem ignores the
 * KDSETMODE result, so it keeps drawing, into the shadow.
 */
static int
psi915_blt_setmode(int mode)
{
	struct psi915_softc *sc = psi915_sc;

	if (sc == NULL || mode != KD_TEXT)
		return (0);
	if (sc->probing || sc->fb_lost)
		return (EIO);
	if (sc->pdev == NULL)
		return (0);
	return (psi915_drm_console_setmode(sc->pdev, 1));
}

static struct gfxp_blt_ops psi915_blt_ops = {
	.blt = NULL,
	.copy = NULL,
	.clear = NULL,
	.setmode = psi915_blt_setmode
};

/*
 * CB_CL_PANIC: called by panic() with panicstr set and the other CPUs
 * stopped, before the panic banner is printed with polled console I/O.
 */
static boolean_t
psi915_panic_callb(void *arg, int code)
{
	struct psi915_softc *sc = arg;

	if (sc != NULL && sc->pdev != NULL)
		psi915_drm_console_panic(sc->pdev);
	return (B_TRUE);
}

/*
 * Console minor and gfx_private state. Must run before any other minor
 * node is created (first-minor rule, see the comment at the top).
 */
static void
psi915_console_attach(struct psi915_softc *sc)
{
	if (!psi915_enable_console)
		return;
	sc->gfxp = gfxp_fb_softc_alloc();
	if (sc->gfxp == NULL)
		return;
	if (gfxp_fb_attach(sc->dip, DDI_ATTACH, sc->gfxp) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "!i915: no firmware framebuffer console");
		gfxp_fb_softc_free(sc->gfxp);
		sc->gfxp = NULL;
		return;
	}
	if (ddi_create_minor_node(sc->dip, PSI915_CONSOLE_NAME, S_IFCHR,
	    (minor_t)lkpi_minor_make(PSI915_NODE_CONSOLE, 0), DDI_NT_DISPLAY,
	    0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "i915: cannot create console minor");
		(void) gfxp_fb_detach(sc->dip, DDI_DETACH, sc->gfxp);
		gfxp_fb_softc_free(sc->gfxp);
		sc->gfxp = NULL;
		return;
	}
	sc->cons_minor = B_TRUE;
}

static void
psi915_console_detach(struct psi915_softc *sc)
{
	if (sc->cons_minor) {
		ddi_remove_minor_node(sc->dip, PSI915_CONSOLE_NAME);
		sc->cons_minor = B_FALSE;
	}
	if (sc->gfxp != NULL) {
		(void) gfxp_fb_detach(sc->dip, DDI_DETACH, sc->gfxp);
		gfxp_fb_softc_free(sc->gfxp);
		sc->gfxp = NULL;
	}
}

static void
psi915_nodes_detach(struct psi915_softc *sc)
{
	while (sc->nnodes > 0) {
		struct psi915_node *n = &sc->nodes[--sc->nnodes];

		(void) drmglue_detach_node(sc->dip, n->node, n->name);
	}
}

static int
psi915_nodes_attach(struct psi915_softc *sc)
{
	struct psi915_node nodes[PSI915_MAX_NODES];
	int i, n, r;

	n = psi915_drm_nodes(sc->pdev, nodes, PSI915_MAX_NODES);
	if (n < 0)
		return (lkpi_errno_to_illumos(n));
	for (i = 0; i < n; i++) {
		r = drmglue_attach_node(sc->dip, nodes[i].node, nodes[i].name,
		    nodes[i].fops, psi915_drm_kcmd);
		if (r != 0) {
			cmn_err(CE_WARN, "i915: minor %s: error %d",
			    nodes[i].name, r);
			psi915_nodes_detach(sc);
			return (r);
		}
		sc->nodes[sc->nnodes++] = nodes[i];
	}
	return (0);
}

static int
psi915_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct psi915_softc *sc;
	const struct lkpi_module_param *const *params;
	size_t nparams;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		sc = psi915_sc;
		if (sc == NULL || sc->dip != dip)
			return (DDI_FAILURE);
		/*
		 * Display hardware first, then the console redraw.
		 * bitmap_resume() copies the shadow into the aperture whatever
		 * the KD mode, so it runs only while the aperture range is
		 * still the console framebuffer (docs/13 G1).
		 */
		if (sc->pdev != NULL &&
		    lkpi_pci_attach(dip, DDI_RESUME) != DDI_SUCCESS)
			return (DDI_FAILURE);
		if (sc->gfxp != NULL && !sc->fb_lost)
			(void) gfxp_fb_attach(dip, DDI_RESUME, sc->gfxp);
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (psi915_sc != NULL) {
		cmn_err(CE_WARN, "i915: instance %d not attached: psi915 "
		    "supports one i915 device", ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
	sc = kmem_zalloc(sizeof (*sc), KM_SLEEP);
	sc->dip = dip;
	psi915_sc = sc;

	psi915_console_attach(sc);
	if (sc->gfxp != NULL)
		gfxp_bm_register_fbops(sc->gfxp, &psi915_blt_ops);

	/* i915.conf properties named like Linux parameters, before probe. */
	params = psi915_i915_params(&nparams);
	(void) lkpi_module_params_apply(dip, params, nparams);

	/*
	 * Probe reprograms the GGTT; until it is known whether i915 kept the
	 * firmware framebuffer bound, gfx_private draws into its shadow only.
	 */
	sc->probing = B_TRUE;
	psi915_console_quiesce(sc);
	if (lkpi_pci_attach(dip, DDI_ATTACH) == DDI_SUCCESS) {
		sc->pdev = ddi_get_driver_private(dip);
		if (psi915_nodes_attach(sc) != 0) {
			(void) lkpi_pci_detach(dip, DDI_DETACH);
			sc->pdev = NULL;
		}
	}
	sc->probing = B_FALSE;

	if (sc->pdev != NULL) {
		/*
		 * Before any process can open a DRM node, only the initial
		 * plane can hold a framebuffer: if i915 did not take over the
		 * firmware framebuffer (intel_plane_initial.c), its GGTT range
		 * may already belong to other objects.
		 */
		if (psi915_drm_initial_fb_present(sc->pdev))
			psi915_console_unquiesce(sc);
		else
			psi915_console_lose(sc, "i915 did not keep the "
			    "firmware framebuffer");
	} else {
		if (!psi915_console_fallback || sc->gfxp == NULL)
			goto fail;
		/*
		 * docs/12 A1: the failed probe may have cleared the GGTT or
		 * turned the plane off, so the console can be blank. It is
		 * still safe: with the DRM device gone nothing else owns the
		 * aperture range. Recovery: set i915:psi915_nomodeset = 1,
		 * which attaches console-only without touching the GPU.
		 */
		cmn_err(CE_WARN, "i915: DRM probe failed; attached as console "
		    "framebuffer only. The screen may stay blank if probe got "
		    "past GGTT/display init; boot with "
		    "\"set i915:psi915_nomodeset = 1\" in /etc/system to keep "
		    "the firmware console untouched");
		psi915_console_unquiesce(sc);
		ddi_report_dev(dip);
	}
	sc->panic_cb = callb_add(psi915_panic_callb, sc, CB_CL_PANIC,
	    "i915_panic");
	return (DDI_SUCCESS);
fail:
	psi915_console_detach(sc);
	psi915_sc = NULL;
	kmem_free(sc, sizeof (*sc));
	return (DDI_FAILURE);
}

static int
psi915_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct psi915_softc *sc = psi915_sc;
	int i;

	if (sc == NULL || sc->dip != dip)
		return (DDI_FAILURE);

	switch (cmd) {
	case DDI_SUSPEND:
		/*
		 * Linux dev_pm_ops first (saves display state), then gfxp.
		 * With the console framebuffer lost, gfxp is left alone in
		 * both directions (its resume would copy the shadow into the
		 * aperture, docs/13 G1).
		 */
		if (sc->pdev != NULL &&
		    lkpi_pci_detach(dip, DDI_SUSPEND) != DDI_SUCCESS)
			return (DDI_FAILURE);
		if (sc->gfxp != NULL && !sc->fb_lost &&
		    gfxp_fb_detach(dip, DDI_SUSPEND, sc->gfxp) != DDI_SUCCESS) {
			if (sc->pdev != NULL)
				(void) lkpi_pci_attach(dip, DDI_RESUME);
			return (DDI_FAILURE);
		}
		return (DDI_SUCCESS);
	case DDI_DETACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	/*
	 * Busy: the console (tem holds it open) or an open DRM file. The
	 * framework already refuses detach while a minor is open; clone
	 * files and anonymous-file mmaps are checked through drmglue.
	 */
	if (sc->cons_open)
		return (DDI_FAILURE);
	for (i = 0; i < sc->nnodes; i++) {
		if (lkpi_node_open_count(sc->nodes[i].node) != 0)
			return (DDI_FAILURE);
	}

	/*
	 * The DRM nodes go first so that nothing can open the device while
	 * i915_pci_remove() runs. If the remove fails (it cannot today, and
	 * detach is unreachable while the KPI holds the dip, docs/13 P1), the
	 * nodes are registered again and the driver stays fully attached
	 * (docs/13 A2).
	 */
	psi915_nodes_detach(sc);
	if (sc->pdev != NULL) {
		if (lkpi_pci_detach(dip, DDI_DETACH) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "i915: Linux remove failed; detach "
			    "refused");
			if (psi915_nodes_attach(sc) != 0)
				cmn_err(CE_WARN, "i915: DRM minor nodes could "
				    "not be restored");
			return (DDI_FAILURE);
		}
		sc->pdev = NULL;
	}
	if (sc->panic_cb != NULL)
		(void) callb_delete(sc->panic_cb);
	psi915_console_detach(sc);
	psi915_sc = NULL;
	kmem_free(sc, sizeof (*sc));
	return (DDI_SUCCESS);
}

/*
 * Module init, called by LKPI_PCI_DRIVER_MODULE's _init before
 * mod_install(): finish the dev_ops and cb_ops, then run the Linux module
 * init (which registers i915_pci_driver). drmglue itself is initialised by
 * the module that links it (linuxkpi by default, DRMGLUE_IN), not here.
 */
static int
psi915_i915_init(void)
{
	psi915_cb_ops = drmglue_cb_ops;
	psi915_cb_ops.cb_open = psi915_open;
	psi915_cb_ops.cb_close = psi915_close;
	psi915_cb_ops.cb_ioctl = psi915_ioctl;
	psi915_cb_ops.cb_devmap = psi915_devmap;
	psi915_cb_ops.cb_segmap = psi915_segmap;

	i915_lkpi_dev_ops.devo_cb_ops = &psi915_cb_ops;
	i915_lkpi_dev_ops.devo_getinfo = psi915_getinfo;
	i915_lkpi_dev_ops.devo_attach = psi915_attach;
	i915_lkpi_dev_ops.devo_detach = psi915_detach;

	psi915_set_nomodeset(psi915_nomodeset);
	return (lkpi_initcall_i915_init());
}

static void
psi915_i915_exit(void)
{
	lkpi_exitcall_i915_exit();
}
