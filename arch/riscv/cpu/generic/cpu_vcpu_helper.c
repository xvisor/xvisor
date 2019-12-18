/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cpu_vcpu_helper.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_devtree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_pagepool.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>
#include <arch_guest.h>
#include <arch_vcpu.h>
#include <vio/vmm_vserial.h>
#include <libs/mathlib.h>

#include <cpu_hwcap.h>
#include <cpu_tlb.h>
#include <cpu_sbi.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_fp.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_timer.h>
#include <cpu_guest_serial.h>
#include <riscv_csr.h>
#include <riscv_lrsc.h>

#define RISCV_ISA_ALLOWED	(riscv_isa_extension_mask(a) | \
				 riscv_isa_extension_mask(c) | \
				 riscv_isa_extension_mask(d) | \
				 riscv_isa_extension_mask(f) | \
				 riscv_isa_extension_mask(i) | \
				 riscv_isa_extension_mask(m) | \
				 riscv_isa_extension_mask(s) | \
				 riscv_isa_extension_mask(u))

static char *guest_fdt_find_serial_node(char *guest_name)
{
	char *serial = NULL;
	char chosen_node_path[VMM_FIELD_NAME_SIZE];
	struct vmm_devtree_node *chosen_node;

	/* Process attributes in chosen node */
	vmm_snprintf(chosen_node_path, sizeof(chosen_node_path), "/%s/%s/%s",
		     VMM_DEVTREE_GUESTINFO_NODE_NAME, guest_name,
		     VMM_DEVTREE_CHOSEN_NODE_NAME);
	chosen_node = vmm_devtree_getnode(chosen_node_path);
	if (chosen_node) {
		/* Process console device passed via chosen node */
		vmm_devtree_read_string(chosen_node,
					VMM_DEVTREE_STDOUT_ATTR_NAME,
					(const char **)&serial);
	}
	vmm_devtree_dref_node(chosen_node);

	return serial;
}

static int guest_vserial_notification(struct vmm_notifier_block *nb,
					unsigned long evt, void *data)
{
	int ret = NOTIFY_OK;
	struct vmm_vserial_event *e = data;
	struct riscv_guest_serial *gserial = container_of(nb,
						struct riscv_guest_serial,
						vser_client);
	if (evt ==  VMM_VSERIAL_EVENT_CREATE) {
		if (!strcmp(e->vser->name, gserial->name)) {
			gserial->vserial = e->vser;
		}
	} else if (evt == VMM_VSERIAL_EVENT_DESTROY) {
		if (!strcmp(e->vser->name, gserial->name))
			gserial->vserial = NULL;
	} else
		ret = NOTIFY_DONE;
	return ret;
}

int arch_guest_init(struct vmm_guest *guest)
{
	struct riscv_guest_priv *priv;
	struct riscv_guest_serial *gserial;
	char *sname;
	int rc;

	if (!guest->reset_count) {
		if (!riscv_isa_extension_available(NULL, h) ||
		    !sbi_has_0_2_rfence())
			return VMM_EINVALID;

		guest->arch_priv = vmm_malloc(sizeof(struct riscv_guest_priv));
		if (!guest->arch_priv) {
			return VMM_ENOMEM;
		}
		priv = riscv_guest_priv(guest);

		priv->time_delta = vmm_manager_guest_reset_timestamp(guest);
		priv->time_delta *= vmm_timer_clocksource_frequency();
		priv->time_delta = -udiv64(priv->time_delta, 1000000000ULL);

		priv->pgtbl = cpu_mmu_pgtbl_alloc(PGTBL_STAGE2);
		if (!priv->pgtbl) {
			vmm_free(guest->arch_priv);
			guest->arch_priv = NULL;
			return VMM_ENOMEM;
		}
		priv->guest_serial = vmm_malloc(sizeof(struct riscv_guest_serial));
		if (!priv->guest_serial)
			return VMM_ENOMEM;

		gserial = riscv_guest_serial(guest);
		sname = guest_fdt_find_serial_node(guest->name);
		if (sname) {
			strlcpy(gserial->name, guest->name,
				sizeof(gserial->name));
			strlcat(gserial->name, "/", sizeof(gserial->name));
			strlcat(gserial->name, sname, sizeof(gserial->name));
		}
		gserial->vser_client.notifier_call = &guest_vserial_notification;
		gserial->vser_client.priority = 0;
		rc = vmm_vserial_register_client(&gserial->vser_client);
		if (rc)
			return rc;
	}

	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	int rc;
	struct riscv_guest_serial *gs;

	if (guest->arch_priv) {
		gs = riscv_guest_serial(guest);

		if ((rc = cpu_mmu_pgtbl_free(riscv_guest_priv(guest)->pgtbl))) {
			return rc;
		}
		if (gs) {
			vmm_vserial_unregister_client(&gs->vser_client);
			vmm_free(gs);
		}
		vmm_free(guest->arch_priv);
	}

	return VMM_OK;
}

int arch_guest_add_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_guest_del_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	int rc = VMM_OK;
	const char *attr;
	virtual_addr_t sp, sp_exec;

	/* Determine stack location */
	if (vcpu->is_normal) {
		sp = 0;
		sp_exec = vcpu->stack_va + vcpu->stack_sz;
		sp_exec = sp_exec & ~(__SIZEOF_POINTER__ - 1);
	} else {
		sp = vcpu->stack_va + vcpu->stack_sz;
		sp = sp & ~(__SIZEOF_POINTER__ - 1);
		if (!vcpu->reset_count) {
			/* First time allocate exception stack */
			sp_exec = vmm_pagepool_alloc(VMM_PAGEPOOL_NORMAL,
				VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));
			if (!sp_exec) {
				return VMM_ENOMEM;
			}
			sp_exec += CONFIG_IRQ_STACK_SIZE;
		} else {
			sp_exec = riscv_regs(vcpu)->sp_exec;
		}
	}

	/* For both Orphan & Normal VCPUs */
	memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));
	riscv_regs(vcpu)->sepc = vcpu->start_pc;
	riscv_regs(vcpu)->sstatus = SSTATUS_SPP | SSTATUS_SPIE;
	riscv_regs(vcpu)->sp = sp;
	riscv_regs(vcpu)->sp_exec = sp_exec;
	riscv_regs(vcpu)->hstatus = 0;

	/* For Orphan VCPUs we are done */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	/* Following initialization for normal VCPUs only */

	/* First time initialization of private context */
	if (!vcpu->reset_count) {
		/* Check allowed compatible string */
		rc = vmm_devtree_read_string(vcpu->node,
				VMM_DEVTREE_COMPATIBLE_ATTR_NAME, &attr);
		if (rc) {
			goto done;
		}
		if (strcmp(attr, "riscv,generic") != 0) {
			rc = VMM_EINVALID;
			goto done;
		}

		/* Alloc private context */
		vcpu->arch_priv = vmm_zalloc(sizeof(struct riscv_priv));
		if (!vcpu->arch_priv) {
			rc = VMM_ENOMEM;
			goto done;
		}

		/* Set register width */
		riscv_priv(vcpu)->xlen = riscv_xlen;

		/* Allocate ISA feature bitmap */
		riscv_priv(vcpu)->isa =
			vmm_zalloc(bitmap_estimate_size(RISCV_ISA_EXT_MAX));
		if (!riscv_priv(vcpu)->isa) {
			vmm_free(vcpu->arch_priv);
			vcpu->arch_priv = NULL;
			rc = VMM_ENOMEM;
			goto done;
		}

		/* Parse VCPU ISA string */
		attr = NULL;
		rc = vmm_devtree_read_string(vcpu->node, "riscv,isa", &attr);
		if (rc || !attr) {
			rc = VMM_EINVALID;
			goto done;
		}
		rc = riscv_isa_parse_string(attr, &riscv_priv(vcpu)->xlen,
					    riscv_priv(vcpu)->isa,
					    RISCV_ISA_EXT_MAX);
		if (rc) {
			goto done;
		}
		if (riscv_priv(vcpu)->xlen > riscv_xlen) {
			rc = VMM_EINVALID;
			goto done;
		}
		riscv_priv(vcpu)->isa[0] &= RISCV_ISA_ALLOWED;
	}

	/* Set a0 to VCPU sub-id (i.e. virtual HARTID) */
	riscv_regs(vcpu)->a0 = vcpu->subid;

	/* Update HSTATUS */
	riscv_regs(vcpu)->hstatus |= HSTATUS_SP2V;
	riscv_regs(vcpu)->hstatus |= HSTATUS_SP2P;
	riscv_regs(vcpu)->hstatus |= HSTATUS_SPV;

	/* TODO: Update HSTATUS.VSXL on non-32bit Host */

	/* Update VCPU CSRs */
	riscv_priv(vcpu)->hie = 0;
	riscv_priv(vcpu)->hip = 0;
	riscv_priv(vcpu)->vsstatus = 0;
	riscv_priv(vcpu)->vstvec = 0;
	riscv_priv(vcpu)->vsscratch = 0;
	riscv_priv(vcpu)->vsepc = 0;
	riscv_priv(vcpu)->vscause = 0;
	riscv_priv(vcpu)->vstval = 0;
	riscv_priv(vcpu)->vsatp = 0;

	/* Initialize FP state */
	cpu_vcpu_fp_init(vcpu);

	riscv_timer_event_init(vcpu, &riscv_timer_priv(vcpu));
done:
	return rc;
}

int arch_vcpu_deinit(struct vmm_vcpu *vcpu)
{
	int rc;
	virtual_addr_t sp_exec;

	/* Free-up exception stack for Orphan VCPU */
	if (!vcpu->is_normal) {
		sp_exec = riscv_regs(vcpu)->sp_exec - CONFIG_IRQ_STACK_SIZE;
		vmm_pagepool_free(VMM_PAGEPOOL_NORMAL, sp_exec,
				  VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));
	}

	/* For both Orphan & Normal VCPUs */

	/* Clear arch registers */
	memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));

	/* For Orphan VCPUs do nothing else */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	rc = riscv_timer_event_deinit(vcpu, &riscv_timer_priv(vcpu));
	if (rc)
		return rc;

	/* Free ISA bitmap */
	vmm_free(riscv_priv(vcpu)->isa);
	riscv_priv(vcpu)->isa = NULL;

	/* Free private context */
	vmm_free(vcpu->arch_priv);
	vcpu->arch_priv = NULL;

	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu,
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	struct riscv_priv *priv;

	if (tvcpu) {
		memcpy(riscv_regs(tvcpu), regs, sizeof(*regs));
		if (tvcpu->is_normal) {
			priv = riscv_priv(tvcpu);
			priv->hie = csr_read(CSR_HIE);
			priv->hip = csr_read(CSR_HIP);
			priv->vsstatus = csr_read(CSR_VSSTATUS);
			priv->vstvec = csr_read(CSR_VSTVEC);
			priv->vsscratch = csr_read(CSR_VSSCRATCH);
			priv->vsepc = csr_read(CSR_VSEPC);
			priv->vscause = csr_read(CSR_VSCAUSE);
			priv->vstval = csr_read(CSR_VSTVAL);
			priv->vsatp = csr_read(CSR_VSATP);
			cpu_vcpu_fp_save(tvcpu, regs);
		}
		clrx();
	}

	memcpy(regs, riscv_regs(vcpu), sizeof(*regs));
	if (vcpu->is_normal) {
		priv = riscv_priv(vcpu);
#ifdef CONFIG_64BIT
		csr_write(CSR_HTIMEDELTA,
			riscv_guest_priv(vcpu->guest)->time_delta);
#else
		csr_write(CSR_HTIMEDELTA,
			(u32)(riscv_guest_priv(vcpu->guest)->time_delta));
		csr_write(CSR_HTIMEDELTAH,
			(u32)(riscv_guest_priv(vcpu->guest)->time_delta >> 32));
#endif
		csr_write(CSR_HIE, priv->hie);
		csr_write(CSR_HIP, priv->hip);
		csr_write(CSR_VSSTATUS, priv->vsstatus);
		csr_write(CSR_VSTVEC, priv->vstvec);
		csr_write(CSR_VSSCRATCH, priv->vsscratch);
		csr_write(CSR_VSEPC, priv->vsepc);
		csr_write(CSR_VSCAUSE, priv->vscause);
		csr_write(CSR_VSTVAL, priv->vstval);
		csr_write(CSR_VSATP, priv->vsatp);
		cpu_vcpu_fp_restore(vcpu, regs);
		if (CONFIG_MAX_GUEST_COUNT <= (1UL << riscv_vmid_bits)) {
			cpu_mmu_stage2_change_pgtbl(vcpu->guest->id,
					riscv_guest_priv(vcpu->guest)->pgtbl);
		} else {
			cpu_mmu_stage2_change_pgtbl(0,
					riscv_guest_priv(vcpu->guest)->pgtbl);
			__hfence_gvma_all();
		}
	}
}

void arch_vcpu_post_switch(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs)
{
	/* Nothing to do here */
}

void cpu_vcpu_dump_general_regs(struct vmm_chardev *cdev,
				arch_regs_t *regs)
{
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "       zero", regs->zero, "         ra", regs->ra);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         sp", regs->sp, "         gp", regs->gp);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         tp", regs->tp, "         s0", regs->s0);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         s1", regs->s1, "         a0", regs->a0);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         a1", regs->a1, "         a2", regs->a2);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         a3", regs->a3, "         a4", regs->a4);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         a5", regs->a5, "         a6", regs->a6);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         a7", regs->a7, "         s2", regs->s2);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         s3", regs->s3, "         s4", regs->s4);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         s5", regs->s5, "         s6", regs->s6);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         s7", regs->s7, "         s8", regs->s8);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         s9", regs->s9, "        s10", regs->s10);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "        s11", regs->s11, "         t0", regs->t0);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         t1", regs->t1, "         t2", regs->t2);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         t3", regs->t3, "         t4", regs->t4);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "         t5", regs->t5, "         t6", regs->t6);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "       sepc", regs->sepc, "    sstatus", regs->sstatus);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    hstatus", regs->hstatus, "    sp_exec", regs->sp_exec);
}

void cpu_vcpu_dump_private_regs(struct vmm_chardev *cdev,
				struct vmm_vcpu *vcpu)
{
	int rc;
	char isa[128];
	struct riscv_priv *priv = riscv_priv(vcpu);
	struct riscv_guest_priv *gpriv = riscv_guest_priv(vcpu->guest);

	rc = riscv_isa_populate_string(priv->xlen, priv->isa,
					isa, sizeof(isa));
	if (rc) {
		vmm_cprintf(cdev, "Failed to populate ISA string\n");
		return;
	}

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, " %s=%s\n",
		    "        isa", isa);
	vmm_cprintf(cdev, "\n");
#ifdef CONFIG_64BIT
	vmm_cprintf(cdev, "%s=0x%"PRIADDR"\n",
		    " htimedelta", (ulong)gpriv->time_delta);
#else
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    " htimedelta", (ulong)(gpriv->time_delta),
		    "htimedeltah", (ulong)(gpriv->time_delta >> 32));
#endif
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "        hie", priv->hie, "        hip", priv->hip);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "   vsstatus", priv->vsstatus, "     vstvec", priv->vstvec);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "  vsscratch", priv->vsscratch, "      vsepc", priv->vsepc);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    vscause", priv->vscause, "     vstval", priv->vstval);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR"\n",
		    "      vsatp", priv->vsatp);

	cpu_vcpu_fp_dump_regs(cdev, vcpu);
}

void cpu_vcpu_dump_exception_regs(struct vmm_chardev *cdev,
				  unsigned long scause,
				  unsigned long stval,
				  unsigned long htval)
{
	vmm_cprintf(cdev, "%s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "   scause", scause, "    stval", stval);
	vmm_cprintf(cdev, "%s=0x%"PRIADDR"\n",
		    "    htval", htval);
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	cpu_vcpu_dump_general_regs(cdev, riscv_regs(vcpu));

	if (vcpu->is_normal) {
		cpu_vcpu_dump_private_regs(cdev, vcpu);
	}
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
