/**
 * Copyright (c) 2022 Ventana Micro Systems Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file cpu_vcpu_nested.c
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief source of VCPU nested functions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_guest_aspace.h>
#include <vmm_host_aspace.h>
#include <generic_mmu.h>
#include <libs/list.h>

#include <cpu_hwcap.h>
#include <cpu_tlb.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_trap.h>
#include <riscv_csr.h>

/*
 * We share the same host VMID between virtual-HS/U modes and
 * virtual-VS/VU modes. To achieve this, we flush all guest TLB
 * entries upon:
 * 1) Change in nested virt state (ON => OFF or OFF => ON)
 * 2) Change in guest hgatp.VMID
 */

#define NESTED_SWTLB_ITLB_MAX_ENTRY	128
#define NESTED_SWTLB_DTLB_MAX_ENTRY	128
#define NESTED_SWTLB_MAX_ENTRY		(NESTED_SWTLB_ITLB_MAX_ENTRY + \
					 NESTED_SWTLB_DTLB_MAX_ENTRY)

struct nested_swtlb_entry {
	struct dlist head;
	struct mmu_page page;
	struct mmu_page shadow_page;
	u32 shadow_reg_flags;
};

struct nested_swtlb_xtlb {
	struct dlist active_list;
	struct dlist free_list;
};

struct nested_swtlb {
	struct nested_swtlb_xtlb itlb;
	struct nested_swtlb_xtlb dtlb;
	struct nested_swtlb_entry *entries;
};

static const struct nested_swtlb_entry *nested_swtlb_lookup(
						struct vmm_vcpu *vcpu,
						physical_addr_t guest_gpa)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	struct nested_swtlb *swtlb = npriv->swtlb;
	struct nested_swtlb_entry *swte;

	list_for_each_entry(swte, &swtlb->itlb.active_list, head) {
		if (swte->shadow_page.ia <= guest_gpa &&
		    guest_gpa < (swte->shadow_page.ia + swte->shadow_page.sz)) {
			list_del(&swte->head);
			list_add(&swte->head, &swtlb->itlb.active_list);
			return swte;
		}
	}

	list_for_each_entry(swte, &swtlb->dtlb.active_list, head) {
		if (swte->shadow_page.ia <= guest_gpa &&
		    guest_gpa < (swte->shadow_page.ia + swte->shadow_page.sz)) {
			list_del(&swte->head);
			list_add(&swte->head, &swtlb->dtlb.active_list);
			return swte;
		}
	}

	return NULL;
}

static void nested_swtlb_update(struct vmm_vcpu *vcpu, bool itlb,
				const struct mmu_page *page,
				const struct mmu_page *shadow_page,
				u32 shadow_reg_flags)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	struct nested_swtlb *swtlb = npriv->swtlb;
	struct nested_swtlb_entry *swte;
	struct nested_swtlb_xtlb *xtlb;
	int rc;

	xtlb = (itlb) ? &swtlb->itlb : &swtlb->dtlb;

	if (!list_empty(&xtlb->free_list)) {
		swte = list_entry(list_pop(&xtlb->free_list),
				  struct nested_swtlb_entry, head);
	} else if (!list_empty(&xtlb->active_list)) {
		swte = list_entry(list_pop_tail(&xtlb->active_list),
				  struct nested_swtlb_entry, head);
		rc = mmu_unmap_page(npriv->pgtbl, &swte->shadow_page);
		if (rc) {
			vmm_panic("%s: shadow page unmap @ 0x%"PRIPADDR
				  " failed (error %d)\n", __func__,
				  swte->shadow_page.ia, rc);
		}
	} else {
		BUG_ON(1);
	}

	memcpy(&swte->page, page, sizeof(swte->page));
	memcpy(&swte->shadow_page, shadow_page, sizeof(swte->shadow_page));
	swte->shadow_reg_flags = shadow_reg_flags;

	rc = mmu_map_page(npriv->pgtbl, &swte->shadow_page);
	if (rc) {
		vmm_panic("%s: shadow page map @ 0x%"PRIPADDR
			  " failed (error %d)\n", __func__,
			  swte->shadow_page.ia, rc);
	}

	list_add(&swte->head, &xtlb->active_list);
}

void cpu_vcpu_nested_swtlb_flush(struct vmm_vcpu *vcpu,
				 physical_addr_t guest_gpa,
				 physical_size_t guest_gpa_size)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	struct nested_swtlb *swtlb = npriv->swtlb;
	physical_addr_t start, end, pstart, pend;
	struct nested_swtlb_entry *swte, *nswte;
	int rc;

	if (!guest_gpa && !guest_gpa_size) {
		start = 0;
		end = ~start;
	} else {
		start = guest_gpa;
		end = guest_gpa + guest_gpa_size - 1;
	}

	list_for_each_entry_safe(swte, nswte, &swtlb->itlb.active_list, head) {
		pstart = swte->page.ia;
		pend = swte->page.ia + swte->page.sz - 1;
		if (end < pstart || pend < start)
			continue;

		list_del(&swte->head);

		rc = mmu_unmap_page(npriv->pgtbl, &swte->shadow_page);
		if (rc) {
			vmm_panic("%s: shadow page unmap @ 0x%"PRIPADDR
				  " failed (error %d)\n", __func__,
				  swte->shadow_page.ia, rc);
		}

		list_add_tail(&swte->head, &swtlb->itlb.free_list);
	}

	list_for_each_entry_safe(swte, nswte, &swtlb->dtlb.active_list, head) {
		pstart = swte->page.ia;
		pend = swte->page.ia + swte->page.sz - 1;
		if (end < pstart || pend < start)
			continue;

		list_del(&swte->head);

		rc = mmu_unmap_page(npriv->pgtbl, &swte->shadow_page);
		if (rc) {
			vmm_panic("%s: shadow page unmap @ 0x%"PRIPADDR
				  " failed (error %d)\n", __func__,
				  swte->shadow_page.ia, rc);
		}

		list_add_tail(&swte->head, &swtlb->dtlb.free_list);
	}
}

static int nested_swtlb_init(struct vmm_vcpu *vcpu)
{
	int i;
	struct nested_swtlb *swtlb;
	struct nested_swtlb_entry *swte;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	swtlb = vmm_zalloc(sizeof(*swtlb));
	if (!swtlb) {
		return VMM_ENOMEM;
	}
	INIT_LIST_HEAD(&swtlb->itlb.active_list);
	INIT_LIST_HEAD(&swtlb->itlb.free_list);
	INIT_LIST_HEAD(&swtlb->dtlb.active_list);
	INIT_LIST_HEAD(&swtlb->dtlb.free_list);

	swtlb->entries = vmm_zalloc(sizeof(*swtlb->entries) *
				    NESTED_SWTLB_MAX_ENTRY);
	if (!swtlb->entries) {
		vmm_free(swtlb);
		return VMM_ENOMEM;
	}

	for (i = 0; i < NESTED_SWTLB_MAX_ENTRY; i++) {
		swte = &swtlb->entries[i];
		INIT_LIST_HEAD(&swte->head);
		if (i < NESTED_SWTLB_ITLB_MAX_ENTRY) {
			list_add_tail(&swte->head, &swtlb->itlb.free_list);
		} else {
			list_add_tail(&swte->head, &swtlb->dtlb.free_list);
		}
	}

	npriv->swtlb = swtlb;
	return VMM_OK;
}

static void nested_swtlb_deinit(struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	struct nested_swtlb *swtlb = npriv->swtlb;

	vmm_free(swtlb->entries);
	vmm_free(swtlb);
}

#ifdef CONFIG_64BIT
#define NESTED_MMU_OFF_BLOCK_SIZE	PGTBL_L2_BLOCK_SIZE
#else
#define NESTED_MMU_OFF_BLOCK_SIZE	PGTBL_L1_BLOCK_SIZE
#endif

enum nested_xlate_access {
	NESTED_XLATE_LOAD = 0,
	NESTED_XLATE_STORE,
	NESTED_XLATE_FETCH,
};

struct nested_xlate_context {
	/* VCPU for which translation is being done */
	struct vmm_vcpu *vcpu;

	/* Original access type for fault generation */
	enum nested_xlate_access original_access;

	/* Details from CSR or instruction */
	bool smode;
	unsigned long sstatus;
	bool hlvx;

	/* Final host region details */
	physical_addr_t host_pa;
	physical_size_t host_sz;
	u32 host_reg_flags;

	/* Fault details */
	physical_size_t nostage_page_sz;
	physical_size_t gstage_page_sz;
	physical_size_t vsstage_page_sz;
	unsigned long scause;
	unsigned long stval;
	unsigned long htval;
};

#define nested_xlate_context_init(__x, __v, __a, __smode, __sstatus, __hlvx)\
do {									\
	(__x)->vcpu = (__v);						\
	(__x)->original_access = (__a);					\
	(__x)->smode = (__smode);					\
	(__x)->sstatus = (__sstatus);					\
	(__x)->hlvx = (__hlvx);						\
	(__x)->host_pa = 0;						\
	(__x)->host_sz = 0;						\
	(__x)->host_reg_flags = 0;					\
	(__x)->nostage_page_sz = 0;					\
	(__x)->gstage_page_sz = 0;					\
	(__x)->vsstage_page_sz = 0;					\
	(__x)->scause = 0;						\
	(__x)->stval = 0;						\
	(__x)->htval = 0;						\
} while (0)

static int nested_nostage_perm_check(enum nested_xlate_access guest_access,
				     u32 reg_flags)
{
	if ((guest_access == NESTED_XLATE_LOAD) ||
	    (guest_access == NESTED_XLATE_FETCH)) {
		if (!(reg_flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM))) {
			return VMM_EFAULT;
		}
	} else if (guest_access == NESTED_XLATE_STORE) {
		if (!(reg_flags & VMM_REGION_ISRAM)) {
			return VMM_EFAULT;
		}
	}

	return VMM_OK;
}

static int nested_xlate_nostage_single(struct vmm_guest *guest,
				       physical_addr_t guest_hpa,
				       physical_size_t block_size,
				       enum nested_xlate_access guest_access,
				       physical_addr_t *out_host_pa,
				       physical_size_t *out_host_sz,
				       u32 *out_host_reg_flags)
{
	int rc;
	u32 reg_flags = 0x0;
	physical_size_t availsz = 0;
	physical_addr_t inaddr, outaddr = 0;

	inaddr = guest_hpa & ~(block_size - 1);

	/* Map host physical address */
	rc = vmm_guest_physical_map(guest, inaddr, block_size,
				    &outaddr, &availsz, &reg_flags);
	if (rc || (availsz < block_size)) {
		return VMM_EFAULT;
	}

	/* Check region permissions */
	rc = nested_nostage_perm_check(guest_access, reg_flags);
	if (rc) {
		return rc;
	}

	/* Update return values */
	if (out_host_pa) {
		*out_host_pa = outaddr;
	}
	if (out_host_sz) {
		*out_host_sz = block_size;
	}
	if (out_host_reg_flags) {
		*out_host_reg_flags = reg_flags;
	}

	return VMM_OK;
}

static void nested_gstage_write_fault(struct nested_xlate_context *xc,
				      physical_addr_t guest_gpa)
{
	/* We should never have non-zero scause here. */
	BUG_ON(xc->scause);

	switch (xc->original_access) {
	case NESTED_XLATE_LOAD:
		xc->scause = CAUSE_LOAD_GUEST_PAGE_FAULT;
		xc->htval = guest_gpa >> 2;
		break;
	case NESTED_XLATE_STORE:
		xc->scause = CAUSE_STORE_GUEST_PAGE_FAULT;
		xc->htval = guest_gpa >> 2;
		break;
	case NESTED_XLATE_FETCH:
		xc->scause = CAUSE_FETCH_GUEST_PAGE_FAULT;
		xc->htval = guest_gpa >> 2;
		break;
	default:
		break;
	};
}

/* Translate guest hpa to host pa */
static int nested_xlate_nostage(struct nested_xlate_context *xc,
				physical_addr_t guest_hpa,
				enum nested_xlate_access guest_access)
{
	int rc;
	u32 outflags = 0;
	physical_size_t outsz = 0;
	physical_addr_t outaddr = 0;

	/* Translate host physical address with L0 block size */
	rc = nested_xlate_nostage_single(xc->vcpu->guest, guest_hpa,
					 PGTBL_L0_BLOCK_SIZE, guest_access,
					 &outaddr, &outsz, &outflags);
	if (rc) {
		return rc;
	}

	/* Try to translate host physical address with L1 block size */
	rc = nested_xlate_nostage_single(xc->vcpu->guest, guest_hpa,
					 PGTBL_L1_BLOCK_SIZE, guest_access,
					 &outaddr, &outsz, &outflags);
	if (rc) {
		goto done;
	}

#ifdef CONFIG_64BIT
	/* Try to translate host physical address with L2 block size */
	rc = nested_xlate_nostage_single(xc->vcpu->guest, guest_hpa,
					 PGTBL_L2_BLOCK_SIZE, guest_access,
					 &outaddr, &outsz, &outflags);
	if (rc) {
		goto done;
	}
#endif

done:
	/* Update return values */
	xc->host_pa = outaddr;
	xc->host_sz = outsz;
	xc->host_reg_flags = outflags;

	return VMM_OK;
}

static void nested_gstage_setfault(void *opaque, int stage, int level,
				   physical_addr_t guest_gpa)
{
	struct nested_xlate_context *xc = opaque;

	xc->gstage_page_sz = arch_mmu_level_block_size(stage, level);
	nested_gstage_write_fault(xc, guest_gpa);
}

static int nested_gstage_gpa2hpa(void *opaque, int stage, int level,
				 physical_addr_t guest_hpa,
				 physical_addr_t *out_host_pa)
{
	int rc;
	physical_size_t outsz = 0;
	physical_addr_t outaddr = 0;
	struct nested_xlate_context *xc = opaque;

	rc = nested_xlate_nostage_single(xc->vcpu->guest, guest_hpa,
					 PGTBL_L0_BLOCK_SIZE,
					 NESTED_XLATE_LOAD,
					 &outaddr, &outsz, NULL);
	if (rc) {
		return rc;
	}

	*out_host_pa = outaddr | (guest_hpa & (outsz - 1));
	return VMM_OK;
}

static struct mmu_get_guest_page_ops nested_xlate_gstage_ops = {
	.gpa2hpa = nested_gstage_gpa2hpa,
	.setfault = nested_gstage_setfault,
};

/* Translate guest gpa to host pa */
static int nested_xlate_gstage(struct nested_xlate_context *xc,
			       physical_addr_t guest_gpa,
			       enum nested_xlate_access guest_access)
{
	int rc;
	unsigned long mode;
	bool perm_fault = FALSE;
	physical_addr_t pgtlb, guest_hpa;
	struct mmu_page page, shadow_page;
	const struct nested_swtlb_entry *swte = NULL;
	struct riscv_priv_nested *npriv = riscv_nested_priv(xc->vcpu);

	/* Find guest G-stage page */
	mode = (npriv->hgatp & HGATP_MODE) >> HGATP_MODE_SHIFT;
	if ((swte = nested_swtlb_lookup(xc->vcpu, guest_gpa))) {
		memcpy(&page, &swte->page, sizeof(page));
	} else if (mode == HGATP_MODE_OFF) {
		memset(&page, 0, sizeof(page));
		page.sz = NESTED_MMU_OFF_BLOCK_SIZE;
		page.ia = guest_gpa & ~(page.sz - 1);
		page.oa = guest_gpa & ~(page.sz - 1);
		page.flags.dirty = 1;
		page.flags.accessed = 1;
		page.flags.global = 1;
		page.flags.user = 1;
		page.flags.read = 1;
		page.flags.write = 1;
		page.flags.execute = 1;
		page.flags.valid = 1;
	} else {
		switch (mode) {
#ifdef CONFIG_64BIT
		case HGATP_MODE_SV57X4:
			rc = 4;
			break;
		case HGATP_MODE_SV48X4:
			rc = 3;
			break;
		case HGATP_MODE_SV39X4:
			rc = 2;
			break;
#else
		case HGATP_MODE_SV32X4:
			rc = 1;
			break;
#endif
		default:
			return VMM_EFAIL;
		}

		pgtlb = (npriv->hgatp & HGATP_PPN) << PGTBL_PAGE_SIZE_SHIFT;
		rc = mmu_get_guest_page(pgtlb, MMU_STAGE2, rc,
					&nested_xlate_gstage_ops, xc,
					guest_gpa, &page);
		if (rc) {
			return rc;
		}
	}

	/* Check guest G-stage page permissions */
	if (!page.flags.user) {
		perm_fault = TRUE;
	} else if (guest_access == NESTED_XLATE_FETCH || xc->hlvx) {
		perm_fault = !page.flags.execute;
	} else if (guest_access == NESTED_XLATE_LOAD) {
		perm_fault = !(page.flags.read ||
			((xc->sstatus & SSTATUS_MXR) && page.flags.execute)) ||
			!page.flags.accessed;
	} else if (guest_access == NESTED_XLATE_STORE) {
		perm_fault = !page.flags.read ||
			     !page.flags.write ||
			     !page.flags.accessed ||
			     !page.flags.dirty;
	}
	if (perm_fault) {
		xc->gstage_page_sz = page.sz;
		nested_gstage_write_fault(xc, guest_gpa);
		return VMM_EFAULT;
	}

	/* Update host region details */
	if (swte) {
		/* Get host details from software TLB entry */
		xc->host_pa = swte->shadow_page.oa;
		xc->host_sz = swte->shadow_page.sz;
		xc->host_reg_flags = swte->shadow_reg_flags;

		/* Check shadow page permissions */
		rc = VMM_EFAULT;
		switch (guest_access) {
		case NESTED_XLATE_LOAD:
			if (swte->shadow_page.flags.read) {
				rc = VMM_OK;
			}
			break;
		case NESTED_XLATE_STORE:
			if (swte->shadow_page.flags.read &&
			    swte->shadow_page.flags.write) {
				rc = VMM_OK;
			}
			break;
		case NESTED_XLATE_FETCH:
			if (swte->shadow_page.flags.execute) {
				rc = VMM_OK;
			}
			break;
		default:
			break;
		}
		if (rc) {
			if (rc == VMM_EFAULT) {
				xc->nostage_page_sz = page.sz;
				nested_gstage_write_fault(xc, guest_gpa);
			}
			return rc;
		}
	} else {
		/* Calculate guest hpa */
		guest_hpa = page.oa | (guest_gpa & (page.sz - 1));

		/* Translate guest hpa to host pa */
		rc = nested_xlate_nostage(xc, guest_hpa, guest_access);
		if (rc) {
			if (rc == VMM_EFAULT) {
				xc->nostage_page_sz = page.sz;
				nested_gstage_write_fault(xc, guest_gpa);
			}
			return rc;
		}

		/* Update output address and size */
		if (page.sz <= xc->host_sz) {
			xc->host_pa |= guest_hpa & (xc->host_sz - 1);
			xc->host_sz = page.sz;
			xc->host_pa &= ~(xc->host_sz - 1);
		}

		/* Prepare shadow page */
		memset(&shadow_page, 0, sizeof(shadow_page));
		shadow_page.ia = (page.sz <= xc->host_sz) ?
				 page.ia : guest_gpa & ~(xc->host_sz - 1);
		shadow_page.oa = xc->host_pa;
		shadow_page.sz = xc->host_sz;
		shadow_page.flags.dirty = 1;
		shadow_page.flags.accessed = 1;
		shadow_page.flags.global = 0;
		shadow_page.flags.user = 1;
		shadow_page.flags.read = 0;
		if ((xc->host_reg_flags &
		     (VMM_REGION_ISRAM | VMM_REGION_ISROM)) &&
		    page.flags.read) {
			shadow_page.flags.read = 1;
		}
		shadow_page.flags.write = 0;
		if ((xc->host_reg_flags & VMM_REGION_ISRAM) &&
		    page.flags.write) {
			shadow_page.flags.write = 1;
		}
		shadow_page.flags.execute = page.flags.execute;
		shadow_page.flags.valid = 1;

		/* Update software TLB */
		nested_swtlb_update(xc->vcpu,
			(guest_access == NESTED_XLATE_FETCH) ? TRUE : FALSE,
			&page, &shadow_page, xc->host_reg_flags);
	}

	return VMM_OK;
}

static void nested_vsstage_write_fault(struct nested_xlate_context *xc,
				       physical_addr_t guest_gva)
{
	switch (xc->original_access) {
	case NESTED_XLATE_LOAD:
		if (!xc->scause) {
			xc->scause = CAUSE_LOAD_PAGE_FAULT;
		}
		xc->stval = guest_gva;
		break;
	case NESTED_XLATE_STORE:
		if (!xc->scause) {
			xc->scause = CAUSE_STORE_PAGE_FAULT;
		}
		xc->stval = guest_gva;
		break;
	case NESTED_XLATE_FETCH:
		if (!xc->scause) {
			xc->scause = CAUSE_FETCH_PAGE_FAULT;
		}
		xc->stval = guest_gva;
		break;
	default:
		break;
	};
}

static void nested_vsstage_setfault(void *opaque, int stage, int level,
				    physical_addr_t guest_gva)
{
	struct nested_xlate_context *xc = opaque;

	xc->vsstage_page_sz = arch_mmu_level_block_size(stage, level);
	nested_vsstage_write_fault(xc, guest_gva);
}

static int nested_vsstage_gpa2hpa(void *opaque, int stage, int level,
				  physical_addr_t guest_gpa,
				  physical_addr_t *out_host_pa)
{
	int rc;
	struct nested_xlate_context *xc = opaque;

	rc = nested_xlate_gstage(xc, guest_gpa, NESTED_XLATE_LOAD);
	if (rc) {
		return rc;
	}

	*out_host_pa = xc->host_pa | (guest_gpa & (xc->host_sz - 1));
	return VMM_OK;
}

static struct mmu_get_guest_page_ops nested_xlate_vsstage_ops = {
	.setfault = nested_vsstage_setfault,
	.gpa2hpa = nested_vsstage_gpa2hpa,
};

/* Translate guest gva to host pa */
static int nested_xlate_vsstage(struct nested_xlate_context *xc,
				physical_addr_t guest_gva,
				enum nested_xlate_access guest_access)
{
	int rc;
	unsigned long mode;
	struct mmu_page page;
	bool perm_fault = FALSE;
	physical_addr_t pgtlb, guest_gpa;
	struct riscv_priv_nested *npriv = riscv_nested_priv(xc->vcpu);

	/* Get guest VS-stage page */
	mode = (npriv->vsatp & SATP_MODE) >> SATP_MODE_SHIFT;
	if (mode == SATP_MODE_OFF) {
		memset(&page, 0, sizeof(page));
		page.sz = NESTED_MMU_OFF_BLOCK_SIZE;
		page.ia = guest_gva & ~(page.sz - 1);
		page.oa = guest_gva & ~(page.sz - 1);
		page.flags.dirty = 1;
		page.flags.accessed = 1;
		page.flags.global = 1;
		page.flags.user = 1;
		page.flags.read = 1;
		page.flags.write = 1;
		page.flags.execute = 1;
		page.flags.valid = 1;
	} else {
		switch (mode) {
#ifdef CONFIG_64BIT
		case SATP_MODE_SV57:
			rc = 4;
			break;
		case SATP_MODE_SV48:
			rc = 3;
			break;
		case SATP_MODE_SV39:
			rc = 2;
			break;
#else
		case SATP_MODE_SV32:
			rc = 1;
			break;
#endif
		default:
			return VMM_EFAIL;
		}

		pgtlb = (npriv->vsatp & SATP_PPN) << PGTBL_PAGE_SIZE_SHIFT;
		rc = mmu_get_guest_page(pgtlb, MMU_STAGE1, rc,
					&nested_xlate_vsstage_ops, xc,
					guest_gva, &page);
		if (rc) {
			return rc;
		}
	}

	/* Check guest VS-stage page permissions */
	if (page.flags.user ?
	    xc->smode && (guest_access == NESTED_XLATE_FETCH ||
			  (xc->sstatus & SSTATUS_SUM)) :
	    !xc->smode) {
		perm_fault = TRUE;
	} else if (guest_access == NESTED_XLATE_FETCH || xc->hlvx) {
		perm_fault = !page.flags.execute;
	} else if (guest_access == NESTED_XLATE_LOAD) {
		perm_fault = !(page.flags.read ||
			((xc->sstatus & SSTATUS_MXR) && page.flags.execute)) ||
			!page.flags.accessed;
	} else if (guest_access == NESTED_XLATE_STORE) {
		perm_fault = !page.flags.read ||
			     !page.flags.write ||
			     !page.flags.accessed ||
			     !page.flags.dirty;
	}
	if (perm_fault) {
		xc->vsstage_page_sz = page.sz;
		nested_vsstage_write_fault(xc, guest_gva);
		return VMM_EFAULT;
	}

	/* Calculate guest gpa */
	guest_gpa = page.oa | (guest_gva & (page.sz - 1));

	/* Translate guest gpa to host pa */
	rc = nested_xlate_gstage(xc, guest_gpa, guest_access);
	if (rc) {
		if (rc == VMM_EFAULT) {
			nested_vsstage_write_fault(xc, guest_gva);
		}
		return rc;
	}

	/* Update output address and size */
	if (page.sz <= xc->host_sz) {
		xc->host_pa |= guest_gpa & (xc->host_sz - 1);
		xc->host_sz = page.sz;
		xc->host_pa &= ~(xc->host_sz - 1);
	} else {
		page.ia = guest_gva & ~(xc->host_sz - 1);
		page.oa = guest_gpa & ~(xc->host_sz - 1);
		page.sz = xc->host_sz;
	}

	return VMM_OK;
}

static void nested_timer_event_expired(struct vmm_timer_event *ev)
{
	/* Do nothing */
}

int cpu_vcpu_nested_init(struct vmm_vcpu *vcpu)
{
	int rc;
	struct vmm_timer_event *event;
	u32 pgtbl_attr = 0, pgtbl_hw_tag = 0;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	event = vmm_zalloc(sizeof(struct vmm_timer_event));
	if (!event) {
		return VMM_ENOMEM;
	}
	INIT_TIMER_EVENT(event, nested_timer_event_expired, vcpu);
	npriv->timer_event = event;

	if (riscv_stage2_vmid_available()) {
		pgtbl_hw_tag = riscv_stage2_vmid_nested + vcpu->guest->id;
		pgtbl_attr |= MMU_ATTR_HW_TAG_VALID;
	}
	npriv->pgtbl = mmu_pgtbl_alloc(MMU_STAGE2, -1, pgtbl_attr,
				       pgtbl_hw_tag);
	if (!npriv->pgtbl) {
		vmm_free(npriv->timer_event);
		return VMM_ENOMEM;
	}

	rc = nested_swtlb_init(vcpu);
	if (rc) {
		mmu_pgtbl_free(npriv->pgtbl);
		vmm_free(npriv->timer_event);
		return rc;
	}

	return VMM_OK;
}

void cpu_vcpu_nested_reset(struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	vmm_timer_event_stop(npriv->timer_event);
	cpu_vcpu_nested_swtlb_flush(vcpu, 0, 0);
	npriv->virt = FALSE;
#ifdef CONFIG_64BIT
	npriv->hstatus = HSTATUS_VSXL_RV64 << HSTATUS_VSXL_SHIFT;
#else
	npriv->hstatus = 0;
#endif
	npriv->hedeleg = 0;
	npriv->hideleg = 0;
	npriv->hvip = 0;
	npriv->hcounteren = 0;
	npriv->htimedelta = 0;
	npriv->htimedeltah = 0;
	npriv->htval = 0;
	npriv->htinst = 0;
	npriv->henvcfg = 0;
	npriv->henvcfgh = 0;
	npriv->hgatp = 0;
	npriv->vsstatus = 0;
	npriv->vsie = 0;
	npriv->vstvec = 0;
	npriv->vsscratch = 0;
	npriv->vsepc = 0;
	npriv->vscause = 0;
	npriv->vstval = 0;
	npriv->vsatp = 0;

	npriv->hvictl = 0;
}

void cpu_vcpu_nested_deinit(struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	nested_swtlb_deinit(vcpu);
	mmu_pgtbl_free(npriv->pgtbl);
	vmm_free(npriv->timer_event);
}

void cpu_vcpu_nested_dump_regs(struct vmm_chardev *cdev,
			       struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h))
		return;

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "    %s=%s\n",
		    "       virt", (npriv->virt) ? "on" : "off");
	vmm_cprintf(cdev, "\n");
#ifdef CONFIG_64BIT
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR"\n",
		    " htimedelta", npriv->htimedelta);
#else
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    " htimedelta", npriv->htimedelta,
		    "htimedeltah", npriv->htimedeltah);
#endif
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    hstatus", npriv->hstatus, "      hgatp", npriv->hgatp);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    hedeleg", npriv->hedeleg, "    hideleg", npriv->hideleg);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "       hvip", npriv->hvip, " hcounteren", npriv->hcounteren);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "      htval", npriv->htval, "     htinst", npriv->htinst);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "   vsstatus", npriv->vsstatus, "       vsie", npriv->vsie);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "      vsatp", npriv->vsatp, "     vstvec", npriv->vstvec);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "  vsscratch", npriv->vsscratch, "      vsepc", npriv->vsepc);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    vscause", npriv->vscause, "     vstval", npriv->vstval);

	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR"\n",
		    "     hvictl", npriv->hvictl);
}

int cpu_vcpu_nested_smode_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask)
{
	u64 tmp64;
	int csr_shift = 0;
	bool read_only = FALSE;
	unsigned long *csr, tmpcsr = 0, csr_rdor = 0;
	unsigned long zero = 0, writeable_mask = 0;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	riscv_stats_priv(vcpu)->nested_smode_csr_rmw++;

	/*
	 * These CSRs should never trap for virtual-HS/U modes because
	 * we only emulate these CSRs for virtual-VS/VU modes.
	 */
	if (!riscv_nested_virt(vcpu)) {
		return VMM_EINVALID;
	}

	/*
	 * Access of these CSRs from virtual-VU mode should be forwarded
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	switch (csr_num) {
	case CSR_SIE:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &npriv->vsie;
		writeable_mask = VSIE_WRITEABLE & (npriv->hideleg >> 1);
		break;
	case CSR_SIEH:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &zero;
		break;
	case CSR_SIP:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &npriv->hvip;
		csr_rdor = cpu_vcpu_timer_vs_irq(vcpu) ? HVIP_VSTIP : 0;
		csr_shift = 1;
		writeable_mask = HVIP_VSSIP & npriv->hideleg;
		break;
	case CSR_SIPH:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &zero;
		break;
	case CSR_STIMECMP:
		if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa,
						   SSTC)) {
			return TRAP_RETURN_ILLEGAL_INSN;
		}
#ifdef CONFIG_32BIT
		if (!(npriv->henvcfgh & ENVCFGH_STCE)) {
#else
		if (!(npriv->henvcfg & ENVCFG_STCE)) {
#endif
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		tmpcsr = cpu_vcpu_timer_vs_cycle(vcpu);
		csr = &tmpcsr;
		writeable_mask = -1UL;
		break;
#ifdef CONFIG_32BIT
	case CSR_STIMECMPH:
		if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa,
						   SSTC)) {
			return TRAP_RETURN_ILLEGAL_INSN;
		}
		if (!(npriv->henvcfgh & ENVCFGH_STCE)) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		tmpcsr = cpu_vcpu_timer_vs_cycle(vcpu) >> 32;
		csr = &tmpcsr;
		writeable_mask = -1UL;
		break;
#endif
	default:
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (val) {
		*val = (csr_shift < 0) ? (*csr | csr_rdor) << -csr_shift :
					 (*csr | csr_rdor) >> csr_shift;
	}

	if (read_only) {
		return TRAP_RETURN_ILLEGAL_INSN;
	} else if (wr_mask) {
		writeable_mask = (csr_shift < 0) ?
				  writeable_mask >> -csr_shift :
				  writeable_mask << csr_shift;
		wr_mask = (csr_shift < 0) ?
			   wr_mask >> -csr_shift : wr_mask << csr_shift;
		new_val = (csr_shift < 0) ?
			   new_val >> -csr_shift : new_val << csr_shift;
		wr_mask &= writeable_mask;
		*csr = (*csr & ~wr_mask) | (new_val & wr_mask);

		switch (csr_num) {
		case CSR_STIMECMP:
#ifdef CONFIG_32BIT
			tmp64 = cpu_vcpu_timer_vs_cycle(vcpu);
			tmp64 &= ~0xffffffffULL;
			tmp64 |= tmpcsr;
#else
			tmp64 = tmpcsr;
#endif
			cpu_vcpu_timer_vs_start(vcpu, tmp64);
			break;
#ifdef CONFIG_32BIT
		case CSR_STIMECMPH:
			tmp64 = cpu_vcpu_timer_vs_cycle(vcpu);
			tmp64 &= ~0xffffffff00000000ULL;
			tmp64 |= ((u64)tmpcsr) << 32;
			cpu_vcpu_timer_vs_start(vcpu, tmp64);
			break;
#endif
		default:
			break;
		}
	}

	return VMM_OK;
}

int cpu_vcpu_nested_hext_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask)
{
	u64 tmp64;
	int csr_shift = 0;
	bool read_only = FALSE, nuke_swtlb = FALSE;
	unsigned int csr_priv = (csr_num >> 8) & 0x3;
	unsigned long *csr, tmpcsr = 0, csr_rdor = 0;
	unsigned long mode, zero = 0, writeable_mask = 0;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	riscv_stats_priv(vcpu)->nested_hext_csr_rmw++;

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded
	 * to virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return (csr_priv == (PRV_S + 1)) ?
			TRAP_RETURN_VIRTUAL_INSN : TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * H-extension CSRs not allowed in virtual-U mode so forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	switch (csr_num) {
	case CSR_HSTATUS:
		csr = &npriv->hstatus;
		writeable_mask = HSTATUS_VTSR | HSTATUS_VTW | HSTATUS_VTVM |
				 HSTATUS_HU | HSTATUS_SPVP | HSTATUS_SPV |
				 HSTATUS_GVA;
		if (wr_mask & HSTATUS_SPV) {
			/*
			 * If hstatus.SPV == 1 then enable host SRET
			 * trapping for the virtual-HS mode which will
			 * allow host to do nested world-switch upon
			 * next SRET instruction executed by the
			 * virtual-HS-mode.
			 *
			 * If hstatus.SPV == 0 then disable host SRET
			 * trapping for the virtual-HS mode which will
			 * ensure that host does not do any nested
			 * world-switch for SRET instruction executed
			 * virtual-HS mode for general interrupt and
			 * trap handling.
			 */
			regs->hstatus &= ~HSTATUS_VTSR;
			regs->hstatus |= (new_val & HSTATUS_SPV) ?
					HSTATUS_VTSR : 0;
		}
		break;
	case CSR_HEDELEG:
		csr = &npriv->hedeleg;
		writeable_mask = HEDELEG_WRITEABLE;
		break;
	case CSR_HIDELEG:
		csr = &npriv->hideleg;
		writeable_mask = HIDELEG_WRITEABLE;
		break;
	case CSR_HVIP:
		csr = &npriv->hvip;
		writeable_mask = HVIP_WRITEABLE;
		break;
	case CSR_HIE:
		csr = &npriv->vsie;
		csr_shift = -1;
		writeable_mask = HVIP_WRITEABLE;
		break;
	case CSR_HIP:
		csr = &npriv->hvip;
		csr_rdor = cpu_vcpu_timer_vs_irq(vcpu) ? HVIP_VSTIP : 0;
		writeable_mask = HVIP_VSSIP;
		break;
	case CSR_HGEIP:
		csr = &zero;
		read_only = TRUE;
		break;
	case CSR_HGEIE:
		csr = &zero;
		break;
	case CSR_HCOUNTEREN:
		csr = &npriv->hcounteren;
		writeable_mask = HCOUNTEREN_WRITEABLE;
		break;
	case CSR_HTIMEDELTA:
		csr = &npriv->htimedelta;
		writeable_mask = -1UL;
		break;
#ifndef CONFIG_64BIT
	case CSR_HTIMEDELTAH:
		csr = &npriv->htimedeltah;
		writeable_mask = -1UL;
		break;
#endif
	case CSR_HTVAL:
		csr = &npriv->htval;
		writeable_mask = -1UL;
		break;
	case CSR_HTINST:
		csr = &npriv->htinst;
		writeable_mask = -1UL;
		break;
	case CSR_HGATP:
		csr = &npriv->hgatp;
		writeable_mask = HGATP_MODE | HGATP_VMID | HGATP_PPN;
		if (wr_mask & HGATP_MODE) {
			mode = (new_val & HGATP_MODE) >> HGATP_MODE_SHIFT;
			switch (mode) {
			/*
			 * We (intentionally) support only Sv39x4 on RV64
			 * and Sv32x4 on RV32 for guest G-stage so that
			 * software page table walks on guest G-stage is
			 * faster.
			 */
#ifdef CONFIG_64BIT
			case HGATP_MODE_SV39X4:
				if (riscv_stage2_mode != HGATP_MODE_SV57X4 &&
				    riscv_stage2_mode != HGATP_MODE_SV48X4 &&
				    riscv_stage2_mode != HGATP_MODE_SV39X4) {
					mode = HGATP_MODE_OFF;
				}
				break;
#else
			case HGATP_MODE_SV32X4:
				if (riscv_stage2_mode != HGATP_MODE_SV32X4) {
					mode = HGATP_MODE_OFF;
				}
				break;
#endif
			default:
				mode = HGATP_MODE_OFF;
				break;
			}
			new_val &= ~HGATP_MODE;
			new_val |= (mode << HGATP_MODE_SHIFT) & HGATP_MODE;
			if ((new_val ^ npriv->hgatp) & HGATP_MODE) {
				nuke_swtlb = TRUE;
			}
		}
		if (wr_mask & HGATP_VMID) {
			if ((new_val ^ npriv->hgatp) & HGATP_VMID) {
				nuke_swtlb = TRUE;
			}
		}
		break;
	case CSR_HENVCFG:
		csr = &npriv->henvcfg;
#ifdef CONFIG_32BIT
		writeable_mask = 0;
#else
		writeable_mask = ENVCFG_STCE;
#endif
		break;
#ifdef CONFIG_32BIT
	case CSR_HENVCFGH:
		csr = &npriv->henvcfgh;
		writeable_mask = ENVCFGH_STCE;
		break;
#endif
	case CSR_VSSTATUS:
		csr = &npriv->vsstatus;
		writeable_mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UBE |
				 SSTATUS_SPP | SSTATUS_SUM | SSTATUS_MXR |
				 SSTATUS_FS | SSTATUS_UXL;
		break;
	case CSR_VSIP:
		csr = &npriv->hvip;
		csr_rdor = cpu_vcpu_timer_vs_irq(vcpu) ? HVIP_VSTIP : 0;
		csr_shift = 1;
		writeable_mask = HVIP_VSSIP & npriv->hideleg;
		break;
	case CSR_VSIE:
		csr = &npriv->vsie;
		writeable_mask = VSIE_WRITEABLE & (npriv->hideleg >> 1);
		break;
	case CSR_VSTVEC:
		csr = &npriv->vstvec;
		writeable_mask = -1UL;
		break;
	case CSR_VSSCRATCH:
		csr = &npriv->vsscratch;
		writeable_mask = -1UL;
		break;
	case CSR_VSEPC:
		csr = &npriv->vsepc;
		writeable_mask = -1UL;
		break;
	case CSR_VSCAUSE:
		csr = &npriv->vscause;
		writeable_mask = 0x1fUL;
		break;
	case CSR_VSTVAL:
		csr = &npriv->vstval;
		writeable_mask = -1UL;
		break;
	case CSR_VSATP:
		csr = &npriv->vsatp;
		writeable_mask = SATP_MODE | SATP_ASID | SATP_PPN;
		if (wr_mask & SATP_MODE) {
			mode = (new_val & SATP_MODE) >> SATP_MODE_SHIFT;
			switch (mode) {
#ifdef CONFIG_64BIT
			case SATP_MODE_SV57:
				if (riscv_stage1_mode != SATP_MODE_SV57) {
					mode = SATP_MODE_OFF;
				}
				break;
			case SATP_MODE_SV48:
				if (riscv_stage1_mode != SATP_MODE_SV57 &&
				    riscv_stage1_mode != SATP_MODE_SV48) {
					mode = SATP_MODE_OFF;
				}
				break;
			case SATP_MODE_SV39:
				if (riscv_stage1_mode != SATP_MODE_SV57 &&
				    riscv_stage1_mode != SATP_MODE_SV48 &&
				    riscv_stage1_mode != SATP_MODE_SV39) {
					mode = SATP_MODE_OFF;
				}
				break;
#else
			case SATP_MODE_SV32:
				if (riscv_stage1_mode != SATP_MODE_SV32) {
					mode = SATP_MODE_OFF;
				}
				break;
#endif
			default:
				mode = SATP_MODE_OFF;
				break;
			}
			new_val &= ~SATP_MODE;
			new_val |= (mode << SATP_MODE_SHIFT) & SATP_MODE;
		}
		break;
	case CSR_VSTIMECMP:
		if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa,
						   SSTC)) {
			return TRAP_RETURN_ILLEGAL_INSN;
		}
		tmpcsr = cpu_vcpu_timer_vs_cycle(vcpu);
		csr = &tmpcsr;
		writeable_mask = -1UL;
		break;
#ifdef CONFIG_32BIT
	case CSR_VSTIMECMPH:
		if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa,
						   SSTC)) {
			return TRAP_RETURN_ILLEGAL_INSN;
		}
		tmpcsr = cpu_vcpu_timer_vs_cycle(vcpu) >> 32;
		csr = &tmpcsr;
		writeable_mask = -1UL;
		break;
#endif
	case CSR_HVICTL:
		csr = &npriv->hvictl;
		writeable_mask = HVICTL_VTI | HVICTL_IID |
				 HVICTL_IPRIOM | HVICTL_IPRIO;
		break;
	default:
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (val) {
		*val = (csr_shift < 0) ? (*csr | csr_rdor) << -csr_shift :
					 (*csr | csr_rdor) >> csr_shift;
	}

	if (read_only) {
		return TRAP_RETURN_ILLEGAL_INSN;
	} else if (wr_mask) {
		writeable_mask = (csr_shift < 0) ?
				  writeable_mask >> -csr_shift :
				  writeable_mask << csr_shift;
		wr_mask = (csr_shift < 0) ?
			   wr_mask >> -csr_shift : wr_mask << csr_shift;
		new_val = (csr_shift < 0) ?
			   new_val >> -csr_shift : new_val << csr_shift;
		wr_mask &= writeable_mask;
		*csr = (*csr & ~wr_mask) | (new_val & wr_mask);

		switch (csr_num) {
		case CSR_VSTIMECMP:
#ifdef CONFIG_32BIT
			tmp64 = cpu_vcpu_timer_vs_cycle(vcpu);
			tmp64 &= ~0xffffffffULL;
			tmp64 |= tmpcsr;
#else
			tmp64 = tmpcsr;
#endif
			cpu_vcpu_timer_vs_start(vcpu, tmp64);
			break;
#ifdef CONFIG_32BIT
		case CSR_VSTIMECMPH:
			tmp64 = cpu_vcpu_timer_vs_cycle(vcpu);
			tmp64 &= ~0xffffffff00000000ULL;
			tmp64 |= ((u64)tmpcsr) << 32;
			cpu_vcpu_timer_vs_start(vcpu, tmp64);
			break;
#endif
		case CSR_HTIMEDELTA:
			if (riscv_isa_extension_available(riscv_priv(vcpu)->isa,
							  SSTC)) {
				cpu_vcpu_timer_vs_restart(vcpu);
			}
			break;
#ifdef CONFIG_32BIT
		case CSR_HTIMEDELTAH:
			if (riscv_isa_extension_available(riscv_priv(vcpu)->isa,
							  SSTC)) {
				cpu_vcpu_timer_vs_restart(vcpu);
			}
			break;
#endif
		default:
			break;
		}
	}

	if (nuke_swtlb) {
		cpu_vcpu_nested_swtlb_flush(vcpu, 0, 0);
	}

	return VMM_OK;
}

int cpu_vcpu_nested_page_fault(struct vmm_vcpu *vcpu,
			       bool trap_from_smode,
			       const struct cpu_vcpu_trap *trap,
			       struct cpu_vcpu_trap *out_trap)
{
	int rc = VMM_OK;
	physical_addr_t guest_gpa;
	struct nested_xlate_context xc;
	enum nested_xlate_access guest_access;

	/*
	 * This function is called to handle guest page faults
	 * from virtual-VS/VU modes.
	 *
	 * We perform a guest gpa to host pa translation for
	 * the faulting guest gpa so that nested software TLB
	 * and shadow page table is updated.
	 */

	guest_gpa = ((physical_addr_t)trap->htval << 2);
	guest_gpa |= ((physical_addr_t)trap->stval & 0x3);
	switch (trap->scause) {
	case CAUSE_LOAD_GUEST_PAGE_FAULT:
		riscv_stats_priv(vcpu)->nested_load_guest_page_fault++;
		guest_access = NESTED_XLATE_LOAD;
		break;
	case CAUSE_STORE_GUEST_PAGE_FAULT:
		riscv_stats_priv(vcpu)->nested_store_guest_page_fault++;
		guest_access = NESTED_XLATE_STORE;
		break;
	case CAUSE_FETCH_GUEST_PAGE_FAULT:
		riscv_stats_priv(vcpu)->nested_fetch_guest_page_fault++;
		guest_access = NESTED_XLATE_FETCH;
		break;
	default:
		memcpy(out_trap, trap, sizeof(*out_trap));
		return VMM_OK;
	}

	nested_xlate_context_init(&xc, vcpu, guest_access, trap_from_smode,
				  csr_read(CSR_VSSTATUS), FALSE);
	rc = nested_xlate_gstage(&xc, guest_gpa, guest_access);
	if (rc == VMM_EFAULT) {
		out_trap->sepc = trap->sepc;
		out_trap->scause = xc.scause;
		out_trap->stval = trap->stval;
		out_trap->htval = xc.htval;
		out_trap->htinst = trap->htinst;
		rc = VMM_OK;
	}

	return rc;
}

void cpu_vcpu_nested_hfence_vvma(struct vmm_vcpu *vcpu,
				 unsigned long *vaddr, unsigned int *asid)
{
	unsigned long hgatp;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	riscv_stats_priv(vcpu)->nested_hfence_vvma++;

	/*
	 * The HFENCE.VVMA instructions help virtual-HS mode flush
	 * VS-stage TLB entries for virtual-VS/VU modes.
	 *
	 * When host G-stage VMID is not available, we flush all guest
	 * TLB (both G-stage and VS-stage) entries upon nested virt
	 * state change so the HFENCE.VVMA becomes a NOP in this case.
	 */

	if (!mmu_pgtbl_has_hw_tag(npriv->pgtbl)) {
		return;
	}

	hgatp = mmu_pgtbl_hw_tag(npriv->pgtbl);
	hgatp = csr_swap(CSR_HGATP, hgatp << HGATP_VMID_SHIFT);

	if (!vaddr && !asid) {
		__hfence_vvma_all();
	} else if (!vaddr && asid) {
		__hfence_vvma_asid(*asid);
	} else if (vaddr && !asid) {
		__hfence_vvma_va(*vaddr);
	} else {
		__hfence_vvma_asid_va(*vaddr, *asid);
	}

	csr_write(CSR_HGATP, hgatp);
}

void cpu_vcpu_nested_hfence_gvma(struct vmm_vcpu *vcpu,
				 physical_addr_t *gaddr, unsigned int *vmid)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	unsigned long current_vmid =
			(npriv->hgatp & HGATP_VMID) >> HGATP_VMID_SHIFT;

	riscv_stats_priv(vcpu)->nested_hfence_gvma++;

	/*
	 * The HFENCE.GVMA instructions help virtual-HS mode flush
	 * G-stage TLB entries for virtual-VS/VU modes.
	 *
	 * Irrespective whether host G-stage VMID is available or not,
	 * we flush all software TLB (only G-stage) entries upon guest
	 * hgatp.VMID change so the HFENCE.GVMA instruction becomes a
	 * NOP for virtual-HS mode when the current guest hgatp.VMID
	 * is different from the VMID specified in the HFENCE.GVMA
	 * instruction.
	 */

	if (vmid && current_vmid != *vmid) {
		return;
	}

	if (gaddr) {
		cpu_vcpu_nested_swtlb_flush(vcpu, *gaddr, 1);
	} else {
		cpu_vcpu_nested_swtlb_flush(vcpu, 0, 0);
	}
}

int cpu_vcpu_nested_hlv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			bool hlvx, void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval)
{
	int rc;
	physical_addr_t hpa;
	struct nested_xlate_context xc;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	riscv_stats_priv(vcpu)->nested_hlv++;

	/* Don't handle misaligned HLV */
	if (vaddr & (len - 1)) {
		*out_scause = CAUSE_MISALIGNED_LOAD;
		*out_stval = vaddr;
		*out_htval = 0;
		return VMM_OK;
	}

	nested_xlate_context_init(&xc, vcpu, NESTED_XLATE_LOAD,
			(npriv->hstatus & HSTATUS_SPVP) ? TRUE : FALSE,
			csr_read(CSR_VSSTATUS), hlvx);
	rc = nested_xlate_vsstage(&xc, vaddr, NESTED_XLATE_LOAD);
	if (rc) {
		if (rc == VMM_EFAULT) {
			*out_scause = xc.scause;
			*out_stval = xc.stval;
			*out_htval = xc.htval;
			return 0;
		}
		return rc;
	}

	hpa = xc.host_pa | (vaddr & (xc.host_sz - 1));
	if (vmm_host_memory_read(hpa, data, len, TRUE) != len) {
		return VMM_EIO;
	}

	return VMM_OK;
}

int cpu_vcpu_nested_hsv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			const void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval)
{
	int rc;
	physical_addr_t hpa;
	struct nested_xlate_context xc;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	riscv_stats_priv(vcpu)->nested_hsv++;

	/* Don't handle misaligned HSV */
	if (vaddr & (len - 1)) {
		*out_scause = CAUSE_MISALIGNED_STORE;
		*out_stval = vaddr;
		*out_htval = 0;
		return VMM_OK;
	}

	nested_xlate_context_init(&xc, vcpu, NESTED_XLATE_STORE,
			(npriv->hstatus & HSTATUS_SPVP) ? TRUE : FALSE,
			csr_read(CSR_VSSTATUS), FALSE);
	rc = nested_xlate_vsstage(&xc, vaddr, NESTED_XLATE_STORE);
	if (rc) {
		if (rc == VMM_EFAULT) {
			*out_scause = xc.scause;
			*out_stval = xc.stval;
			*out_htval = xc.htval;
			return 0;
		}
		return rc;
	}

	hpa = xc.host_pa | (vaddr & (xc.host_sz - 1));
	if (vmm_host_memory_write(hpa, (void *)data, len, TRUE) != len) {
		return VMM_EIO;
	}

	return VMM_OK;
}

void cpu_vcpu_nested_set_virt(struct vmm_vcpu *vcpu, struct arch_regs *regs,
			      enum nested_set_virt_event event, bool virt,
			      bool spvp, bool gva)
{
	unsigned long tmp;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	/* If H-extension is not available for VCPU then do nothing */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return;
	}

	/* Skip hardware CSR update if no change in virt state */
	if (virt == npriv->virt)
		goto skip_csr_update;

	/* Swap hcounteren and hedeleg CSRs */
	npriv->hcounteren = csr_swap(CSR_HCOUNTEREN, npriv->hcounteren);
	npriv->hedeleg = csr_swap(CSR_HEDELEG, npriv->hedeleg);

	/* Update environment configuration */
	cpu_vcpu_envcfg_update(vcpu, virt);

	/* Update interrupt delegation */
	cpu_vcpu_irq_deleg_update(vcpu, virt);

	/* Update time delta */
	cpu_vcpu_timer_delta_update(vcpu, virt);

	/* Update G-stage page table */
	cpu_vcpu_gstage_update(vcpu, virt);

	/* Swap hardware vs<xyz> CSRs except vsie and vsstatus */
	npriv->vstvec = csr_swap(CSR_VSTVEC, npriv->vstvec);
	npriv->vsscratch = csr_swap(CSR_VSSCRATCH, npriv->vsscratch);
	npriv->vsepc = csr_swap(CSR_VSEPC, npriv->vsepc);
	npriv->vscause = csr_swap(CSR_VSCAUSE, npriv->vscause);
	npriv->vstval = csr_swap(CSR_VSTVAL, npriv->vstval);
	npriv->vsatp = csr_swap(CSR_VSATP, npriv->vsatp);

	/* Update vsstatus CSR */
	if (virt) {
		/* Nested virtualization state changing from OFF to ON */
		riscv_stats_priv(vcpu)->nested_enter++;

		/*
		 * Update vsstatus in following manner:
		 * 1) Swap hardware vsstatus (i.e. virtual-HS mode sstatus)
		 *    with vsstatus in nested virtualization context (i.e.
		 *    virtual-VS mode sstatus)
		 * 2) Swap host sstatus.FS (i.e. HS mode sstatus.FS) with
		 *    the vsstatus.FS saved in nested virtualization context
		 *    (i.e. virtual-HS mode sstatus.FS)
		 */
		npriv->vsstatus = csr_swap(CSR_VSSTATUS, npriv->vsstatus);
		tmp = regs->sstatus & SSTATUS_FS;
		regs->sstatus &= ~SSTATUS_FS;
		regs->sstatus |= (npriv->vsstatus & SSTATUS_FS);
		npriv->vsstatus &= ~SSTATUS_FS;
		npriv->vsstatus |= tmp;
	} else {
		/* Nested virtualization state changing from ON to OFF */
		riscv_stats_priv(vcpu)->nested_exit++;

		/*
		 * Update vsstatus in following manner:
		 * 1) Swap host sstatus.FS (i.e. virtual-HS mode sstatus.FS)
		 *    with vsstatus.FS saved in the nested virtualization
		 *    context (i.e. HS mode sstatus.FS)
		 * 2) Swap hardware vsstatus (i.e. virtual-VS mode sstatus)
		 *    with vsstatus in nested virtualization context (i.e.
		 *    virtual-HS mode sstatus)
		 */
		tmp = regs->sstatus & SSTATUS_FS;
		regs->sstatus &= ~SSTATUS_FS;
		regs->sstatus |= (npriv->vsstatus & SSTATUS_FS);
		npriv->vsstatus &= ~SSTATUS_FS;
		npriv->vsstatus |= tmp;
		npriv->vsstatus = csr_swap(CSR_VSSTATUS, npriv->vsstatus);
	}

skip_csr_update:
	if (event != NESTED_SET_VIRT_EVENT_SRET) {
		/* Update Guest hstatus.SPV bit */
		npriv->hstatus &= ~HSTATUS_SPV;
		npriv->hstatus |= (npriv->virt) ? HSTATUS_SPV : 0;

		/* Update Guest hstatus.SPVP bit */
		if (npriv->virt) {
			npriv->hstatus &= ~HSTATUS_SPVP;
			if (spvp)
				npriv->hstatus |= HSTATUS_SPVP;
		}

		/* Update Guest hstatus.GVA bit */
		if (event == NESTED_SET_VIRT_EVENT_TRAP) {
			npriv->hstatus &= ~HSTATUS_GVA;
			npriv->hstatus |= (gva) ? HSTATUS_GVA : 0;
		}
	}

	/* Update host SRET trapping */
	regs->hstatus &= ~HSTATUS_VTSR;
	if (virt) {
		if (npriv->hstatus & HSTATUS_VTSR) {
			regs->hstatus |= HSTATUS_VTSR;
		}
	} else {
		if (npriv->hstatus & HSTATUS_SPV) {
			regs->hstatus |= HSTATUS_VTSR;
		}
	}

	/* Update host VM trapping */
	regs->hstatus &= ~HSTATUS_VTVM;
	if (virt && (npriv->hstatus & HSTATUS_VTVM)) {
		regs->hstatus |= HSTATUS_VTVM;
	}

	/* Update virt flag */
	npriv->virt = virt;
}

void cpu_vcpu_nested_take_vsirq(struct vmm_vcpu *vcpu,
				struct arch_regs *regs)
{
	int vsirq;
	bool next_spp;
	unsigned long irqs;
	struct cpu_vcpu_trap trap;
	struct riscv_priv_nested *npriv;

	/* Do nothing for Orphan VCPUs */
	if (!vcpu->is_normal) {
		return;
	}

	/* Do nothing if virt state is OFF */
	npriv = riscv_nested_priv(vcpu);
	if (!npriv->virt) {
		return;
	}

	/* Determine virtual-VS mode interrupt number */
	vsirq = 0;
	irqs = npriv->hvip | (cpu_vcpu_timer_vs_irq(vcpu) ? HVIP_VSTIP : 0);
	irqs &= npriv->vsie << 1;
	irqs &= npriv->hideleg;
	if (irqs & MIP_VSEIP) {
		vsirq = IRQ_S_EXT;
	} else if (irqs & MIP_VSTIP) {
		vsirq = IRQ_S_TIMER;
	} else if (irqs & MIP_VSSIP) {
		vsirq = IRQ_S_SOFT;
	}
	if (vsirq <= 0) {
		return;
	}

	/*
	 * Determine whether we are resuming in virtual-VS mode
	 * or virtual-VU mode
	 */
	next_spp = (regs->sstatus & SSTATUS_SPP) ? TRUE : FALSE;

	/*
	 * If we going to virtual-VS mode and interrupts are disabled
	 * then start nested timer event.
	 */
	if (next_spp && !(csr_read(CSR_VSSTATUS) & SSTATUS_SIE)) {
		if (!vmm_timer_event_pending(npriv->timer_event)) {
			vmm_timer_event_start(npriv->timer_event, 10000000);
		}
		return;
	}
	vmm_timer_event_stop(npriv->timer_event);

	riscv_stats_priv(vcpu)->nested_vsirq++;

	/* Take virtual-VS mode interrupt */
	trap.scause = SCAUSE_INTERRUPT_MASK | vsirq;
	trap.sepc = regs->sepc;
	trap.stval = 0;
	trap.htval = 0;
	trap.htinst = 0;
	cpu_vcpu_redirect_smode_trap(regs, &trap, next_spp);

	return;
}
