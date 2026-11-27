/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Interrupts, tasklets, irq_work and PCI interrupt vectors (see
 * <linux/interrupt.h>, <linux/pci.h>). Clean-room.
 *
 * IRQ numbers index lkpi_irq_table (1..LKPI_IRQ_MAX; 0 is "no IRQ").
 * Each entry names a backend vector (lkpi_plat_intr, index) and, once
 * requested, the Linux handler. The illumos handler wrapper counts
 * itself in `active` around the Linux handler so synchronize_irq() can
 * wait for running handlers without taking a lock the handler takes.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linuxkpi/plat_dev.h>

#define	LKPI_IRQ_MAX	256

struct lkpi_irq_desc {
	bool			used;
	struct lkpi_plat_intr	*ip;
	int			idx;
	irq_handler_t		handler;
	irq_handler_t		thread_fn;
	void			*dev_id;
	const char		*name;
	unsigned long		flags;
	int			depth;		/* disable_irq() nesting */
	atomic_t		active;		/* handlers running */
	struct work_struct	thread_work;
	unsigned int		irq;
};

struct lkpi_pci_irqs {
	struct lkpi_plat_intr	*ip;
	unsigned int		type;
	int			count;
	bool			implicit;	/* default INTx (docs/13 P2) */
	unsigned int		irq[];		/* KPI irq numbers */
};

static struct lkpi_irq_desc lkpi_irq_table[LKPI_IRQ_MAX + 1];
static DEFINE_MUTEX(lkpi_irq_lock);

static struct lkpi_irq_desc *
lkpi_irq_desc(unsigned int irq)
{
	if (irq == 0 || irq > LKPI_IRQ_MAX || !lkpi_irq_table[irq].used)
		return (NULL);
	return (&lkpi_irq_table[irq]);
}

static void
lkpi_irq_thread_work(struct work_struct *w)
{
	struct lkpi_irq_desc *d = container_of(w, struct lkpi_irq_desc,
	    thread_work);
	irq_handler_t fn = READ_ONCE(d->thread_fn);

	if (fn != NULL)
		(void) fn((int)d->irq, d->dev_id);
}

/* Backend handler: runs in illumos interrupt context. */
static bool
lkpi_irq_dispatch(void *arg)
{
	struct lkpi_irq_desc *d = arg;
	irqreturn_t r = IRQ_NONE;
	irq_handler_t h;

	atomic_inc(&d->active);
	h = READ_ONCE(d->handler);
	if (READ_ONCE(d->depth) == 0) {
		if (h != NULL)
			r = h((int)d->irq, d->dev_id);
		else if (READ_ONCE(d->thread_fn) != NULL)
			r = IRQ_WAKE_THREAD;	/* default primary handler */
		if (r == IRQ_WAKE_THREAD && READ_ONCE(d->thread_fn) != NULL)
			(void) queue_work(system_highpri_wq, &d->thread_work);
	}
	atomic_dec(&d->active);
	return (r != IRQ_NONE);
}

/* Allocate KPI irq numbers for count vectors of ip. */
static int
lkpi_irq_alloc_descs(struct lkpi_plat_intr *ip, int count, unsigned int *out)
{
	unsigned int irq;
	int n = 0;

	mutex_lock(&lkpi_irq_lock);
	for (irq = 1; irq <= LKPI_IRQ_MAX && n < count; irq++) {
		struct lkpi_irq_desc *d = &lkpi_irq_table[irq];

		if (d->used)
			continue;
		memset(d, 0, sizeof (*d));
		d->used = true;
		d->ip = ip;
		d->idx = n;
		d->irq = irq;
		atomic_set(&d->active, 0);
		INIT_WORK(&d->thread_work, lkpi_irq_thread_work);
		out[n++] = irq;
	}
	if (n < count) {
		while (n > 0)
			lkpi_irq_table[out[--n]].used = false;
		mutex_unlock(&lkpi_irq_lock);
		return (-ENOSPC);
	}
	mutex_unlock(&lkpi_irq_lock);
	return (0);
}

int
request_threaded_irq(unsigned int irq, irq_handler_t handler,
    irq_handler_t thread_fn, unsigned long flags, const char *name,
    void *dev_id)
{
	struct lkpi_irq_desc *d;
	int r;

	might_sleep();
	mutex_lock(&lkpi_irq_lock);
	d = lkpi_irq_desc(irq);
	if (d == NULL) {
		mutex_unlock(&lkpi_irq_lock);
		return (-EINVAL);
	}
	if (d->handler != NULL || d->thread_fn != NULL) {
		mutex_unlock(&lkpi_irq_lock);
		return (-EBUSY);
	}
	if (lkpi_plat_intr_hilevel(d->ip, d->idx)) {
		mutex_unlock(&lkpi_irq_lock);
		pr_err("linuxkpi: irq %u (%s) is above LOCK_LEVEL; "
		    "not supported\n", irq, name);
		return (-EINVAL);
	}
	d->thread_fn = thread_fn;
	d->dev_id = dev_id;
	d->name = name;
	d->flags = flags;
	d->depth = (flags & IRQF_NO_AUTOEN) ? 1 : 0;
	WRITE_ONCE(d->handler, handler);
	r = lkpi_plat_intr_add(d->ip, d->idx, lkpi_irq_dispatch, d);
	if (r == 0 && d->depth == 0)
		r = lkpi_plat_intr_enable(d->ip, d->idx);
	if (r != 0) {
		WRITE_ONCE(d->handler, NULL);
		d->thread_fn = NULL;
	}
	mutex_unlock(&lkpi_irq_lock);
	return (r);
}

void
synchronize_irq(unsigned int irq)
{
	struct lkpi_irq_desc *d = lkpi_irq_desc(irq);

	if (d == NULL)
		return;
	while (atomic_read(&d->active) != 0)
		lkpi_plat_sleep_ns(10000);
	(void) flush_work(&d->thread_work);
}

const void *
free_irq(unsigned int irq, void *dev_id)
{
	struct lkpi_irq_desc *d;
	const char *name;

	might_sleep();
	mutex_lock(&lkpi_irq_lock);
	d = lkpi_irq_desc(irq);
	if (d == NULL || d->dev_id != dev_id ||
	    (d->handler == NULL && d->thread_fn == NULL)) {
		mutex_unlock(&lkpi_irq_lock);
		WARN(1, "linuxkpi: free_irq(%u) not requested\n", irq);
		return (NULL);
	}
	if (d->depth == 0)
		(void) lkpi_plat_intr_disable(d->ip, d->idx);
	lkpi_plat_intr_remove(d->ip, d->idx);
	WRITE_ONCE(d->handler, NULL);
	mutex_unlock(&lkpi_irq_lock);
	synchronize_irq(irq);
	(void) cancel_work_sync(&d->thread_work);
	d->thread_fn = NULL;
	name = d->name;
	d->name = NULL;
	d->dev_id = NULL;
	return (name);
}

void
disable_irq_nosync(unsigned int irq)
{
	struct lkpi_irq_desc *d = lkpi_irq_desc(irq);

	if (d == NULL)
		return;
	mutex_lock(&lkpi_irq_lock);
	if (d->depth++ == 0 && (d->handler != NULL || d->thread_fn != NULL))
		(void) lkpi_plat_intr_disable(d->ip, d->idx);
	mutex_unlock(&lkpi_irq_lock);
}

void
disable_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	synchronize_irq(irq);
}

void
enable_irq(unsigned int irq)
{
	struct lkpi_irq_desc *d = lkpi_irq_desc(irq);

	if (d == NULL)
		return;
	mutex_lock(&lkpi_irq_lock);
	if (d->depth == 0)
		WARN(1, "linuxkpi: unbalanced enable_irq(%u)\n", irq);
	else if (--d->depth == 0 &&
	    (d->handler != NULL || d->thread_fn != NULL))
		(void) lkpi_plat_intr_enable(d->ip, d->idx);
	mutex_unlock(&lkpi_irq_lock);
}

void *
lkpi_irq_iblock(unsigned int irq)
{
	struct lkpi_irq_desc *d = lkpi_irq_desc(irq);

	return (d != NULL ? lkpi_plat_intr_iblock(d->ip, d->idx) : NULL);
}

struct lkpi_devm_irq {
	unsigned int	irq;
	void		*dev_id;
};

static void
lkpi_devm_irq_release(struct device *dev, void *res)
{
	struct lkpi_devm_irq *r = res;

	(void) dev;
	(void) free_irq(r->irq, r->dev_id);
}

static int
lkpi_devm_irq_match(struct device *dev, void *res, void *data)
{
	struct lkpi_devm_irq *a = res, *b = data;

	(void) dev;
	return (a->irq == b->irq && a->dev_id == b->dev_id);
}

int
devm_request_threaded_irq(struct device *dev, unsigned int irq,
    irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags,
    const char *name, void *dev_id)
{
	struct lkpi_devm_irq *r;
	int err;

	r = devres_alloc(lkpi_devm_irq_release, sizeof (*r), GFP_KERNEL);
	if (r == NULL)
		return (-ENOMEM);
	err = request_threaded_irq(irq, handler, thread_fn, flags, name,
	    dev_id);
	if (err != 0) {
		devres_free(r);
		return (err);
	}
	r->irq = irq;
	r->dev_id = dev_id;
	devres_add(dev, r);
	return (0);
}

void
devm_free_irq(struct device *dev, unsigned int irq, void *dev_id)
{
	struct lkpi_devm_irq key = { irq, dev_id };

	WARN_ON(devres_release(dev, lkpi_devm_irq_release,
	    lkpi_devm_irq_match, &key) != 0);
}

/* PCI vectors. */

static bool lkpi_pci_irqs_requested(struct pci_dev *pdev);

int
pci_alloc_irq_vectors(struct pci_dev *pdev, unsigned int min_vecs,
    unsigned int max_vecs, unsigned int flags)
{
	struct lkpi_pci_irqs *pi;
	struct lkpi_plat_intr *ip;
	unsigned int types = 0, type;
	int count, r;

	if (pdev->lkpi_irqs != NULL) {
		/* The implicit INTx default gives way to the driver's choice. */
		if (!pdev->lkpi_irqs->implicit || lkpi_pci_irqs_requested(pdev))
			return (-EINVAL);
		pci_free_irq_vectors(pdev);
	}
	if (flags & PCI_IRQ_MSIX)
		types |= LKPI_INTR_MSIX;
	if (flags & PCI_IRQ_MSI)
		types |= LKPI_INTR_MSI;
	if (flags & PCI_IRQ_INTX)
		types |= LKPI_INTR_INTX;
	if (min_vecs == 0)
		min_vecs = 1;
	r = lkpi_plat_intr_alloc(pdev->dev.lkpi_dip, types, (int)min_vecs,
	    (int)max_vecs, &ip, &type, &count);
	if (r != 0)
		return (r);
	pi = kzalloc(sizeof (*pi) + sizeof (unsigned int) * (size_t)count,
	    GFP_KERNEL);
	if (pi == NULL) {
		lkpi_plat_intr_free(ip);
		return (-ENOMEM);
	}
	r = lkpi_irq_alloc_descs(ip, count, pi->irq);
	if (r != 0) {
		kfree(pi);
		lkpi_plat_intr_free(ip);
		return (r);
	}
	pi->ip = ip;
	pi->type = type;
	pi->count = count;
	pdev->lkpi_irqs = pi;
	pdev->irq = (int)pi->irq[0];
	pdev->msi_enabled = type == LKPI_INTR_MSI;
	pdev->msix_enabled = type == LKPI_INTR_MSIX;
	return (count);
}

void
pci_free_irq_vectors(struct pci_dev *pdev)
{
	struct lkpi_pci_irqs *pi = pdev->lkpi_irqs;
	int i;

	if (pi == NULL)
		return;
	mutex_lock(&lkpi_irq_lock);
	for (i = 0; i < pi->count; i++) {
		struct lkpi_irq_desc *d = &lkpi_irq_table[pi->irq[i]];

		WARN(d->handler != NULL || d->thread_fn != NULL,
		    "linuxkpi: irq %u still requested\n", pi->irq[i]);
		d->used = false;
	}
	mutex_unlock(&lkpi_irq_lock);
	lkpi_plat_intr_free(pi->ip);
	kfree(pi);
	pdev->lkpi_irqs = NULL;
	pdev->irq = 0;
	pdev->msi_enabled = 0;
	pdev->msix_enabled = 0;
}

int
pci_irq_vector(struct pci_dev *pdev, unsigned int nr)
{
	struct lkpi_pci_irqs *pi = pdev->lkpi_irqs;

	if (pi == NULL || nr >= (unsigned int)pi->count)
		return (-EINVAL);
	return ((int)pi->irq[nr]);
}

/* Any handler requested on the device's vectors? */
static bool
lkpi_pci_irqs_requested(struct pci_dev *pdev)
{
	struct lkpi_pci_irqs *pi = pdev->lkpi_irqs;
	bool busy = false;
	int i;

	mutex_lock(&lkpi_irq_lock);
	for (i = 0; i < pi->count; i++) {
		struct lkpi_irq_desc *d = &lkpi_irq_table[pi->irq[i]];

		if (d->handler != NULL || d->thread_fn != NULL)
			busy = true;
	}
	mutex_unlock(&lkpi_irq_lock);
	return (busy);
}

/*
 * Linux's pdev->irq always holds the legacy INTx line. Here a device
 * gets an implicit INTx vector when a driver binds (lkpi_pci_probe) and
 * after pci_enable_msi() fails, so request_irq(pdev->irq) works without
 * MSI (docs/13 P2). Allocating MSI/MSI-X replaces it; failures are
 * ignored (no INTx pin, no vector).
 */
void
lkpi_pci_intx_default(struct pci_dev *pdev)
{
	if (pdev->lkpi_irqs != NULL)
		return;
	if (pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_INTX) > 0)
		pdev->lkpi_irqs->implicit = true;
}

/* Driver unbind: release the implicit vector if still unused. */
void
lkpi_pci_intx_release(struct pci_dev *pdev)
{
	if (pdev->lkpi_irqs != NULL && pdev->lkpi_irqs->implicit &&
	    !lkpi_pci_irqs_requested(pdev))
		pci_free_irq_vectors(pdev);
}

int
pci_enable_msi(struct pci_dev *pdev)
{
	int r = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);

	if (r < 0) {
		lkpi_pci_intx_default(pdev);	/* pdev->irq = INTx */
		return (r);
	}
	return (0);
}

void
pci_disable_msi(struct pci_dev *pdev)
{
	if (pdev->msi_enabled) {
		pci_free_irq_vectors(pdev);
		lkpi_pci_intx_default(pdev);
	}
}

/* Tasklets over system_highpri_wq. */

void
lkpi_tasklet_work(struct work_struct *w)
{
	struct tasklet_struct *t = container_of(w, struct tasklet_struct,
	    lkpi_work);

	if (atomic_read(&t->count) != 0)
		return;		/* disabled: tasklet_enable() requeues */
	if (!tasklet_trylock(t)) {
		/* Run directly by its owner right now: try again later. */
		(void) queue_work(system_highpri_wq, &t->lkpi_work);
		return;
	}
	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state)) {
		if (t->use_callback)
			t->callback(t);
		else
			t->func(t->data);
	}
	tasklet_unlock(t);
}

void
tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long),
    unsigned long data)
{
	INIT_WORK(&t->lkpi_work, lkpi_tasklet_work);
	t->state = 0;
	atomic_set(&t->count, 0);
	t->use_callback = false;
	t->func = func;
	t->data = data;
}

void
tasklet_setup(struct tasklet_struct *t,
    void (*callback)(struct tasklet_struct *))
{
	INIT_WORK(&t->lkpi_work, lkpi_tasklet_work);
	t->state = 0;
	atomic_set(&t->count, 0);
	t->use_callback = true;
	t->callback = callback;
	t->data = 0;
}

void
tasklet_schedule(struct tasklet_struct *t)
{
	if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state) ||
	    !work_pending(&t->lkpi_work))
		(void) queue_work(system_highpri_wq, &t->lkpi_work);
}

void
tasklet_kill(struct tasklet_struct *t)
{
	(void) cancel_work_sync(&t->lkpi_work);
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}

void
tasklet_disable_nosync(struct tasklet_struct *t)
{
	atomic_inc(&t->count);
	smp_mb__after_atomic();
}

void
tasklet_disable(struct tasklet_struct *t)
{
	tasklet_disable_nosync(t);
	tasklet_unlock_wait(t);
}

void
tasklet_enable(struct tasklet_struct *t)
{
	smp_mb__before_atomic();
	if (atomic_dec_and_test(&t->count) &&
	    test_bit(TASKLET_STATE_SCHED, &t->state))
		(void) queue_work(system_highpri_wq, &t->lkpi_work);
}

bool
tasklet_trylock(struct tasklet_struct *t)
{
	return (!test_and_set_bit(TASKLET_STATE_RUN, &t->state));
}

void
tasklet_unlock(struct tasklet_struct *t)
{
	smp_mb__before_atomic();
	clear_bit(TASKLET_STATE_RUN, &t->state);
}

void
tasklet_unlock_wait(struct tasklet_struct *t)
{
	while (test_bit(TASKLET_STATE_RUN, &t->state))
		lkpi_plat_sleep_ns(10000);
}

/* Busy-waits (may be called with preemption disabled). */
void
tasklet_unlock_spin_wait(struct tasklet_struct *t)
{
	while (test_bit(TASKLET_STATE_RUN, &t->state))
		lkpi_cpu_relax();
}

void
lkpi_irq_work_fn(struct work_struct *w)
{
	struct irq_work *iw = container_of(w, struct irq_work, lkpi_work);

	iw->func(iw);
}
