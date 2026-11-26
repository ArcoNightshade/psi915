/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Userland hardware backend (see <linuxkpi/plat_dev.h>,
 * <linuxkpi/fakedev.h>): fake PCI devices for the unit tests. Compiles to
 * nothing in the kernel.
 */

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_USERLAND

#include <linuxkpi/fakedev.h>

#define	FAKE_PAGE	4096UL

static pthread_mutex_t fake_lock = PTHREAD_MUTEX_INITIALIZER;
static struct lkpi_fake_pci *fake_devs;

const char *lkpi_fake_firmware_dir = "/nonexistent";
const char *lkpi_fake_dmi[32];
char lkpi_fake_uevent_action[32];
char lkpi_fake_uevent_path[128];
char lkpi_fake_uevent_env0[128];
int lkpi_fake_uevent_count;

void
lkpi_fake_pci_add(struct lkpi_fake_pci *f)
{
	pthread_mutex_lock(&fake_lock);
	f->next = fake_devs;
	fake_devs = f;
	pthread_mutex_unlock(&fake_lock);
}

void
lkpi_fake_pci_remove(struct lkpi_fake_pci *f)
{
	struct lkpi_fake_pci **pp;

	pthread_mutex_lock(&fake_lock);
	for (pp = &fake_devs; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == f) {
			*pp = f->next;
			break;
		}
	}
	pthread_mutex_unlock(&fake_lock);
}

int
lkpi_plat_pci_info(void *dip, struct lkpi_plat_pci_info *info)
{
	*info = ((struct lkpi_fake_pci *)dip)->info;
	return (0);
}

int
lkpi_plat_pci_cfg_open(void *dip, void **cfgh)
{
	*cfgh = dip;
	return (0);
}

void
lkpi_plat_pci_cfg_close(void *cfgh)
{
	(void) cfgh;
}

u32
lkpi_plat_pci_cfg_read(void *cfgh, int where, int size)
{
	struct lkpi_fake_pci *f = cfgh;
	u32 v = 0;

	if (where < 0 || where + size > (int)sizeof (f->cfg))
		return (~0U);
	memcpy(&v, &f->cfg[where], (size_t)size);
	return (v);
}

void
lkpi_plat_pci_cfg_write(void *cfgh, int where, int size, u32 val)
{
	struct lkpi_fake_pci *f = cfgh;

	if (where < 0 || where + size > (int)sizeof (f->cfg))
		return;
	memcpy(&f->cfg[where], &val, (size_t)size);
}

void
lkpi_plat_pci_walk(int (*fn)(void *dip, void *arg), void *arg)
{
	struct lkpi_fake_pci *f;

	pthread_mutex_lock(&fake_lock);
	for (f = fake_devs; f != NULL; f = f->next)
		if (fn(f, arg) != 0)
			break;
	pthread_mutex_unlock(&fake_lock);
}

void lkpi_plat_dev_hold(void *dip) { (void) dip; }
void lkpi_plat_dev_rele(void *dip) { (void) dip; }
void *lkpi_plat_dev_parent(void *dip) { (void) dip; return (NULL); }

const char *
lkpi_plat_dev_name(void *dip)
{
	struct lkpi_fake_pci *f = dip;

	return (f->name != NULL ? f->name : "fakepci#0");
}

void *
lkpi_plat_map_regs(void *dip, int rnumber, u64 off, u64 len, int cache,
    void **handle)
{
	struct lkpi_fake_pci *f = dip;
	int bar = rnumber - 1;

	(void) cache;
	if (bar < 0 || bar >= LKPI_PCI_NUM_BARS || f->bar_mem[bar] == NULL ||
	    off + len > f->info.bar[bar].len)
		return (NULL);
	*handle = f;
	return ((char *)f->bar_mem[bar] + off);
}

void
lkpi_plat_unmap_regs(void **handle)
{
	*handle = NULL;
}

void *
lkpi_plat_map_phys(u64 phys, size_t len, int cache)
{
	struct lkpi_fake_pci *f;
	void *va = NULL;
	int i;

	(void) cache;
	pthread_mutex_lock(&fake_lock);
	for (f = fake_devs; f != NULL && va == NULL; f = f->next) {
		for (i = 0; i < LKPI_PCI_NUM_BARS; i++) {
			u64 s = f->info.bar[i].start, l = f->info.bar[i].len;

			if (f->bar_mem[i] != NULL && phys >= s &&
			    phys + len <= s + l) {
				va = (char *)f->bar_mem[i] + (phys - s);
				break;
			}
		}
	}
	pthread_mutex_unlock(&fake_lock);
	/* Otherwise "physical" addresses are kernel addresses here. */
	return (va != NULL ? va : (void *)(uintptr_t)phys);
}

void
lkpi_plat_unmap_phys(void *va, size_t len)
{
	(void) va;
	(void) len;
}

/* Interrupts. */

struct lkpi_plat_intr {
	struct lkpi_fake_pci	*f;
	int			count;
};

int
lkpi_plat_intr_alloc(void *dip, unsigned int types, int min, int max,
    struct lkpi_plat_intr **ip, unsigned int *type, int *count)
{
	struct lkpi_fake_pci *f = dip;
	struct lkpi_plat_intr *p;
	static const unsigned int order[] = {
		LKPI_INTR_MSIX, LKPI_INTR_MSI, LKPI_INTR_INTX
	};
	int i, n;

	for (i = 0; i < 3; i++) {
		if (!(types & order[i]) || !(f->intr_types & order[i]))
			continue;
		n = order[i] == LKPI_INTR_INTX ? 1 :
		    (f->max_vec < max ? f->max_vec : max);
		if (n > LKPI_FAKE_MAX_VEC)
			n = LKPI_FAKE_MAX_VEC;
		if (n < min || f->vec_alloc != 0)
			continue;
		p = calloc(1, sizeof (*p));
		if (p == NULL)
			return (-ENOMEM);
		p->f = f;
		p->count = n;
		f->vec_alloc = n;
		*ip = p;
		*type = order[i];
		*count = n;
		return (0);
	}
	return (-ENOSPC);
}

void
lkpi_plat_intr_free(struct lkpi_plat_intr *ip)
{
	ip->f->vec_alloc = 0;
	free(ip);
}

int
lkpi_plat_intr_add(struct lkpi_plat_intr *ip, int idx,
    bool (*fn)(void *arg), void *arg)
{
	if (idx < 0 || idx >= ip->count)
		return (-EINVAL);
	pthread_mutex_lock(&fake_lock);
	ip->f->vec_fn[idx] = fn;
	ip->f->vec_arg[idx] = arg;
	pthread_mutex_unlock(&fake_lock);
	return (0);
}

void
lkpi_plat_intr_remove(struct lkpi_plat_intr *ip, int idx)
{
	pthread_mutex_lock(&fake_lock);
	ip->f->vec_fn[idx] = NULL;
	ip->f->vec_arg[idx] = NULL;
	pthread_mutex_unlock(&fake_lock);
}

int
lkpi_plat_intr_enable(struct lkpi_plat_intr *ip, int idx)
{
	__atomic_store_n(&ip->f->vec_enabled[idx], true, __ATOMIC_SEQ_CST);
	return (0);
}

int
lkpi_plat_intr_disable(struct lkpi_plat_intr *ip, int idx)
{
	__atomic_store_n(&ip->f->vec_enabled[idx], false, __ATOMIC_SEQ_CST);
	return (0);
}

void *
lkpi_plat_intr_iblock(struct lkpi_plat_intr *ip, int idx)
{
	(void) ip;
	(void) idx;
	return (NULL);
}

bool
lkpi_plat_intr_hilevel(struct lkpi_plat_intr *ip, int idx)
{
	(void) ip;
	(void) idx;
	return (false);
}

bool
lkpi_fake_raise_irq(struct lkpi_fake_pci *f, int vec)
{
	bool (*fn)(void *);
	void *arg;
	bool r;

	pthread_mutex_lock(&fake_lock);
	fn = f->vec_fn[vec];
	arg = f->vec_arg[vec];
	pthread_mutex_unlock(&fake_lock);
	if (fn == NULL ||
	    !__atomic_load_n(&f->vec_enabled[vec], __ATOMIC_SEQ_CST))
		return (false);
	r = fn(arg);
	if (r)
		__atomic_fetch_add(&f->claimed, 1, __ATOMIC_SEQ_CST);
	return (r);
}

/*
 * DMA through a fake IOMMU: each binding gets a fresh, page-aligned bus
 * address range from a bump allocator starting at 1 MiB, so drivers that
 * use physical or virtual addresses instead of DMA addresses fail.
 */
struct lkpi_plat_dma {
	struct lkpi_plat_dma	*next;
	u64			bus;
	void			*kva;
	size_t			len;
	bool			coherent;
	size_t			alloc_len;
};

static struct lkpi_plat_dma *fake_dma_list;
static u64 fake_dma_next = 0x100000;
static int fake_dma_count;

static int
fake_dma_bind(u64 mask, void *kva, size_t len, bool coherent, size_t alloc,
    u64 *bus, struct lkpi_plat_dma **hp)
{
	struct lkpi_plat_dma *h;
	u64 off = (uintptr_t)kva & (FAKE_PAGE - 1);
	u64 span = (off + len + FAKE_PAGE - 1) & ~(FAKE_PAGE - 1);

	h = calloc(1, sizeof (*h));
	if (h == NULL)
		return (-ENOMEM);
	pthread_mutex_lock(&fake_lock);
	if (fake_dma_next + span - 1 > mask) {
		pthread_mutex_unlock(&fake_lock);
		free(h);
		return (-EIO);
	}
	h->bus = fake_dma_next + off;
	fake_dma_next += span + FAKE_PAGE;	/* guard page */
	h->kva = kva;
	h->len = len;
	h->coherent = coherent;
	h->alloc_len = alloc;
	h->next = fake_dma_list;
	fake_dma_list = h;
	fake_dma_count++;
	pthread_mutex_unlock(&fake_lock);
	*bus = h->bus;
	*hp = h;
	return (0);
}

static void
fake_dma_unbind(struct lkpi_plat_dma *h)
{
	struct lkpi_plat_dma **pp;

	pthread_mutex_lock(&fake_lock);
	for (pp = &fake_dma_list; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == h) {
			*pp = h->next;
			fake_dma_count--;
			break;
		}
	}
	pthread_mutex_unlock(&fake_lock);
}

void *
lkpi_fake_dma_kva(u64 bus, size_t len)
{
	struct lkpi_plat_dma *h;
	void *va = NULL;

	pthread_mutex_lock(&fake_lock);
	for (h = fake_dma_list; h != NULL; h = h->next) {
		if (bus >= h->bus && bus + len <= h->bus + h->len) {
			va = (char *)h->kva + (bus - h->bus);
			break;
		}
	}
	pthread_mutex_unlock(&fake_lock);
	return (va);
}

int
lkpi_fake_dma_live(void)
{
	return (__atomic_load_n(&fake_dma_count, __ATOMIC_SEQ_CST));
}

int
lkpi_plat_dma_alloc_coherent(void *dip, u64 mask, size_t size, void **kva,
    u64 *bus, struct lkpi_plat_dma **h)
{
	size_t alloc = (size + FAKE_PAGE - 1) & ~(FAKE_PAGE - 1);
	void *p;
	int r;

	(void) dip;
	if (posix_memalign(&p, FAKE_PAGE, alloc) != 0)
		return (-ENOMEM);
	memset(p, 0, alloc);
	r = fake_dma_bind(mask, p, size, true, alloc, bus, h);
	if (r != 0) {
		free(p);
		return (r);
	}
	*kva = p;
	return (0);
}

void
lkpi_plat_dma_free_coherent(struct lkpi_plat_dma *h)
{
	fake_dma_unbind(h);
	free(h->kva);
	free(h);
}

int
lkpi_plat_dma_map(void *dip, u64 mask, void *kva, size_t size, int dir,
    u64 *bus, struct lkpi_plat_dma **h)
{
	(void) dip;
	(void) dir;
	return (fake_dma_bind(mask, kva, size, false, 0, bus, h));
}

void
lkpi_plat_dma_unmap(struct lkpi_plat_dma *h)
{
	fake_dma_unbind(h);
	free(h);
}

void
lkpi_plat_dma_sync(struct lkpi_plat_dma *h, size_t off, size_t len,
    bool for_device)
{
	(void) h;
	(void) off;
	(void) len;
	(void) for_device;
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
}

/* Memory: "physical" addresses equal kernel addresses here. */
void *
lkpi_plat_contig_alloc(size_t size, bool zero, bool cansleep, void **cookie)
{
	void *p;

	(void) cansleep;
	if (posix_memalign(&p, FAKE_PAGE, size) != 0)
		return (NULL);
	if (zero)
		memset(p, 0, size);
	*cookie = NULL;
	return (p);
}

void
lkpi_plat_contig_free(void *kva, size_t size, void *cookie)
{
	(void) size;
	(void) cookie;
	free(kva);
}

u64
lkpi_plat_virt_to_phys(const void *kva)
{
	return ((u64)(uintptr_t)kva);
}

/* Only physically (here: virtually) contiguous pages can be "mapped". */
void *
lkpi_plat_vmap(const u64 *phys, size_t npages, int cache)
{
	size_t i;

	(void) cache;
	for (i = 1; i < npages; i++)
		if (phys[i] != phys[0] + i * FAKE_PAGE)
			return (NULL);
	return ((void *)(uintptr_t)phys[0]);
}

void
lkpi_plat_vunmap(void *va, size_t npages)
{
	(void) va;
	(void) npages;
}

int
lkpi_plat_set_cache(void *kva, size_t len, int cache)
{
	(void) kva;
	(void) len;
	(void) cache;
	return (0);
}

unsigned long
lkpi_plat_physmem_pages(void)
{
	long n = sysconf(_SC_PHYS_PAGES);

	return (n > 0 ? (unsigned long)n : 262144UL);
}

int
lkpi_plat_read_file(const char *path, void **bufp, size_t *lenp)
{
	char full[512];
	FILE *fp;
	long len;
	void *buf;

	if (path[0] == '/' && strncmp(path, "/usr/lib/firmware/", 18) == 0)
		path += 18;
	(void) snprintf(full, sizeof (full), "%s/%s", lkpi_fake_firmware_dir,
	    path);
	fp = fopen(full, "rb");
	if (fp == NULL)
		return (-ENOENT);
	if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) < 0 ||
	    fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return (-EIO);
	}
	buf = lkpi_plat_alloc(len > 0 ? (size_t)len : 1, LKPI_PLAT_SLEEP);
	if (len > 0 && fread(buf, 1, (size_t)len, fp) != (size_t)len) {
		fclose(fp);
		lkpi_plat_free(buf, (size_t)len);
		return (-EIO);
	}
	fclose(fp);
	*bufp = buf;
	*lenp = (size_t)len;
	return (0);
}

const char *
lkpi_plat_dmi_string(int field)
{
	return (field >= 0 && field < 32 ? lkpi_fake_dmi[field] : NULL);
}

int
lkpi_plat_acpi_get_handle(void *parent, const char *path, void **h)
{
	(void) parent;
	(void) path;
	*h = NULL;
	return (-ENODEV);
}

int
lkpi_plat_acpi_evaluate(void *h, const char *method, void *args, int nargs,
    void **result)
{
	(void) h;
	(void) method;
	(void) args;
	(void) nargs;
	*result = NULL;
	return (-ENODEV);
}

void
lkpi_plat_acpi_free(void *obj)
{
	free(obj);
}

void *
lkpi_plat_acpi_dev_handle(void *dip)
{
	(void) dip;
	return (NULL);
}

void
lkpi_plat_uevent(void *dip, const char *action, const char *devpath,
    char *const *envp, int nenv)
{
	(void) dip;
	(void) snprintf(lkpi_fake_uevent_action,
	    sizeof (lkpi_fake_uevent_action), "%s", action);
	(void) snprintf(lkpi_fake_uevent_path, sizeof (lkpi_fake_uevent_path),
	    "%s", devpath);
	(void) snprintf(lkpi_fake_uevent_env0, sizeof (lkpi_fake_uevent_env0),
	    "%s", nenv > 0 ? envp[0] : "");
	__atomic_fetch_add(&lkpi_fake_uevent_count, 1, __ATOMIC_SEQ_CST);
}

u64 lkpi_plat_rdmsr(u32 msr) { (void) msr; return (0); }
void lkpi_plat_wrmsr(u32 msr, u64 val) { (void) msr; (void) val; }
void lkpi_plat_wbinvd(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

#endif /* LINUXKPI_USERLAND */

/*
 * The fake DMA backend always translates (fake IOMMU), so devices are
 * IOMMU-mapped unless a test sets the override to 0.
 */
int linuxkpi_iommu_mapped = -1;

bool
lkpi_plat_dev_iommu_mapped(void *dip)
{
	(void) dip;
	if (linuxkpi_iommu_mapped >= 0)
		return (linuxkpi_iommu_mapped != 0);
	return (true);
}
