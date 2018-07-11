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
 * @brief IOMMU driver for ARM smmu-500.
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
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_platform.h>
#include <vmm_iommu.h>
#include <vmm_devtree.h>
#include <libs/list.h>

#include "io-pgtable.h"

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
#define ARM_SMMU_GR0_NS(smmu)		((smmu)->base)

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

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)
#define sCR0_VMID16EN			(1 << 31)
#define sCR0_BSU_SHIFT			14
#define sCR0_BSU_MASK			0x3

/* Auxiliary Configuration register */
#define ARM_SMMU_GR0_sACR		0x10

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ARM_SMMU_GR0_ID1		0x24
#define ARM_SMMU_GR0_ID2		0x28
#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38
#define ARM_SMMU_GR0_ID7		0x3c
#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58

#define ID0_S1TS			(1 << 30)
#define ID0_S2TS			(1 << 29)
#define ID0_NTS				(1 << 28)
#define ID0_SMS				(1 << 27)
#define ID0_ATOSNS			(1 << 26)
#define ID0_PTFS_NO_AARCH32		(1 << 25)
#define ID0_PTFS_NO_AARCH32S		(1 << 24)
#define ID0_CTTW			(1 << 14)
#define ID0_NUMIRPT_SHIFT		16
#define ID0_NUMIRPT_MASK		0xff
#define ID0_NUMSIDB_SHIFT		9
#define ID0_NUMSIDB_MASK		0xf
#define ID0_NUMSMRG_SHIFT		0
#define ID0_NUMSMRG_MASK		0xff

#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB_SHIFT		28
#define ID1_NUMPAGENDXB_MASK		7
#define ID1_NUMS2CB_SHIFT		16
#define ID1_NUMS2CB_MASK		0xff
#define ID1_NUMCB_SHIFT			0
#define ID1_NUMCB_MASK			0xff

#define ID2_OAS_SHIFT			4
#define ID2_OAS_MASK			0xf
#define ID2_IAS_SHIFT			0
#define ID2_IAS_MASK			0xf
#define ID2_UBS_SHIFT			8
#define ID2_UBS_MASK			0xf
#define ID2_PTFS_4K			(1 << 12)
#define ID2_PTFS_16K			(1 << 13)
#define ID2_PTFS_64K			(1 << 14)
#define ID2_VMID16			(1 << 15)

#define ID7_MAJOR_SHIFT			4
#define ID7_MAJOR_MASK			0xf

/* Global TLB invalidation */
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)
#define TLB_LOOP_TIMEOUT		1000000	/* 1s! */

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_ID_SHIFT			0

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_CBNDX_SHIFT		0
#define S2CR_CBNDX_MASK			0xff
#define S2CR_TYPE_SHIFT			16
#define S2CR_TYPE_MASK			0x3
enum arm_smmu_s2cr_type {
	S2CR_TYPE_TRANS,
	S2CR_TYPE_BYPASS,
	S2CR_TYPE_FAULT,
};

#define S2CR_PRIVCFG_SHIFT		24
#define S2CR_PRIVCFG_MASK		0x3
enum arm_smmu_s2cr_privcfg {
	S2CR_PRIVCFG_DEFAULT,
	S2CR_PRIVCFG_DIPAN,
	S2CR_PRIVCFG_UNPRIV,
	S2CR_PRIVCFG_PRIV,
};

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_VMID_MASK			0xff
#define CBAR_S1_BPSHCFG_SHIFT		8
#define CBAR_S1_BPSHCFG_MASK		3
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_S1_MEMATTR_SHIFT		12
#define CBAR_S1_MEMATTR_MASK		0xf
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_MASK			0x3
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_BYPASS	(1 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_FAULT	(2 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_TRANS	(3 << CBAR_TYPE_SHIFT)
#define CBAR_IRPTNDX_SHIFT		24
#define CBAR_IRPTNDX_MASK		0xff

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_32BIT		(0 << 0)
#define CBA2R_RW64_64BIT		(1 << 0)
#define CBA2R_VMID_SHIFT		16
#define CBA2R_VMID_MASK			0xffff

/* Translation context bank */
#define ARM_SMMU_CB_BASE(smmu)		((smmu)->base + ((smmu)->reg_size >> 1))
#define ARM_SMMU_CB(smmu, n)		((n) * (1 << (smmu)->pgshift))

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_RESUME		0x8
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBR1		0x28
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c
#define ARM_SMMU_CB_PAR			0x50
#define ARM_SMMU_CB_FSR			0x58
#define ARM_SMMU_CB_FAR			0x60
#define ARM_SMMU_CB_FSYNR0		0x68
#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638
#define ARM_SMMU_CB_ATS1PR		0x800
#define ARM_SMMU_CB_ATSR		0x8f0

#define SCTLR_S1_ASIDPNE		(1 << 12)
#define SCTLR_CFCFG			(1 << 7)
#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_E				(1 << 4)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)

#define ARM_MMU500_ACTLR_CPRE		(1 << 1)

#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

#define CB_PAR_F			(1 << 0)

#define ATSR_ACTIVE			(1 << 0)

#define RESUME_RETRY			(0 << 0)
#define RESUME_TERMINATE		(1 << 0)

#define TTBCR2_SEP_SHIFT		15
#define TTBCR2_SEP_UPSTREAM		(0x7 << TTBCR2_SEP_SHIFT)

#define TTBRn_ASID_SHIFT		48

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define FSYNR0_WNR			(1 << 4)

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_s2cr {
	struct iommu_group		*group;
	int				count;
	enum arm_smmu_s2cr_type		type;
	enum arm_smmu_s2cr_privcfg	privcfg;
	u8				cbndx;
};

struct smmu_vmsa_device {
	struct list_head list;
	struct vmm_devtree_node *node;

	void *base;
	physical_addr_t reg_pa;
	physical_size_t reg_size;

	unsigned int *irqs;
	u32 num_global_irqs;
	u32 num_context_irqs;
	u32 pgshift;
	unsigned long	ipa_size;
	unsigned long	pa_size;
	struct arm_smmu_smr *smrs;
	struct arm_smmu_s2cr *s2crs;
	
	u32 num_context_banks;
	u16 num_mapping_groups;
	u16 streamid_mask;
};

struct smmu_vmsa_domain {
	struct smmu_vmsa_device *mmu;
	struct vmm_iommu_domain io_domain;

	struct io_pgtable_cfg cfg;
	struct io_pgtable_ops *iop;

	unsigned int context_id;
	vmm_spinlock_t lock;			/* Protects mappings */
};

struct smmu_vmsa_sid {
	unsigned int sid;
	unsigned int mask;
	int smr;
};

struct smmu_vmsa_archdata {
	struct smmu_vmsa_device *mmu;
	struct smmu_vmsa_sid *sids;
	unsigned int num_sid;

	/* io_xxx only updated at time of attaching device */
	struct vmm_device *io_dev;
	struct vmm_iommu_domain *io_domain;
};

static DEFINE_SPINLOCK(smmu_devices_lock);
static LIST_HEAD(smmu_devices);

static struct smmu_vmsa_domain* to_vmsa_domain(struct vmm_iommu_domain *dom)
{
	return container_of(dom, struct smmu_vmsa_domain, io_domain);
}

static void arm_smmu_write_s2cr(struct smmu_vmsa_device *smmu, int idx)
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg = (s2cr->type & S2CR_TYPE_MASK) << S2CR_TYPE_SHIFT |
		  (s2cr->cbndx & S2CR_CBNDX_MASK) << S2CR_CBNDX_SHIFT |
		  (s2cr->privcfg & S2CR_PRIVCFG_MASK) << S2CR_PRIVCFG_SHIFT;

	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));
}

static void arm_smmu_write_smr(struct smmu_vmsa_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = smr->id << SMR_ID_SHIFT | smr->mask << SMR_MASK_SHIFT;

	if (smr->valid)
		reg |= SMR_VALID;
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
}

static void arm_smmu_write_sme(struct smmu_vmsa_device *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	if (smmu->smrs)
		arm_smmu_write_smr(smmu, idx);
}

static void arm_smmu_reset_sme(struct smmu_vmsa_device *smmu, int idx)
{
	u32 reg;
	reg = (0x3 << S2CR_TYPE_SHIFT);
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));

	reg = 0x00;
	vmm_writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
}

static void arm_smmu_destroy_domain_context(struct smmu_vmsa_domain *smmu_domain)
{
	struct smmu_vmsa_device *smmu = smmu_domain->mmu;
	void *cb_base;

	if (!smmu)
		return;

	/*
	 * Disable the context bank and free the page tables before freeing
	 * it.
	 */
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, smmu_domain->context_id);
	vmm_writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
}

static int arm_smmu_find_sids(struct smmu_vmsa_device *mmu, struct vmm_device *dev,
			      struct smmu_vmsa_sid *sids, unsigned int num_sid)
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
		sids[i].smr = -1;
	}

	return 0;
}

static struct iommu_gather_ops smmu_gather_ops = {
	.tlb_flush_all	= NULL,
	.tlb_add_flush	= NULL,
	.tlb_sync	= NULL,
};

#if 0 /* TODO: */
static void arm_smmu_init_context_bank(struct smmu_vmsa_domain *smmu_domain,
				       struct smmu_vmsa_device *smmu,
				       struct io_pgtable_cfg *pgtbl_cfg)
{
	u32 reg, reg2;
	u64 reg64;
	bool stage1;
	void *cb_base, *gr1_base;

	gr1_base = ARM_SMMU_GR1(smmu);
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);

	if (smmu->version > ARM_SMMU_V1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
			reg = CBA2R_RW64_64BIT;
		else
			reg = CBA2R_RW64_32BIT;
		/* 16-bit VMIDs live in CBA2R */
		if (smmu->features & ARM_SMMU_FEAT_VMID16)
			reg |= ARM_SMMU_CB_VMID(smmu, cfg) << CBA2R_VMID_SHIFT;

		vmm_writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBA2R(cfg->cbndx));
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
		reg |= ARM_SMMU_CB_VMID(smmu, cfg) << CBAR_VMID_SHIFT;
	}
	vmm_writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBAR(cfg->cbndx));

	/* TTBRs */
	if (stage1) {
		u16 asid = ARM_SMMU_CB_ASID(smmu, cfg);

		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			reg = pgtbl_cfg->arm_v7s_cfg.ttbr[0];
			vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0);
			reg = pgtbl_cfg->arm_v7s_cfg.ttbr[1];
			vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR1);
			vmm_writel_relaxed(asid, cb_base + ARM_SMMU_CB_CONTEXTIDR);
		} else {
			reg64 = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
			reg64 |= (u64)asid << TTBRn_ASID_SHIFT;
			vmm_writeq_relaxed(reg64, cb_base + ARM_SMMU_CB_TTBR0);
			reg64 = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1];
			reg64 |= (u64)asid << TTBRn_ASID_SHIFT;
			vmm_writeq_relaxed(reg64, cb_base + ARM_SMMU_CB_TTBR1);
		}
	} else {
		reg64 = pgtbl_cfg->arm_lpae_s2_cfg.vttbr;
		vmm_writeq_relaxed(reg64, cb_base + ARM_SMMU_CB_TTBR0);
	}

	/* TTBCR */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			reg = pgtbl_cfg->arm_v7s_cfg.tcr;
			reg2 = 0;
		} else {
			reg = pgtbl_cfg->arm_lpae_s1_cfg.tcr;
			reg2 = pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32;
			reg2 |= TTBCR2_SEP_UPSTREAM;
		}
		if (smmu->version > ARM_SMMU_V1)
			vmm_writel_relaxed(reg2, cb_base + ARM_SMMU_CB_TTBCR2);
	} else {
		reg = pgtbl_cfg->arm_lpae_s2_cfg.vtcr;
	}
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR);

	/* MAIRs (stage-1 only) */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			reg = pgtbl_cfg->arm_v7s_cfg.prrr;
			reg2 = pgtbl_cfg->arm_v7s_cfg.nmrr;
		} else {
			reg = pgtbl_cfg->arm_lpae_s1_cfg.mair[0];
			reg2 = pgtbl_cfg->arm_lpae_s1_cfg.mair[1];
		}
		vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_S1_MAIR0);
		vmm_writel_relaxed(reg2, cb_base + ARM_SMMU_CB_S1_MAIR1);
	}

	/* SCTLR */
	reg = SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE | SCTLR_M;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
#ifndef CONFIG_CPU_LE
	reg |= SCTLR_E;
#endif
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
}
#endif

static int arm_smmu_init_domain_context(struct smmu_vmsa_domain *domain,
					struct smmu_vmsa_device *smmu)
{
	domain->cfg.quirks = IO_PGTABLE_QUIRK_ARM_NS;
	domain->cfg.pgsize_bitmap = SZ_1G | SZ_2M | SZ_4K,
	domain->cfg.ias = 48;
	domain->cfg.oas = 48;
	domain->cfg.tlb = &smmu_gather_ops;

	domain->iop = alloc_io_pgtable_ops(ARM_64_LPAE_S1, &domain->cfg,
					   domain);
	if (!domain->iop)
		return VMM_EINVALID;

#if 0 /* TODO: */
	arm_smmu_init_context_bank(smmu_domain, smmu, &pgtbl_cfg);
#endif

	return VMM_OK;
}

static int arm_smmu_find_sme(struct smmu_vmsa_device *smmu, u16 id, u16 mask)
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

static bool arm_smmu_free_sme(struct smmu_vmsa_device *smmu, int idx)
{
	if (--smmu->s2crs[idx].count)
		return FALSE;

	/* TODO: smmu->s2crs[idx]->reg = 0x0; */
	if (smmu->smrs)
		smmu->smrs[idx].valid = 0;

	return TRUE;
}

static void arm_smmu_master_free_smes(struct smmu_vmsa_archdata *archdata)
{
	struct smmu_vmsa_device *smmu = archdata->mmu;
	int i;

	for (i = 0; i < archdata->num_sid; i++) {
		if ((archdata->sids[i].smr > -1) &&
		    arm_smmu_free_sme(smmu, archdata->sids[i].smr)) {
			arm_smmu_write_sme(smmu, archdata->sids[i].smr);
			archdata->sids[i].smr = -1;
		}
	}
}

static physical_addr_t smmu_iova_to_phys(struct vmm_iommu_domain *domain,
					physical_addr_t iova)
{
	physical_addr_t ret;
	struct smmu_vmsa_domain *smmu_domain = to_vmsa_domain(domain);
	struct io_pgtable_ops *ops= smmu_domain->iop;

	if (!ops)
		return 0;

	ret = ops->iova_to_phys(ops, iova);

	return ret;
}

static int smmu_map(struct vmm_iommu_domain *domain, physical_addr_t iova,
			physical_addr_t paddr, size_t size, int prot)
{
	int ret;
	struct smmu_vmsa_domain *smmu_domain = to_vmsa_domain(domain);
	struct io_pgtable_ops *ops= smmu_domain->iop;

	if (!ops)
		return VMM_ENODEV;

	ret = ops->map(ops, iova, paddr, size, prot);
	return ret;
}

static size_t smmu_unmap(struct vmm_iommu_domain *domain,
			 physical_addr_t iova, size_t size)
{
	size_t ret;
	struct smmu_vmsa_domain *smmu_domain = to_vmsa_domain(domain);
	struct io_pgtable_ops *ops= smmu_domain->iop;

	if (!ops)
		return 0;

	ret = ops->unmap(ops, iova, size);
	return ret;
}

static void smmu_domain_free(struct vmm_iommu_domain *domain)
{
	struct smmu_vmsa_domain *smmu_domain = to_vmsa_domain(domain);

	/*
	 * Free the domain resources. We assume that all devices have already
	 * been detached.
	 */
	arm_smmu_destroy_domain_context(smmu_domain);
	free_io_pgtable_ops(smmu_domain->iop);
	vmm_free(smmu_domain);
}

static struct vmm_iommu_domain * smmu_domain_alloc(unsigned int type)
{
	struct smmu_vmsa_domain *domain;

	if (type != VMM_IOMMU_DOMAIN_UNMANAGED)
		return NULL;

	domain = vmm_zalloc(sizeof(*domain));
	if (!domain)
		return NULL;

	INIT_SPIN_LOCK(&domain->lock);

	return &domain->io_domain;
}

static int smmu_attach_device(struct vmm_iommu_domain *domain,
			      struct vmm_device *dev)
{
	struct smmu_vmsa_archdata *archdata = dev->iommu_priv;
	struct smmu_vmsa_device *smmu = archdata->mmu;
	struct smmu_vmsa_domain *smmu_domain = to_vmsa_domain(domain);
	irq_flags_t flags;
	int i, ret = VMM_OK;

	if (!smmu) {
		vmm_lerror(dev->name, "Cannot attach to SMMU\n");
		return VMM_ENXIO;
	}

	vmm_spin_lock_irqsave(&smmu_domain->lock, flags);
	if (!smmu_domain->mmu) {
		/* The domain hasn't been used yet, initialize it. */
		smmu_domain->mmu = smmu;
		ret = arm_smmu_init_domain_context(smmu_domain, smmu);
	} else if (smmu_domain->mmu != smmu) {
		/*
		 * Something is wrong, we can't attach two devices using
		 * different IOMMUs to the same domain.
		 */
		vmm_lerror(dev->name,
			   "Can't attach SMMU %s to domain on SMMU %s\n",
			   smmu->node->name, smmu_domain->mmu->node->name);
		ret = VMM_EINVALID;
	}
	vmm_spin_unlock_irqrestore(&smmu_domain->lock, flags);
	if (ret)
		return ret;

	/* Allocate stream matching entries */
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

		archdata->sids[i].smr = ret;

		arm_smmu_write_sme(smmu, archdata->sids[i].smr);
	}

	archdata->io_dev = dev;
	archdata->io_domain = domain;

	vmm_linfo(smmu->node->name, "arm-smmu: attached %s device "
		  "to domain=0x%p\n", dev->name, domain);

	return VMM_OK;

free_archdata_smes:
	arm_smmu_master_free_smes(archdata);
	return ret;
}

static void smmu_detach_device(struct vmm_iommu_domain *domain,
			       struct vmm_device *dev)
{
	struct smmu_vmsa_archdata *archdata = dev->iommu_priv;
	struct smmu_vmsa_device *smmu = archdata->mmu;

	vmm_linfo(smmu->node->name, "arm-smmu: detached %s device "
		  "from domain=0x%p\n", dev->name, domain);

	arm_smmu_master_free_smes(archdata);
}

static int smmu_add_device(struct vmm_device *dev)
{
	struct smmu_vmsa_archdata *archdata = NULL;
	struct smmu_vmsa_device *smmu;
	struct vmm_iommu_group *group = NULL;
	struct smmu_vmsa_sid *sids = NULL;
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
		goto free_archdata;
	}
	for (i = 0; i < num_sid; i++)
		sids[i].smr = -1;
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
		goto free_archdata_sids;
	archdata->mmu = smmu;

	/* Sanity check number of bits in stream ID */
	for (i = 0; i < num_sid; ++i) {
		if (sids[i].sid & ~smmu->streamid_mask) {
			ret = VMM_EINVALID;
			goto free_archdata_sids;
		}
		sids[i].mask &= smmu->streamid_mask;
	}

	archdata->io_dev = NULL;
	archdata->io_domain = NULL;

	group = vmm_iommu_group_alloc();
	if (VMM_IS_ERR(group)) {
		vmm_lerror(dev->name, "Failed to allocate IOMMU group\n");
		ret = VMM_PTR_ERR(group);
		goto free_archdata_sids;
	}

	ret = vmm_iommu_group_add_device(group, dev);
	vmm_iommu_group_put(group);
	if (ret < 0) {
		vmm_lerror(dev->name, "Failed to add device to IPMMU group\n");
		goto free_archdata_sids;
	}

	dev->iommu_priv = archdata;

	vmm_linfo(smmu->node->name, "arm-smmu: added %s device\n", dev->name);

	return 0;

free_archdata_sids:
	vmm_free(archdata->sids);
free_archdata:
	vmm_free(archdata);

	return ret;
}

static void smmu_remove_device(struct vmm_device *dev)
{
	struct smmu_vmsa_archdata *archdata = dev->iommu_priv;
	struct smmu_vmsa_device *smmu = archdata->mmu;

	vmm_linfo(smmu->node->name, "arm-smmu: removed %s device\n",
		  dev->name);

	vmm_iommu_group_remove_device(dev);

	vmm_free(archdata->sids);
	vmm_free(archdata);

	dev->iommu_priv = NULL;
}

static struct vmm_iommu_ops smmu_ops = {
	.domain_alloc = smmu_domain_alloc,
	.domain_free = smmu_domain_free,
	.attach_dev = smmu_attach_device,
	.detach_dev = smmu_detach_device,
	.map = smmu_map,
	.unmap = smmu_unmap,
	.iova_to_phys = smmu_iova_to_phys,
	.add_device = smmu_add_device,
	.remove_device = smmu_remove_device,
	.pgsize_bitmap = SZ_1G | SZ_2M | SZ_4K,
};

static vmm_irq_return_t smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct smmu_vmsa_device *smmu = dev;
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

static int arm_smmu_device_cfg_probe(struct smmu_vmsa_device *smmu)
{
	unsigned long size;
	struct vmm_devtree_node *node = smmu->node;
	void *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id;

	/* ID0 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID0);
	if (id & ID0_S1TS) {
		vmm_linfo(node->name, "arm-smmu: stage 1 translation\n");
	}

	if (id & ID0_S2TS) {
		vmm_linfo(node->name, "arm-smmu: stage 2 translation\n");
	}

	if (id & ID0_NTS) {
		vmm_linfo(node->name, "arm-smmu: nested translation\n");
	}

	/* Max. number of entries we have for stream matching/indexing */
	size = 1 << ((id >> ID0_NUMSIDB_SHIFT) & ID0_NUMSIDB_MASK);
	smmu->streamid_mask = size - 1;
	if (id & ID0_SMS) {
		size = (id >> ID0_NUMSMRG_SHIFT) & ID0_NUMSMRG_MASK;
		smmu->num_mapping_groups = size;
	} else {
		return VMM_EINVALID;
	}

	/* ID1 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;

	vmm_linfo(node->name, "arm-smmu: num_contexts=%d num_groups=%d "
		  "streamid_mask=0x%x\n", smmu->num_context_banks,
		  smmu->num_mapping_groups, smmu->streamid_mask);

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size *= 2 << smmu->pgshift;
	if (smmu->reg_size != size) {
		vmm_lerror(node->name, "%s: address space size (0x%lx) "
			   "differs from mapped region size (0x%lx)!\n",
			   __func__, size, smmu->reg_size);
		return VMM_EINVALID;
	}

	/* ID2 */
	id = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->ipa_size = size;
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->pa_size = size;

	vmm_linfo(node->name, "arm-smmu: ias_bits=%ld oas_bits=%ld\n",
		  smmu->ipa_size, smmu->pa_size);

	return 0;
}

static void __arm_smmu_tlb_sync(struct smmu_vmsa_device *smmu)
{
	int count = 0;
	void *gr0_base = ARM_SMMU_GR0(smmu);

	vmm_writel_relaxed(0, gr0_base + ARM_SMMU_GR0_sTLBGSYNC);
	while (vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sTLBGSTATUS)
	       & sTLBGSTATUS_GSACTIVE) {
		barrier();
		if (++count == TLB_LOOP_TIMEOUT) {
			vmm_lerror_once(smmu->node->name,
			"TLB sync timed out -- SMMU may be deadlocked\n");
			return;
		}
		vmm_udelay(1);
	}
}

static void arm_smmu_device_reset(struct smmu_vmsa_device *smmu)
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

	/*
	 * Before clearing ARM_MMU500_ACTLR_CPRE, need to
	 * clear CACHE_LOCK bit of ACR first. And, CACHE_LOCK
	 * bit is only present in MMU-500r2 onwards.
	 */
	reg = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_ID7);
	major = (reg >> ID7_MAJOR_SHIFT) & ID7_MAJOR_MASK;
	if (major >= 2) {
		reg = vmm_readl_relaxed(gr0_base + ARM_SMMU_GR0_sACR);
		reg &= ~ARM_MMU500_ACR_CACHE_LOCK;
		vmm_writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_sACR);
	}

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, i);
		vmm_writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
		vmm_writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);
		reg = vmm_readl_relaxed(cb_base + ARM_SMMU_CB_ACTLR);
		reg &= ~ARM_MMU500_ACTLR_CPRE;
		vmm_writel_relaxed(reg, cb_base + ARM_SMMU_CB_ACTLR);
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

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	/* Push the button */
	__arm_smmu_tlb_sync(smmu);
	vmm_writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
}

static int arm_smmu_init(struct vmm_devtree_node *node)
{
	int ret = VMM_OK, i;
	irq_flags_t flags;
	virtual_addr_t va;
	physical_addr_t pa;
	physical_size_t size;
	u32 num_irqs, global_irqs;
	struct smmu_vmsa_device *smmu;

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

	if (smmu->num_context_banks != smmu->num_context_irqs) {
		vmm_lerror(node->name, "%s: number of context banks don't match "
			   "number of context irqs\n", __func__);
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

	smmu->s2crs = vmm_zalloc(smmu->num_mapping_groups *sizeof(*smmu->s2crs));
	if (!smmu->s2crs) {
		vmm_lerror(node->name, "%s: failed to alloc S2CRs\n",
			   __func__);
		ret = VMM_ENOMEM;
		goto fail_free_smrs;
	}

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		ret = vmm_host_irq_register(smmu->irqs[i], "arm-smmu-global",
				           smmu_global_fault, smmu);
		if (ret) {
			vmm_lerror(node->name, "%s: failed to register global "
				   "irq%d (%u)\n", __func__, i, smmu->irqs[i]);
			while (i > 0) {
				vmm_host_irq_unregister(smmu->irqs[i], smmu);
				i--;
			}
			goto fail_free_s2crs;
		}
	}

	arm_smmu_device_reset(smmu);

	vmm_spin_lock_irqsave(&smmu_devices_lock, flags);
	list_add_tail(&smmu->list, &smmu_devices);
	vmm_spin_unlock_irqrestore(&smmu_devices_lock, flags);

	///* Oh, for a proper bus abstraction */
	if (!vmm_iommu_present(&platform_bus))
		vmm_bus_set_iommu(&platform_bus, &smmu_ops);

	vmm_linfo(node->name, "arm-smmu: ready!\n");

	return VMM_OK;

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
VMM_IOMMU_INIT_DECLARE(smmu_v2, "arm,smmu-v2", arm_smmu_init);
VMM_IOMMU_INIT_DECLARE(smmu_400, "arm,mmu-400", arm_smmu_init);
VMM_IOMMU_INIT_DECLARE(smmu_401, "arm,mmu-401", arm_smmu_init);
VMM_IOMMU_INIT_DECLARE(smmu_500, "arm,mmu-500", arm_smmu_init);
