 /**
 * Copyright (c) 2017 Bhargav Shah.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @file arm-smmu.c
 * @author Bhargav Shah (bhargavshah1988@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IOMMU driver for ARM SMMU v1/v2
 *
 * The source has been adapted from Linux
 * drivers/iommu/arm-smmu.c
 *
 * Copyright (C) 2013 ARM Limited
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_macros.h>
#include <vmm_delay.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_mutex.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_platform.h>
#include <vmm_iommu.h>
#include <vmm_devtree.h>
#include <arch_atomic.h>
#include <libs/list.h>
#include <libs/bitmap.h>
#include <libs/mathlib.h>

#include "arm-smmu-regs.h"
#include "io-pgtable.h"

#define ARM_MMU500_ACTLR_CPRE		(1 << 1)

#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)
#define ARM_MMU500_ACR_S2CRB_TLBEN	(1 << 10)
#define ARM_MMU500_ACR_SMTNMB_TLBEN	(1 << 8)

#define TLB_LOOP_TIMEOUT		1000000	/* 1s! */
#define TLB_SPIN_COUNT			10

/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128

/* SMMU global address space */
#define ARM_SMMU_GR0(smmu)		((smmu)->base)
#define ARM_SMMU_GR1(smmu)		((smmu)->base + (1 << (smmu)->pgshift))

/*
 * SMMU global address space with conditional offset to access secure
 * aliases of non-secure registers (e.g. nsCR0: 0x400, nsGFSR: 0x448,
 * nsGFSYNR0: 0x450)
 */
#define ARM_SMMU_GR0_NS(smmu)						\
	((smmu)->base +							\
		((smmu->options & ARM_SMMU_OPT_SECURE_CFG_ACCESS)	\
			? 0x400 : 0))

/*
 * Some 64-bit registers only make sense to write atomically, but in such
 * cases all the data relevant to AArch32 formats lies within the lower word,
 * therefore this actually makes more sense than it might first appear.
 */
#ifdef CONFIG_64BIT
#define smmu_write_atomic_lq		vmm_writeq_relaxed
#else
#define smmu_write_atomic_lq		vmm_writel_relaxed
#endif

/* Translation context bank */
#define ARM_SMMU_CB(smmu, n)		\
	((smmu)->cb_base + ((n) << (smmu)->pgshift))

enum arm_smmu_arch_version {
	ARM_SMMU_V1,
	ARM_SMMU_V1_64K,
	ARM_SMMU_V2,
};

enum arm_smmu_implementation {
	GENERIC_SMMU,
	ARM_MMU500,
	CAVIUM_SMMUV2,
};

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_s2cr {
	int				count;
	enum arm_smmu_s2cr_type		type;
	enum arm_smmu_s2cr_privcfg	privcfg;
	u8				cbndx;
};

#define s2cr_init_val (struct arm_smmu_s2cr){				\
	.type = disable_bypass ? S2CR_TYPE_FAULT : S2CR_TYPE_BYPASS,	\
}

struct arm_smmu_cb {
	u64				ttbr[2];
	u32				tcr[2];
	u32				mair[2];
	struct arm_smmu_cfg		*cfg;
};

struct arm_smmu_device {
	struct list_head list;
	struct vmm_devtree_node *node;

#define ARM_SMMU_FEAT_COHERENT_WALK	(1 << 0)
#define ARM_SMMU_FEAT_STREAM_MATCH	(1 << 1)
#define ARM_SMMU_FEAT_TRANS_S1		(1 << 2)
#define ARM_SMMU_FEAT_TRANS_S2		(1 << 3)
#define ARM_SMMU_FEAT_TRANS_NESTED	(1 << 4)
#define ARM_SMMU_FEAT_TRANS_OPS		(1 << 5)
#define ARM_SMMU_FEAT_VMID16		(1 << 6)
#define ARM_SMMU_FEAT_FMT_AARCH64_4K	(1 << 7)
#define ARM_SMMU_FEAT_FMT_AARCH64_16K	(1 << 8)
#define ARM_SMMU_FEAT_FMT_AARCH64_64K	(1 << 9)
#define ARM_SMMU_FEAT_FMT_AARCH32_L	(1 << 10)
#define ARM_SMMU_FEAT_FMT_AARCH32_S	(1 << 11)
#define ARM_SMMU_FEAT_EXIDS		(1 << 12)
	u32				features;

#define ARM_SMMU_OPT_SECURE_CFG_ACCESS (1 << 0)
	u32				options;
	enum arm_smmu_arch_version	version;
	enum arm_smmu_implementation	model;

	void				*base;
	physical_addr_t			reg_pa;
	physical_size_t			reg_size;
	void				*cb_base;

	u32				num_global_irqs;
	u32				num_context_irqs;
	u32				*irqs;

	u16				num_mapping_groups;
	u16				streamid_mask;
	u16				smr_mask_mask;
	struct arm_smmu_smr		*smrs;
	struct arm_smmu_s2cr		*s2crs;
	struct vmm_mutex		stream_map_mutex;

	u32				num_s2_context_banks;
	u32				num_context_banks;
	u32				pgshift;
	DECLARE_BITMAP(context_map, ARM_SMMU_MAX_CBS);
	struct arm_smmu_cb		*cbs;
	atomic_t			irptndx;

	u32				cavium_id_base; /* Specific to Cavium */

	unsigned long			ipa_size;
	unsigned long			pa_size;
	unsigned long			va_size;
	unsigned long			pgsize_bitmap;

	vmm_spinlock_t			global_sync_lock;

	struct vmm_iommu_controller	controller;
};

enum arm_smmu_context_fmt {
	ARM_SMMU_CTX_FMT_NONE,
	ARM_SMMU_CTX_FMT_AARCH64,
	ARM_SMMU_CTX_FMT_AARCH32_L,
	ARM_SMMU_CTX_FMT_AARCH32_S,
};

struct arm_smmu_cfg {
	u8				cbndx;
	u8				irptndx;
	union {
		u16			asid;
		u16			vmid;
	};
	u32				cbar;
	enum arm_smmu_context_fmt	fmt;
};
#define INVALID_IRPTNDX			0xff

enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
	ARM_SMMU_DOMAIN_NESTED,
	ARM_SMMU_DOMAIN_BYPASS,
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct io_pgtable_ops		*pgtbl_ops;
	const struct iommu_gather_ops	*tlb_ops;
	struct arm_smmu_cfg		cfg;
	enum arm_smmu_domain_stage	stage;
	struct vmm_mutex		init_mutex; /* Protects smmu pointer */
	vmm_spinlock_t			cb_lock; /* Serialises ATS1* ops and TLB syncs */
	struct vmm_iommu_domain		domain;
};

struct arm_smmu_sid {
	unsigned int sid;
	unsigned int mask;
	int sme;
};

struct arm_smmu_archdata {
	struct arm_smmu_device *smmu;
	struct arm_smmu_sid *sids;
	unsigned int num_sid;

	/* io_xxx only updated at time of attaching device */
	struct vmm_device *io_dev;
	struct vmm_iommu_domain *io_domain;
};

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static atomic_t cavium_smmu_context_count = ARCH_ATOMIC_INITIALIZER(0);
static DEFINE_SPINLOCK(smmu_devices_lock);
static LIST_HEAD(smmu_devices);
static bool disable_bypass = FALSE;

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_SECURE_CFG_ACCESS, "calxeda,smmu-secure-config-access" },
	{ 0, NULL},
};

static struct arm_smmu_domain *to_smmu_domain(struct vmm_iommu_domain *dom)
{
	return container_of(dom, struct arm_smmu_domain, domain);
}

static struct arm_smmu_device *to_smmu_device(struct vmm_iommu_controller *ct)
{
	return container_of(ct, struct arm_smmu_device, controller);
}

static void parse_driver_options(struct arm_smmu_device *smmu)
{
	int i = 0;

	do {
		if (vmm_devtree_getattr(smmu->node, arm_smmu_options[i].prop)) {
			smmu->options |= arm_smmu_options[i].opt;
			vmm_linfo(smmu->node->name, "arm-smmu: option %s\n",
				  arm_smmu_options[i].prop);
		}
	} while (arm_smmu_options[++i].opt);
}

static void arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = smr->id << SMR_ID_SHIFT | smr->mask << SMR_MASK_SHIFT;

	if (!(smmu->features & ARM_SMMU_FEAT_EXIDS) && smr->valid)
		reg |= SMR_VALID;
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
}

static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg = (s2cr->type & S2CR_TYPE_MASK) << S2CR_TYPE_SHIFT |
		  (s2cr->cbndx & S2CR_CBNDX_MASK) << S2CR_CBNDX_SHIFT |
		  (s2cr->privcfg & S2CR_PRIVCFG_MASK) << S2CR_PRIVCFG_SHIFT;

	if (smmu->features & ARM_SMMU_FEAT_EXIDS && smmu->smrs &&
	    smmu->smrs[idx].valid)
		reg |= S2CR_EXIDVALID;
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));
}

static void arm_smmu_write_sme(struct arm_smmu_device *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	if (smmu->smrs)
		arm_smmu_write_smr(smmu, idx);
}

/*
 * The width of SMR's mask field depends on sCR0_EXIDENABLE, so this function
 * should be called after sCR0 is written.
 */
static void arm_smmu_test_smr_masks(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	u32 smr;

	if (!smmu->smrs)
		return;

	/*
	 * SMR.ID bits may not be preserved if the corresponding MASK
	 * bits are set, so check each one separately. We can reject
	 * masters later if they try to claim IDs outside these masks.
	 */
	smr = smmu->streamid_mask << SMR_ID_SHIFT;
	vmm_writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(0));
	smr = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(0));
	smmu->streamid_mask = smr >> SMR_ID_SHIFT;

	smr = smmu->streamid_mask << SMR_MASK_SHIFT;
	vmm_writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(0));
	smr = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(0));
	smmu->smr_mask_mask = smr >> SMR_MASK_SHIFT;
}

static void arm_smmu_reset_sme(struct arm_smmu_device *smmu, int idx)
{
	u32 reg;
	reg = (0x3 << S2CR_TYPE_SHIFT);
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));

	reg = 0x00;
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
}

static int arm_smmu_find_sme(struct arm_smmu_device *smmu, u16 id, u16 mask)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, free_idx = -1;

	/* Stream indexing is blissfully easy */
	if (!smrs)
		return id;

	/* Validating SMRs is... less so */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		if (!smrs[i].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = i;
			continue;
		}

		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((mask & smrs[i].mask) == mask &&
		    !((id ^ smrs[i].id) & ~smrs[i].mask))
			return i;

		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[i].id) & ~(smrs[i].mask | mask)))
			return VMM_EINVALID;
	}

	return free_idx;
}

static bool arm_smmu_free_sme(struct arm_smmu_device *smmu, int idx)
{
	if (--smmu->s2crs[idx].count)
		return FALSE;

	smmu->s2crs[idx] = s2cr_init_val;
	if (smmu->smrs)
		smmu->smrs[idx].valid = 0;

	return TRUE;
}

static void arm_smmu_master_free_smes(struct arm_smmu_archdata *archdata)
{
	struct arm_smmu_device *smmu = archdata->smmu;
	int i;

	vmm_mutex_lock(&smmu->stream_map_mutex);
	for (i = 0; i < archdata->num_sid; i++) {
		if ((archdata->sids[i].sme > -1) &&
		    arm_smmu_free_sme(smmu, archdata->sids[i].sme)) {
			arm_smmu_write_sme(smmu, archdata->sids[i].sme);
			archdata->sids[i].sme = -1;
		}
	}
	vmm_mutex_unlock(&smmu->stream_map_mutex);
}

static int __arm_smmu_alloc_bitmap(unsigned long *map, int start, int end)
{
	int idx;

	do {
		idx = find_next_zero_bit(map, end, start);
		if (idx == end)
			return VMM_ENOSPC;
	} while (test_and_set_bit(idx, map));

	return idx;
}

static void __arm_smmu_free_bitmap(unsigned long *map, int idx)
{
	clear_bit(idx, map);
}

/* Wait for any pending TLB invalidations to complete */
static void __arm_smmu_tlb_sync(struct arm_smmu_device *smmu,
				void *sync, void *status)
{
	unsigned int spin_cnt, delay;

	vmm_writel_relaxed(0, sync);
	for (delay = 1; delay < TLB_LOOP_TIMEOUT; delay *= 2) {
		for (spin_cnt = TLB_SPIN_COUNT; spin_cnt > 0; spin_cnt--) {
			if (!(vmm_readl_relaxed(status) & sTLBGSTATUS_GSACTIVE))
				return;
			arch_cpu_relax();
		}
		vmm_udelay(delay);
	}
	vmm_lerror(smmu->node->name,
		   "TLB sync timed out -- SMMU may be deadlocked\n");
}

static void arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	void *base = ARM_SMMU_GR0(smmu);
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&smmu->global_sync_lock, flags);
	__arm_smmu_tlb_sync(smmu, base + ARM_SMMU_GR0_sTLBGSYNC,
			    base + ARM_SMMU_GR0_sTLBGSTATUS);
	vmm_spin_unlock_irqrestore(&smmu->global_sync_lock, flags);
}

static void arm_smmu_tlb_sync_context(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void *base = ARM_SMMU_CB(smmu, smmu_domain->cfg.cbndx);
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	__arm_smmu_tlb_sync(smmu, base + ARM_SMMU_CB_TLBSYNC,
			    base + ARM_SMMU_CB_TLBSTATUS);
	vmm_spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
}

static void arm_smmu_tlb_sync_vmid(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	arm_smmu_tlb_sync_global(smmu_domain->smmu);
}

static void arm_smmu_tlb_inv_context_s1(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void *base = ARM_SMMU_CB(smmu_domain->smmu, cfg->cbndx);

	vmm_writel_relaxed(cfg->asid, base + ARM_SMMU_CB_S1_TLBIASID);
	arm_smmu_tlb_sync_context(cookie);
}

static void arm_smmu_tlb_inv_context_s2(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void *base = ARM_SMMU_GR0(smmu);

	vmm_writel_relaxed(smmu_domain->cfg.vmid, base + ARM_SMMU_GR0_TLBIVMID);
	arm_smmu_tlb_sync_global(smmu);
}

static void arm_smmu_tlb_inv_range_nosync(unsigned long iova, size_t size,
					  size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	void *reg = ARM_SMMU_CB(smmu_domain->smmu, cfg->cbndx);

	if (stage1) {
		reg += leaf ? ARM_SMMU_CB_S1_TLBIVAL : ARM_SMMU_CB_S1_TLBIVA;

		if (cfg->fmt != ARM_SMMU_CTX_FMT_AARCH64) {
			iova &= ~12UL;
			iova |= cfg->asid;
			do {
				vmm_writel_relaxed(iova, reg);
				iova += granule;
			} while (size -= granule);
		} else {
			iova >>= 12;
			iova |= (u64)cfg->asid << 48;
			do {
				vmm_writeq_relaxed(iova, reg);
				iova += granule >> 12;
			} while (size -= granule);
		}
	} else {
		reg += leaf ? ARM_SMMU_CB_S2_TLBIIPAS2L :
			      ARM_SMMU_CB_S2_TLBIIPAS2;
		iova >>= 12;
		do {
			smmu_write_atomic_lq(iova, reg);
			iova += granule >> 12;
		} while (size -= granule);
	}
}

/*
 * On MMU-401 at least, the cost of firing off multiple TLBIVMIDs appears
 * almost negligible, but the benefit of getting the first one in as far ahead
 * of the sync as possible is significant, hence we don't just make this a
 * no-op and set .tlb_sync to arm_smmu_inv_context_s2() as you might think.
 */
static void arm_smmu_tlb_inv_vmid_nosync(unsigned long iova, size_t size,
					 size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	void *base = ARM_SMMU_GR0(smmu_domain->smmu);

	vmm_writel_relaxed(smmu_domain->cfg.vmid, base + ARM_SMMU_GR0_TLBIVMID);
}

static const struct iommu_gather_ops arm_smmu_s1_tlb_ops = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s1,
	.tlb_add_flush	= arm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_context,
};

static const struct iommu_gather_ops arm_smmu_s2_tlb_ops_v2 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_add_flush	= arm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_context,
};

static const struct iommu_gather_ops arm_smmu_s2_tlb_ops_v1 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_add_flush	= arm_smmu_tlb_inv_vmid_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_vmid,
};

static vmm_irq_return_t arm_smmu_context_fault(int irq, void *dev)
{
	u32 fsr, fsynr;
	unsigned long iova;
	struct vmm_iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void *cb_base;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	fsr = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT))
		return VMM_IRQ_NONE;

	fsynr = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR0);
	iova = vmm_readq_relaxed(cb_base + ARM_SMMU_CB_FAR);

	vmm_lerror(smmu->node->name,
	"Unhandled context fault: fsr=0x%x, iova=0x%08lx, fsynr=0x%x, cb=%d\n",
			    fsr, iova, fsynr, cfg->cbndx);

	vmm_writel(fsr, cb_base + ARM_SMMU_CB_FSR);

	return VMM_IRQ_HANDLED;
}

static vmm_irq_return_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;
	void *gr0_base = ARM_SMMU_GR0_NS(smmu);

	gfsr = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSR);
	gfsynr0 = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr)
		return VMM_IRQ_NONE;

	vmm_lerror_once(smmu->node->name,
		"Unexpected global fault, this could be serious\n");
	vmm_lerror_once(smmu->node->name,
		"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	vmm_writel(gfsr, gr0_base + ARM_SMMU_GR0_sGFSR);

	return VMM_IRQ_HANDLED;
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct io_pgtable_cfg *pgtbl_cfg)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_cb *cb = &smmu_domain->smmu->cbs[cfg->cbndx];
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	cb->cfg = cfg;

	/* TTBCR */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->tcr[0] = pgtbl_cfg->arm_v7s_cfg.tcr;
		} else {
			cb->tcr[0] = pgtbl_cfg->arm_lpae_s1_cfg.tcr;
			cb->tcr[1] = pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32;
			cb->tcr[1] |= TTBCR2_SEP_UPSTREAM;
			if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
				cb->tcr[1] |= TTBCR2_AS;
		}
	} else {
		cb->tcr[0] = pgtbl_cfg->arm_lpae_s2_cfg.vtcr;
	}

	/* TTBRs */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->ttbr[0] = pgtbl_cfg->arm_v7s_cfg.ttbr[0];
			cb->ttbr[1] = pgtbl_cfg->arm_v7s_cfg.ttbr[1];
		} else {
			cb->ttbr[0] = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
			cb->ttbr[0] |= (u64)cfg->asid << TTBRn_ASID_SHIFT;
			cb->ttbr[1] = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1];
			cb->ttbr[1] |= (u64)cfg->asid << TTBRn_ASID_SHIFT;
		}
	} else {
		cb->ttbr[0] = pgtbl_cfg->arm_lpae_s2_cfg.vttbr;
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->mair[0] = pgtbl_cfg->arm_v7s_cfg.prrr;
			cb->mair[1] = pgtbl_cfg->arm_v7s_cfg.nmrr;
		} else {
			cb->mair[0] = pgtbl_cfg->arm_lpae_s1_cfg.mair[0];
			cb->mair[1] = pgtbl_cfg->arm_lpae_s1_cfg.mair[1];
		}
	}
}

static void arm_smmu_write_context_bank(struct arm_smmu_device *smmu, int idx)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	struct arm_smmu_cfg *cfg = cb->cfg;
	void *cb_base, *gr1_base;

	cb_base = ARM_SMMU_CB(smmu, idx);

	/* Unassigned context banks only need disabling */
	if (!cfg) {
		vmm_writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
		return;
	}

	gr1_base = ARM_SMMU_GR1(smmu);
	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	/* CBA2R */
	if (smmu->version > ARM_SMMU_V1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
			reg = CBA2R_RW64_64BIT;
		else
			reg = CBA2R_RW64_32BIT;
		/* 16-bit VMIDs live in CBA2R */
		if (smmu->features & ARM_SMMU_FEAT_VMID16)
			reg |= cfg->vmid << CBA2R_VMID_SHIFT;

		vmm_writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBA2R(idx));
	}

	/* CBAR */
	reg = cfg->cbar;
	if (smmu->version < ARM_SMMU_V2)
		reg |= cfg->irptndx << CBAR_IRPTNDX_SHIFT;

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= (CBAR_S1_BPSHCFG_NSH << CBAR_S1_BPSHCFG_SHIFT) |
			(CBAR_S1_MEMATTR_WB << CBAR_S1_MEMATTR_SHIFT);
	} else if (!(smmu->features & ARM_SMMU_FEAT_VMID16)) {
		/* 8-bit VMIDs live in CBAR */
		reg |= cfg->vmid << CBAR_VMID_SHIFT;
	}
	vmm_writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBAR(idx));

	/*
	 * TTBCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	if (stage1 && smmu->version > ARM_SMMU_V1)
		vmm_writel_relaxed(cb->tcr[1], cb_base + ARM_SMMU_CB_TTBCR2);
	vmm_writel_relaxed(cb->tcr[0], cb_base + ARM_SMMU_CB_TTBCR);

	/* TTBRs */
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		vmm_writel_relaxed(cfg->asid, cb_base + ARM_SMMU_CB_CONTEXTIDR);
		vmm_writel_relaxed(cb->ttbr[0], cb_base + ARM_SMMU_CB_TTBR0);
		vmm_writel_relaxed(cb->ttbr[1], cb_base + ARM_SMMU_CB_TTBR1);
	} else {
		vmm_writeq_relaxed(cb->ttbr[0], cb_base + ARM_SMMU_CB_TTBR0);
		if (stage1)
			vmm_writeq_relaxed(cb->ttbr[1], cb_base + ARM_SMMU_CB_TTBR1);
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		vmm_writel_relaxed(cb->mair[0], cb_base + ARM_SMMU_CB_S1_MAIR0);
		vmm_writel_relaxed(cb->mair[1], cb_base + ARM_SMMU_CB_S1_MAIR1);
	}

	/* SCTLR */
	reg = SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE | SCTLR_M;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
	if (IS_ENABLED(CONFIG_CPU_BE))
		reg |= SCTLR_E;

	vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
}

static int arm_smmu_init_domain_context(struct vmm_iommu_domain *domain,
					struct arm_smmu_device *smmu)
{
	u32 irq;
	int start, ret = 0;
	unsigned long ias, oas;
	struct io_pgtable_ops *pgtbl_ops;
	struct io_pgtable_cfg pgtbl_cfg;
	enum io_pgtable_fmt fmt;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;

	vmm_mutex_lock(&smmu_domain->init_mutex);
	if (smmu_domain->smmu)
		goto out_unlock;

	if (domain->type == VMM_IOMMU_DOMAIN_IDENTITY) {
		smmu_domain->stage = ARM_SMMU_DOMAIN_BYPASS;
		smmu_domain->smmu = smmu;
		goto out_unlock;
	}

	/*
	 * Mapping the requested stage onto what we support is surprisingly
	 * complicated, mainly because the spec allows S1+S2 SMMUs without
	 * support for nested translation. That means we end up with the
	 * following table:
	 *
	 * Requested        Supported        Actual
	 *     S1               N              S1
	 *     S1             S1+S2            S1
	 *     S1               S2             S2
	 *     S1               S1             S1
	 *     N                N              N
	 *     N              S1+S2            S2
	 *     N                S2             S2
	 *     N                S1             S1
	 *
	 * Note that you can't actually request stage-2 mappings.
	 */
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S1))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S2;
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S2))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

	/*
	 * Choosing a suitable context format is even more fiddly. Until we
	 * grow some way for the caller to express a preference, and/or move
	 * the decision into the io-pgtable code where it arguably belongs,
	 * just aim for the closest thing to the rest of the system, and hope
	 * that the hardware isn't esoteric enough that we can't assume AArch64
	 * support to be a superset of AArch32 support...
	 */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_L)
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_L;
	if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_ARMV7S) &&
	    !IS_ENABLED(CONFIG_64BIT) && !IS_ENABLED(CONFIG_ARM_LPAE) &&
	    (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S) &&
	    (smmu_domain->stage == ARM_SMMU_DOMAIN_S1))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_S;
	if ((IS_ENABLED(CONFIG_64BIT) || cfg->fmt == ARM_SMMU_CTX_FMT_NONE) &&
	    (smmu->features & (ARM_SMMU_FEAT_FMT_AARCH64_64K |
			       ARM_SMMU_FEAT_FMT_AARCH64_16K |
			       ARM_SMMU_FEAT_FMT_AARCH64_4K)))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH64;

	if (cfg->fmt == ARM_SMMU_CTX_FMT_NONE) {
		ret = VMM_EINVALID;
		goto out_unlock;
	}

	switch (smmu_domain->stage) {
	case ARM_SMMU_DOMAIN_S1:
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
		ias = smmu->va_size;
		oas = smmu->ipa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = ARM_64_LPAE_S1;
		} else if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_L) {
			fmt = ARM_32_LPAE_S1;
			ias = min(ias, 32UL);
			oas = min(oas, 40UL);
		} else {
			fmt = ARM_V7S;
			ias = min(ias, 32UL);
			oas = min(oas, 32UL);
		}
		smmu_domain->tlb_ops = &arm_smmu_s1_tlb_ops;
		break;
	case ARM_SMMU_DOMAIN_NESTED:
		/*
		 * We will likely want to change this if/when KVM gets
		 * involved.
		 */
	case ARM_SMMU_DOMAIN_S2:
		cfg->cbar = CBAR_TYPE_S2_TRANS;
		start = 0;
		ias = smmu->ipa_size;
		oas = smmu->pa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = ARM_64_LPAE_S2;
		} else {
			fmt = ARM_32_LPAE_S2;
			ias = min(ias, 40UL);
			oas = min(oas, 40UL);
		}
		if (smmu->version == ARM_SMMU_V2)
			smmu_domain->tlb_ops = &arm_smmu_s2_tlb_ops_v2;
		else
			smmu_domain->tlb_ops = &arm_smmu_s2_tlb_ops_v1;
		break;
	default:
		ret = VMM_EINVALID;
		goto out_unlock;
	}
	ret = __arm_smmu_alloc_bitmap(smmu->context_map, start,
				      smmu->num_context_banks);
	if (ret < 0)
		goto out_unlock;

	cfg->cbndx = ret;
	if (smmu->version < ARM_SMMU_V2) {
		cfg->irptndx = arch_atomic_add_return(&smmu->irptndx, 1);
		cfg->irptndx = umod32(cfg->irptndx, smmu->num_context_irqs);
	} else {
		cfg->irptndx = cfg->cbndx;
	}

	if (smmu_domain->stage == ARM_SMMU_DOMAIN_S2)
		cfg->vmid = cfg->cbndx + 1 + smmu->cavium_id_base;
	else
		cfg->asid = cfg->cbndx + smmu->cavium_id_base;

	pgtbl_cfg = (struct io_pgtable_cfg) {
		.pgsize_bitmap	= smmu->pgsize_bitmap,
		.ias		= ias,
		.oas		= oas,
		.tlb		= smmu_domain->tlb_ops,
	};

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		pgtbl_cfg.quirks = IO_PGTABLE_QUIRK_NO_DMA;

	smmu_domain->smmu = smmu;
	pgtbl_ops = alloc_io_pgtable_ops(fmt, &pgtbl_cfg, smmu_domain);
	if (!pgtbl_ops) {
		ret = VMM_ENOMEM;
		goto out_clear_smmu;
	}

	/* Update the domain's page sizes to reflect the page table format */
	domain->geometry.aperture_end = (1UL << ias) - 1;
	domain->geometry.force_aperture = true;

	/* Initialise the context bank with our page table cfg */
	arm_smmu_init_context_bank(smmu_domain, &pgtbl_cfg);
	arm_smmu_write_context_bank(smmu, cfg->cbndx);

	/*
	 * Request context fault interrupt. Do this last to avoid the
	 * handler seeing a half-initialised domain state.
	 */
	irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
	ret = vmm_host_irq_register(irq, "arm-smmu-context-fault",
				    arm_smmu_context_fault, domain);
	if (ret < 0) {
		vmm_lerror(smmu->node->name,
			   "failed to request context IRQ %d (%u)\n",
			   cfg->irptndx, irq);
		cfg->irptndx = INVALID_IRPTNDX;
	}

	vmm_mutex_unlock(&smmu_domain->init_mutex);

	/* Publish page table ops for map/unmap */
	smmu_domain->pgtbl_ops = pgtbl_ops;
	return 0;

out_clear_smmu:
	smmu_domain->smmu = NULL;
out_unlock:
	vmm_mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static void arm_smmu_destroy_domain_context(struct vmm_iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	u32 irq;

	if (!smmu || domain->type == VMM_IOMMU_DOMAIN_IDENTITY)
		return;

	/*
	 * Disable the context bank and free the page tables before freeing
	 * it.
	 */
	smmu->cbs[cfg->cbndx].cfg = NULL;
	arm_smmu_write_context_bank(smmu, cfg->cbndx);

	if (cfg->irptndx != INVALID_IRPTNDX) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		vmm_host_irq_unregister(irq, domain);
	}

	free_io_pgtable_ops(smmu_domain->pgtbl_ops);
	__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);
}

static int arm_smmu_find_sids(struct arm_smmu_device *mmu, struct vmm_device *dev,
			      struct arm_smmu_sid *sids, unsigned int num_sid)
{
	unsigned int i;

	for (i = 0; i < num_sid; ++i) {
		struct vmm_devtree_phandle_args args;
		int ret;

		ret = vmm_devtree_parse_phandle_with_args(dev->of_node,
					"iommus", "#iommu-cells", i, &args);
		if (ret < 0)
			return ret;

		vmm_devtree_dref_node(args.np);

		if (args.np != mmu->node || args.args_count != 1)
			return VMM_EINVALID;

		sids[i].sid = args.args[0];
		if (args.args_count == 2)
			sids[i].mask = args.args[1];
		sids[i].sme = -1;
	}

	return 0;
}

static physical_addr_t arm_smmu_iova_to_phys_hard(struct vmm_iommu_domain *domain,
						  physical_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;
	void *cb_base;
	u32 tmp, try;
	physical_addr_t phys;
	irq_flags_t flags;
	unsigned long va;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);

	vmm_spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	/* ATS1 registers can only be written atomically */
	va = iova & ~0xfffUL;
	if (smmu->version == ARM_SMMU_V2)
		smmu_write_atomic_lq(va, cb_base + ARM_SMMU_CB_ATS1PR);
	else /* Register is only 32-bit in v1 */
		vmm_writel_relaxed(va, cb_base + ARM_SMMU_CB_ATS1PR);

	try = 100;
	tmp = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_ATSR);
	while ((tmp & ATSR_ACTIVE) && try) {
		vmm_udelay(5);
		tmp = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_ATSR);
		try--;
	}
	if ((tmp & ATSR_ACTIVE) && !try) {
		vmm_lerror(smmu->node->name,
		"iova to phys timed out. Falling back to software table walk.\n");
		return ops->iova_to_phys(ops, iova);
	}

	phys = vmm_readq_relaxed(cb_base + ARM_SMMU_CB_PAR);
	vmm_spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	if (phys & CB_PAR_F) {
		vmm_lerror(smmu->node->name, "translation fault!\n");
		vmm_lerror(smmu->node->name, "PAR = 0x%"PRIPADDR"\n", phys);
		return 0;
	}

	return (phys & GENMASK_ULL(39, 12)) | (iova & 0xfff);
}

static physical_addr_t arm_smmu_iova_to_phys(struct vmm_iommu_domain *domain,
					     physical_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	if (domain->type == VMM_IOMMU_DOMAIN_IDENTITY)
		return iova;

	if (!ops)
		return 0;

	if ((smmu_domain->smmu->features & ARM_SMMU_FEAT_TRANS_OPS) &&
	    (smmu_domain->stage == ARM_SMMU_DOMAIN_S1))
		return arm_smmu_iova_to_phys_hard(domain, iova);

	return ops->iova_to_phys(ops, iova);
}

static int arm_smmu_map(struct vmm_iommu_domain *domain, physical_addr_t iova,
			physical_addr_t paddr, size_t size, int prot)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;

	if (!ops)
		return VMM_ENODEV;

	ret = ops->map(ops, iova, paddr, size, prot);
	return ret;
}

static size_t arm_smmu_unmap(struct vmm_iommu_domain *domain,
			 physical_addr_t iova, size_t size)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;

	if (!ops)
		return 0;

	ret = ops->unmap(ops, iova, size);
	return ret;
}

static void arm_smmu_domain_free(struct vmm_iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	/*
	 * Free the domain resources. We assume that all devices
	 * have already been detached.
	 */
	arm_smmu_destroy_domain_context(domain);
	vmm_free(smmu_domain);
}

static struct vmm_iommu_domain *arm_smmu_domain_alloc(unsigned int type,
					struct vmm_iommu_controller *ctrl)
{
	int ret;
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu = to_smmu_device(ctrl);

	if (type != VMM_IOMMU_DOMAIN_UNMANAGED &&
	    type != VMM_IOMMU_DOMAIN_IDENTITY)
		return NULL;

	/* Allocate SMMU domain */
	smmu_domain = vmm_zalloc(sizeof(*smmu_domain));
	if (!smmu_domain)
		return NULL;

	INIT_MUTEX(&smmu_domain->init_mutex);
	INIT_SPIN_LOCK(&smmu_domain->cb_lock);

	/* Allocate and initialize context bank */
	ret = arm_smmu_init_domain_context(&smmu_domain->domain, smmu);
	if (ret) {
		vmm_lerror(smmu->node->name,
			   "Failed to init SMMU context bank (error %d)\n",
			   ret);
		vmm_free(smmu_domain);
		return NULL;
	}

	return &smmu_domain->domain;
}

static int arm_smmu_attach_device(struct vmm_iommu_domain *domain,
				  struct vmm_device *dev)
{
	struct arm_smmu_archdata *archdata = dev->iommu_priv;
	struct arm_smmu_device *smmu = archdata->smmu;
	int i, ret = VMM_OK;

	if (!smmu) {
		vmm_lerror(dev->name, "Cannot attach to SMMU\n");
		return VMM_ENXIO;
	}

	/* Allocate and update stream matching entries */
	vmm_mutex_lock(&smmu->stream_map_mutex);
	for (i = 0; i < archdata->num_sid; ++i) {
		ret = arm_smmu_find_sme(smmu,
					archdata->sids[i].sid,
					archdata->sids[i].mask);
		if (ret < 0)
			goto free_archdata_smes;

		if (smmu->smrs && smmu->s2crs[ret].count == 0) {
			smmu->smrs[ret].id = archdata->sids[i].sid;
			smmu->smrs[ret].mask = archdata->sids[i].mask;
			smmu->smrs[ret].valid = 1;
		}

		smmu->s2crs[ret].count++;

		archdata->sids[i].sme = ret;

		arm_smmu_write_sme(smmu, archdata->sids[i].sme);
	}
	vmm_mutex_unlock(&smmu->stream_map_mutex);

	archdata->io_dev = dev;
	archdata->io_domain = domain;

	vmm_linfo(smmu->node->name, "arm-smmu: attached %s device "
		  "to domain=0x%p\n", dev->name, domain);

	return VMM_OK;

free_archdata_smes:
	vmm_mutex_unlock(&smmu->stream_map_mutex);
	arm_smmu_master_free_smes(archdata);
	return ret;
}

static void arm_smmu_detach_device(struct vmm_iommu_domain *domain,
				   struct vmm_device *dev)
{
	struct arm_smmu_archdata *archdata = dev->iommu_priv;
	struct arm_smmu_device *smmu = archdata->smmu;

	vmm_linfo(smmu->node->name, "arm-smmu: detached %s device "
		  "from domain=0x%p\n", dev->name, domain);

	arm_smmu_master_free_smes(archdata);
}

static int arm_smmu_add_device(struct vmm_device *dev)
{
	struct arm_smmu_archdata *archdata = NULL;
	struct arm_smmu_device *smmu;
	struct vmm_iommu_group *group = NULL;
	struct arm_smmu_sid *sids = NULL;
	unsigned int i;
	int num_sid;
	int ret = VMM_ENODEV;

	num_sid = vmm_devtree_count_phandle_with_args(dev->of_node,
						"iommus", "#iommu-cells");
	if (num_sid <= 0)
		return VMM_ENODEV;

	if (dev->iommu_priv) {
		vmm_lerror(dev->name,
			   "%s: IOMMU driver already assigned to device\n",
			   __func__);
		return VMM_EINVALID;
	}

	archdata = vmm_zalloc(sizeof(*archdata));
	if (!archdata) {
		return VMM_ENOMEM;
	}

	sids = vmm_zalloc(num_sid * sizeof(*sids));
	if (!sids) {
		ret = VMM_ENOMEM;
		goto fail_free_archdata;
	}
	for (i = 0; i < num_sid; i++)
		sids[i].sme = -1;
	archdata->sids = sids;
	archdata->num_sid = num_sid;

	vmm_spin_lock(&smmu_devices_lock);
	list_for_each_entry(smmu, &smmu_devices, list) {
		ret = arm_smmu_find_sids(smmu, dev, sids, num_sid);
		if (!ret) {
			/*
			 * TODO Take a reference to the MMU to protect
			 * against device removal.
			 */
			break;
		}
	}
	vmm_spin_unlock(&smmu_devices_lock);
	if (ret < 0)
		goto fail_free_archdata_sids;
	archdata->smmu = smmu;

	/* Sanity check number of bits in stream ID */
	for (i = 0; i < num_sid; ++i) {
		if (sids[i].sid & ~smmu->streamid_mask) {
			ret = VMM_EINVALID;
			goto fail_free_archdata_sids;
		}
		if (sids[i].mask & ~smmu->smr_mask_mask) {
			ret = VMM_EINVALID;
			goto fail_free_archdata_sids;
		}
		sids[i].mask &= smmu->streamid_mask;
	}

	archdata->io_dev = NULL;
	archdata->io_domain = NULL;

	group = vmm_iommu_group_alloc(dev->name, &smmu->controller);
	if (VMM_IS_ERR(group)) {
		vmm_lerror(dev->name, "Failed to allocate IOMMU group\n");
		ret = VMM_PTR_ERR(group);
		goto fail_free_archdata_sids;
	}

	ret = vmm_iommu_group_add_device(group, dev);
	if (ret < 0) {
		vmm_lerror(dev->name, "Failed to add device to IOMMU group\n");
		goto fail_free_group;
	}

	/*
	 * We put group in-advance so that group is free'ed
	 * automatically when all device are removed from it.
	 */
	vmm_iommu_group_put(group);

	dev->iommu_priv = archdata;

	vmm_linfo(smmu->node->name, "arm-smmu: added %s device\n", dev->name);

	return 0;

fail_free_group:
	vmm_iommu_group_put(group);
fail_free_archdata_sids:
	vmm_free(archdata->sids);
fail_free_archdata:
	vmm_free(archdata);

	return ret;
}

static void arm_smmu_remove_device(struct vmm_device *dev)
{
	struct arm_smmu_archdata *archdata = dev->iommu_priv;
	struct arm_smmu_device *smmu = archdata->smmu;

	vmm_linfo(smmu->node->name, "arm-smmu: removed %s device\n",
		  dev->name);

	dev->iommu_priv = NULL;

	arm_smmu_master_free_smes(archdata);

	vmm_iommu_group_remove_device(dev);

	vmm_free(archdata->sids);
	vmm_free(archdata);
}

static struct vmm_iommu_ops arm_smmu_ops = {
	.domain_alloc = arm_smmu_domain_alloc,
	.domain_free = arm_smmu_domain_free,
	.attach_dev = arm_smmu_attach_device,
	.detach_dev = arm_smmu_detach_device,
	.map = arm_smmu_map,
	.unmap = arm_smmu_unmap,
	.iova_to_phys = arm_smmu_iova_to_phys,
	.add_device = arm_smmu_add_device,
	.remove_device = arm_smmu_remove_device,
	.pgsize_bitmap = -1UL,
};

static int arm_smmu_id_size_to_bits(int size)
{
	switch (size) {
	case 0:
		return 32;
	case 1:
		return 36;
	case 2:
		return 40;
	case 3:
		return 42;
	case 4:
		return 44;
	case 5:
	default:
		return 48;
	}
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned long size;
	struct vmm_devtree_node *node = smmu->node;
	void *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id;
	bool cttw_reg, cttw_fw = smmu->features & ARM_SMMU_FEAT_COHERENT_WALK;

	/* ID0 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID0);
	if (id & ID0_S1TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S1;
		vmm_linfo(node->name, "arm-smmu: stage 1 translation\n");
	}

	if (id & ID0_S2TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S2;
		vmm_linfo(node->name, "arm-smmu: stage 2 translation\n");
	}

	if (id & ID0_NTS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_NESTED;
		vmm_linfo(node->name, "arm-smmu: nested translation\n");
	}

	if (!(smmu->features &
		(ARM_SMMU_FEAT_TRANS_S1 | ARM_SMMU_FEAT_TRANS_S2))) {
		vmm_lerror(node->name, "%s: no translation support!\n",
			   __func__);
		return VMM_ENODEV;
	}

	if ((id & ID0_S1TS) &&
		((smmu->version < ARM_SMMU_V2) || !(id & ID0_ATOSNS))) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_OPS;
		vmm_linfo(node->name, "arm-smmu: address translation ops\n");
	}

	/*
	 * In order for DMA API calls to work properly, we must defer to what
	 * the FW says about coherency, regardless of what the hardware claims.
	 * Fortunately, this also opens up a workaround for systems where the
	 * ID register value has ended up configured incorrectly.
	 */
	cttw_reg = !!(id & ID0_CTTW);
	if (cttw_fw || cttw_reg)
		vmm_linfo(node->name, "arm-smmu: %scoherent table walk\n",
			  cttw_fw ? "" : "non-");
	if (cttw_fw != cttw_reg)
		vmm_linfo(node->name, "arm-smmu: (IDR0.CTTW overridden by "
			  "FW configuration)\n");

	/* Max. number of entries we have for stream matching/indexing */
	if (smmu->version == ARM_SMMU_V2 && id & ID0_EXIDS) {
		smmu->features |= ARM_SMMU_FEAT_EXIDS;
		size = 1 << 16;
	} else {
		size = 1 << ((id >> ID0_NUMSIDB_SHIFT) & ID0_NUMSIDB_MASK);
	}
	smmu->streamid_mask = size - 1;
	if (id & ID0_SMS) {
		size = (id >> ID0_NUMSMRG_SHIFT) & ID0_NUMSMRG_MASK;
		smmu->num_mapping_groups = size;
		if (size == 0) {
			vmm_lerror(node->name, "%s: stream-matching supported, "
				   "but no SMRs present!\n", __func__);
			return VMM_ENODEV;
		}
	} else {
		return VMM_EINVALID;
	}

	vmm_linfo(node->name, "arm-smmu: num_groups=%d streamid_mask=0x%x\n",
		  smmu->num_mapping_groups, smmu->streamid_mask);

	if (smmu->version < ARM_SMMU_V2 || !(id & ID0_PTFS_NO_AARCH32)) {
		smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_L;
		if (!(id & ID0_PTFS_NO_AARCH32S))
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_S;
	}

	/* ID1 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size <<= smmu->pgshift;
	if (smmu->cb_base != gr0_base + size) {
		vmm_lwarning(node->name, "%s: SMMU address space size (0x%lx)"
			     " differs from mapped region size (0x%tx)!\n",
			     __func__, size * 2, (smmu->cb_base - gr0_base) * 2);
	}

	smmu->num_s2_context_banks = (id >> ID1_NUMS2CB_SHIFT) & ID1_NUMS2CB_MASK;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;
	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		vmm_lerror(node->name, "%s: impossible number of S2 "
			   "context banks!\n", __func__);
		return VMM_ENODEV;
	}
	vmm_linfo(node->name, "arm-smmu: %u context banks (%u stage-2 only)\n",
		  smmu->num_context_banks, smmu->num_s2_context_banks);

	/*
	 * Cavium CN88xx erratum #27704.
	 * Ensure ASID and VMID allocation is unique across all SMMUs in
	 * the system.
	 */
	if (smmu->model == CAVIUM_SMMUV2) {
		smmu->cavium_id_base =
			arch_atomic_add_return(&cavium_smmu_context_count,
						smmu->num_context_banks);
		smmu->cavium_id_base -= smmu->num_context_banks;
		vmm_linfo(node->name, "arm-smmu: enabling workaround for "
			  "Cavium erratum 27704\n");
	}

	/* ID2 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->ipa_size = size;

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->pa_size = size;

	if (id & ID2_VMID16)
		smmu->features |= ARM_SMMU_FEAT_VMID16;

	if (smmu->version < ARM_SMMU_V2) {
		smmu->va_size = smmu->ipa_size;
		if (smmu->version == ARM_SMMU_V1_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	} else {
		size = (id >> ID2_UBS_SHIFT) & ID2_UBS_MASK;
		smmu->va_size = arm_smmu_id_size_to_bits(size);
		if (id & ID2_PTFS_4K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_4K;
		if (id & ID2_PTFS_16K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_16K;
		if (id & ID2_PTFS_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	}

	/* Now we've corralled the various formats, what'll it do? */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S)
		smmu->pgsize_bitmap |= SZ_4K | SZ_64K | SZ_1M | SZ_16M;
	if (smmu->features &
	    (ARM_SMMU_FEAT_FMT_AARCH32_L | ARM_SMMU_FEAT_FMT_AARCH64_4K))
		smmu->pgsize_bitmap |= SZ_4K | SZ_2M | SZ_1G;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_16K)
		smmu->pgsize_bitmap |= SZ_16K | SZ_32M;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_64K)
		smmu->pgsize_bitmap |= SZ_64K | SZ_512M;

	if (arm_smmu_ops.pgsize_bitmap == -1UL)
		arm_smmu_ops.pgsize_bitmap = smmu->pgsize_bitmap;
	else
		arm_smmu_ops.pgsize_bitmap |= smmu->pgsize_bitmap;
	vmm_linfo(node->name, "arm-smmu: Supported page sizes: 0x%08lx\n",
		  smmu->pgsize_bitmap);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		vmm_linfo(node->name, "arm-smmu: Stage-1: %lu-bit VA -> %lu-bit IPA\n",
			  smmu->va_size, smmu->ipa_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		vmm_linfo(node->name, "arm-smmu: Stage-2: %lu-bit IPA -> %lu-bit PA\n",
			  smmu->ipa_size, smmu->pa_size);

	return VMM_OK;
}

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	virtual_addr_t *cb_base;
	int i;
	u32 reg, major;

	/* clear global FSR */
	reg = vmm_readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);
	vmm_writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as bypass unless overridden.
	 */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		arm_smmu_reset_sme(smmu, i);
	}

	if (smmu->model == ARM_MMU500) {
		/*
		 * Before clearing ARM_MMU500_ACTLR_CPRE, need to
		 * clear CACHE_LOCK bit of ACR first. And, CACHE_LOCK
		 * bit is only present in MMU-500r2 onwards.
		 */
		reg = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID7);
		major = (reg >> ID7_MAJOR_SHIFT) & ID7_MAJOR_MASK;
		reg = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sACR);
		if (major >= 2)
			reg &= ~ARM_MMU500_ACR_CACHE_LOCK;
		/*
		 * Allow unmatched Stream IDs to allocate bypass
		 * TLB entries for reduced latency.
		 */
		reg |= ARM_MMU500_ACR_SMTNMB_TLBEN | ARM_MMU500_ACR_S2CRB_TLBEN;
		vmm_writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_sACR);
	}

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		cb_base = ARM_SMMU_CB(smmu, i);

		arm_smmu_write_context_bank(smmu, i);
		vmm_writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);

		/*
		 * Disable MMU-500's not-particularly-beneficial next-page
		 * prefetcher for the sake of errata #841119 and #826419.
		 */
		if (smmu->model == ARM_MMU500) {
			reg = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_ACTLR);
			reg &= ~ARM_MMU500_ACTLR_CPRE;
			vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_ACTLR);
		}
	}

	/* Invalidate the TLB, just in case */
	vmm_writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLH);
	vmm_writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	reg = vmm_readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE);

	/* Disable TLB broadcasting. */
	reg |= (sCR0_VMIDPNE | sCR0_PTM);

	/* Enable client access, handling unmatched streams as appropriate */
	reg &= ~sCR0_CLIENTPD;
	reg &= ~sCR0_USFCFG; /* HINT: Set sCR0_USFCFG to disable bypass */

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	/* Push the button */
	arm_smmu_tlb_sync_global(smmu);
	vmm_writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
}

static int arm_smmu_init(struct vmm_devtree_node *node,
			 enum arm_smmu_arch_version version,
			 enum arm_smmu_implementation model)
{
	int ret = VMM_OK, i;
	irq_flags_t flags;
	virtual_addr_t va;
	physical_addr_t pa;
	physical_size_t size;
	u32 num_irqs, global_irqs;
	struct arm_smmu_device *smmu;

	smmu = vmm_zalloc(sizeof(*smmu));
	if (!smmu) {
		vmm_lerror(node->name, "%s: can't allocate device data\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail;
	}
	INIT_LIST_HEAD(&smmu->list);
	vmm_devtree_ref_node(node);
	smmu->node = node;
	smmu->version = version;
	smmu->model = model;
	INIT_SPIN_LOCK(&smmu->global_sync_lock);

	parse_driver_options(smmu);

	if (vmm_devtree_is_dma_coherent(node))
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	ret = vmm_devtree_request_regmap(node, &va, 0, "SMMU");
	if (ret) {
		vmm_lerror(node->name, "%s: can't map device regs\n",
			   __func__);
		goto fail_free_smmu;
	}
	smmu->base = (void *)va;

	ret = vmm_devtree_regsize(node, &pa, 0);
	if (ret) {
		vmm_lerror(node->name, "%s: can't find reg physical address\n",
			   __func__);
		goto fail_unmap_regs;
	}
	smmu->reg_pa = size;

	ret = vmm_devtree_regsize(node, &size, 0);
	if (ret) {
		vmm_lerror(node->name, "%s: can't find reg size\n", __func__);
		goto fail_unmap_regs;
	}
	smmu->reg_size = size;

	smmu->cb_base = smmu->base + smmu->reg_size / 2;

	vmm_linfo(node->name, "arm-smmu: phys=0x%"PRIPADDR" size=%"PRIPSIZE
		  "\n", pa, size);

	if (vmm_devtree_read_u32(node, "#global-interrupts",
				 &global_irqs)) {
		vmm_lerror(node->name, "%s: can't find #global-intretupts "
			   "DT prop\n", __func__);
		ret = VMM_ENODEV;
		goto fail_unmap_regs;
	}
	num_irqs = vmm_devtree_irq_count(node);
	if (num_irqs < global_irqs) {
		vmm_lerror(node->name, "%s: number of global-intretupts "
			   "cannot be larger than total interrupts\n",
			   __func__);
		ret = VMM_ENODEV;
		goto fail_unmap_regs;
	}

	smmu->num_global_irqs = global_irqs;
	smmu->num_context_irqs = num_irqs - global_irqs;

	vmm_linfo(node->name, "arm-smmu: num_irqs=%d num_global_irqs=%d\n",
		  num_irqs, global_irqs);

	if (!smmu->num_context_irqs) {
		vmm_lerror(node->name, "%s: need atleast one context irqs\n",
			   __func__);
		ret = VMM_ENODEV;
		goto fail_unmap_regs;
	}

	smmu->irqs = vmm_zalloc(sizeof(*smmu->irqs) * num_irqs);
	if (!smmu->irqs) {
		vmm_lerror(node->name, "%s: failed to allocate irqs\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail_unmap_regs;
	}

	for (i = 0; i < num_irqs; ++i) {
		int irq = vmm_devtree_irq_parse_map(node, i);
		if (irq < 0) {
			vmm_lerror(node->name, "%s: failed to parse irq%d\n",
				   __func__, i);
			ret = VMM_ENODEV;
			goto fail_free_irqs;
		}

		smmu->irqs[i] = irq;
	}

	ret = arm_smmu_device_cfg_probe(smmu);
	if (ret) {
		vmm_lerror(node->name, "%s: cfg_probe() failed\n", __func__);
		goto fail_free_irqs;
	}

	if (smmu->version == ARM_SMMU_V2 &&
	    smmu->num_context_banks != smmu->num_context_irqs) {
		vmm_lerror(node->name, "%s: found only %d context "
			   "interrupt(s) but %d required\n", __func__,
			   smmu->num_context_irqs, smmu->num_context_banks);
		ret = VMM_ENODEV;
		goto fail_free_irqs;
	}

	smmu->smrs = vmm_zalloc(smmu->num_mapping_groups * sizeof(*smmu->smrs));
	if (!smmu->smrs) {
		vmm_lerror(node->name, "%s: failed to alloc SMRs\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail_free_irqs;
	}

	smmu->s2crs = vmm_zalloc(smmu->num_mapping_groups * sizeof(*smmu->s2crs));
	if (!smmu->s2crs) {
		vmm_lerror(node->name, "%s: failed to alloc S2CRs\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail_free_smrs;
	}
	for (i = 0; i < smmu->num_mapping_groups; i++)
		smmu->s2crs[i] = s2cr_init_val;

	INIT_MUTEX(&smmu->stream_map_mutex);

	smmu->cbs = vmm_zalloc(smmu->num_context_banks *sizeof(*smmu->cbs));
	if (!smmu->cbs) {
		vmm_lerror(node->name, "%s: failed to alloc CBs\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail_free_s2crs;
	}
	ARCH_ATOMIC_INIT(&smmu->irptndx, 0);

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		ret = vmm_host_irq_register(smmu->irqs[i],
					    "arm-smmu-global-fault",
				            arm_smmu_global_fault, smmu);
		if (ret) {
			vmm_lerror(node->name, "%s: failed to register global "
				   "irq%d (%u)\n", __func__, i, smmu->irqs[i]);
			while (i > 0) {
				vmm_host_irq_unregister(smmu->irqs[i], smmu);
				i--;
			}
			goto fail_free_cbs;
		}
	}

	arm_smmu_device_reset(smmu);
	arm_smmu_test_smr_masks(smmu);

	/* Register IOMMU controller */
	if (strlcpy(smmu->controller.name, smmu->node->name,
	    sizeof(smmu->controller.name)) >= sizeof(smmu->controller.name)) {
		vmm_lerror(node->name, "%s: failed to copy controller name\n",
			   __func__);
		ret = VMM_EOVERFLOW;
		goto fail_free_cbs;
	}
	ret = vmm_iommu_controller_register(&smmu->controller);
	if (ret) {
		vmm_lerror(node->name, "%s: failed to register controller\n",
			   __func__);
		goto fail_free_cbs;
	}

	vmm_spin_lock_irqsave(&smmu_devices_lock, flags);
	list_add_tail(&smmu->list, &smmu_devices);
	vmm_spin_unlock_irqrestore(&smmu_devices_lock, flags);

	///* Oh, for a proper bus abstraction */
	if (!vmm_iommu_present(&platform_bus))
		vmm_bus_set_iommu(&platform_bus, &arm_smmu_ops);

	vmm_linfo(node->name, "arm-smmu: ready!\n");

	return VMM_OK;

fail_free_cbs:
	vmm_free(smmu->cbs);
fail_free_s2crs:
	vmm_free(smmu->s2crs);
fail_free_smrs:
	vmm_free(smmu->smrs);
fail_free_irqs:
	vmm_free(smmu->irqs);
fail_unmap_regs:
	vmm_devtree_regunmap_release(node, (virtual_addr_t)smmu->base, 0);
fail_free_smmu:
	vmm_devtree_dref_node(smmu->node);
	vmm_free(smmu);
fail:
	return ret;
}

static int arm_smmu_v1_init(struct vmm_devtree_node *node)
{
	return arm_smmu_init(node, ARM_SMMU_V1, GENERIC_SMMU);
}

static int arm_smmu_v2_init(struct vmm_devtree_node *node)
{
	return arm_smmu_init(node, ARM_SMMU_V2, GENERIC_SMMU);
}

static int arm_smmu_401_init(struct vmm_devtree_node *node)
{
	return arm_smmu_init(node, ARM_SMMU_V1_64K, GENERIC_SMMU);
}

static int arm_smmu_500_init(struct vmm_devtree_node *node)
{
	return arm_smmu_init(node, ARM_SMMU_V2, ARM_MMU500);
}

static int cavium_smmu_v2_init(struct vmm_devtree_node *node)
{
	return arm_smmu_init(node, ARM_SMMU_V2, CAVIUM_SMMUV2);
}

VMM_IOMMU_INIT_DECLARE(smmu_v1, "arm,smmu-v1", arm_smmu_v1_init);
VMM_IOMMU_INIT_DECLARE(smmu_v2, "arm,smmu-v2", arm_smmu_v2_init);
VMM_IOMMU_INIT_DECLARE(smmu_400, "arm,mmu-400", arm_smmu_v1_init);
VMM_IOMMU_INIT_DECLARE(smmu_401, "arm,mmu-401", arm_smmu_401_init);
VMM_IOMMU_INIT_DECLARE(smmu_500, "arm,mmu-500", arm_smmu_500_init);
VMM_IOMMU_INIT_DECLARE(cavium_smmu_v2, "cavium,smmu-v2", cavium_smmu_v2_init);
