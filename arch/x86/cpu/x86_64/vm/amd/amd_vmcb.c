/**
 * Copyright (c) 2013 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file amd_vmcb.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief AMD Virtual Machine Control Block helper functions.
 */
#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <cpu_features.h>
#include <vm/amd_svm.h>
#include <vm/amd_intercept.h>

void print_vmcb_state (struct vmcb *vmcb)
{
	return;
}

#define BIT_MASK(n)  (~(~0ULL << (n)))
#define SUB_BIT(x, start, len) ((((x) >> (start)) & BIT_MASK(len)))


/* [REF] AMD64 manual vol 2, pp. 373 */
static int check_efer_svme (const struct vmcb *vmcb)
{
	return (!(vmcb->efer & EFER_SVME));
}

static int check_cr0cd_cr0nw (const struct vmcb *vmcb)
{
	return ((!( vmcb->cr0 & X86_CR0_CD )) && (vmcb->cr0 & X86_CR0_NW));
}

static int check_cr0_32_63 (const struct vmcb *vmcb)
{
	return (SUB_BIT(vmcb->cr0, 32,	32));
}

static int check_cr4_11_63 (const struct vmcb *vmcb)
{
	return (SUB_BIT(vmcb->cr4, 11, (u64)53));
}

static int check_dr6_32_63 (const struct vmcb *vmcb)
{
	return (SUB_BIT(vmcb->dr6, 32, 32));
}

static int check_dr7_32_63 (const struct vmcb *vmcb)
{
	return (SUB_BIT(vmcb->dr7, 32, 32));
}

static int check_efer_15_63 (const struct vmcb *vmcb)
{
	return (SUB_BIT(vmcb->efer, 15, 49));
}

static int check_eferlme_cr0pg_cr4pae (const struct vmcb *vmcb)
{
	return ((vmcb->efer & EFER_LME) && (vmcb->cr0 & X86_CR0_PG) &&
		 (!(vmcb->cr4 & X86_CR4_PAE)));
}

static int check_eferlme_cr0pg_cr0pe (const struct vmcb *vmcb)
{
	return ((vmcb->efer & EFER_LME) && (vmcb->cr0 & X86_CR0_PG) &&
		 (!(vmcb->cr0 & X86_CR0_PE)));
}

/* [REF] Code-Segment Register - Long mode */
static int check_eferlme_cr0pg_cr4pae_csl_csd (const struct vmcb *vmcb)
{
	return ((vmcb->efer & EFER_LME) && (vmcb->cr0 & X86_CR0_PG) &&
		 (vmcb->cr4 & X86_CR4_PAE) && (vmcb->cs.attrs.fields.l) && (vmcb->cs.attrs.fields.db));
}

static int check_vmrun_intercept (const struct vmcb *vmcb)
{
	return (!(vmcb->general2_intercepts & INTRCPT_VMRUN));
}

// TODO: HeeDong - Fill the function
static int check_msr_ioio_intercept_tables (const struct vmcb *vmcb)
{
	/* The MSR or IOIO intercept tables extend to a physical address >= the maximum supported physical address */
//	return ( ( vmcb->iopm_base_pa >= ) || ( vmcb->msrpm_base_pa >= ) ); /* [DEBUG] */
	return 0;
}

#if 0
static int
check_misc ( const struct vmcb *vmcb )
{

//	if ( vmcb->cr3 ) { /* Any MBZ (must be zero) bits of CR3 are set */
//		0-2, 5-11		(legacy, non-pae)
//		0-2			(legacy, pae)
//		0-2, 5-11, and 52-63	(long, non-pae)
//	}

	/* Other MBZ bits exist in various registers stored in the VMCB. */


	/* Illegal event injection (vol. 2, p. 468) */

	return 0;
}
#endif

struct consistencty_check {
	int ( *func ) (const struct vmcb *vmcb);
	char *error_msg;
};

void vmcb_check_consistency(struct vmcb *vmcb)
{
	const struct consistencty_check tbl[]
		= { { &check_efer_svme,	  "EFER.SVME is not set.\n" },
		    { &check_cr0cd_cr0nw, "CR0.CD is not set, and CR0.NW is set.\n" },
		    { &check_cr0_32_63,	  "CR0[32:63] are not zero.\n" },
		    { &check_cr4_11_63,	  "CR4[11:63] are not zero.\n" },
		    { &check_dr6_32_63,	  "DR6[32:63] are not zero.\n" },
		    { &check_dr7_32_63,	  "DR7[32:63] are not zero.\n" },
		    { &check_efer_15_63,  "EFER[15:63] are not zero.\n" },
		    { &check_eferlme_cr0pg_cr4pae, "EFER.LME is set, CR0.PG is set, and CR4.PAE is not set.\n" },
		    { &check_eferlme_cr0pg_cr0pe,  "EFER.LME is set, CR0.PG is set, and CR4.PE is not set.\n" },
		    { &check_eferlme_cr0pg_cr4pae_csl_csd, "EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D are set.\n" },
		    { &check_vmrun_intercept, "The VMRUN intercept bit is clear.\n" },
		    { &check_msr_ioio_intercept_tables, "Wrong The MSR or IOIO intercept tables address.\n" } };
	const size_t nelm = sizeof (tbl) / sizeof (struct consistencty_check);

	int i;
	for (i = 0; i < nelm; i++) {
		if ((* tbl[i].func) (vmcb)) {
			vmm_printf(tbl[i].error_msg);
			vmm_printf("Consistency check failed.\n");
		}
	}
}

static void seg_selector_dump(char *name, const struct seg_selector *s)
{
	vmm_printf("%s: sel=%x, attr=%x, limit=%x, base=%x\n",
		name, s->sel, s->attrs.bytes, s->limit,
		(unsigned long long)s->base );
}

void vmcb_dump( const struct vmcb *vmcb )
{
	vmm_printf("Dumping guest's current state\n");
	vmm_printf("Size of VMCB = %x, address = %x\n",
		(int) sizeof(struct vmcb), vmcb);

	vmm_printf("cr_intercepts = %x dr_intercepts = %x exception_intercepts = %x\n",
		vmcb->cr_intercepts, vmcb->dr_intercepts, vmcb->exception_intercepts);
	vmm_printf("general1_intercepts = %x general2_intercepts = %x\n",
		vmcb->general1_intercepts, vmcb->general2_intercepts);
	vmm_printf("iopm_base_pa = %x msrpm_base_pa = %x tsc_offset = %x\n",
		(unsigned long long) vmcb->iopm_base_pa,
		(unsigned long long) vmcb->msrpm_base_pa,
		(unsigned long long) vmcb->tsc_offset);
	vmm_printf("tlb_control = %x vintr = %x interrupt_shadow = %x\n", vmcb->tlb_control,
		(unsigned long long) vmcb->vintr.bytes,
		(unsigned long long) vmcb->interrupt_shadow);
	vmm_printf("exitcode = %x exitintinfo = %x\n",
		(unsigned long long) vmcb->exitcode,
		(unsigned long long) vmcb->exitintinfo.bytes);
	vmm_printf("exitinfo1 = %x exitinfo2 = %x \n",
		(unsigned long long) vmcb->exitinfo1,
		(unsigned long long) vmcb->exitinfo2);
	vmm_printf("np_enable = %x guest_asid = %x\n",
		(unsigned long long) vmcb->np_enable, vmcb->guest_asid);
	vmm_printf("cpl = %x efer = %x star = %x lstar = %x\n",
		vmcb->cpl, (unsigned long long) vmcb->efer,
		(unsigned long long) vmcb->star, (unsigned long long) vmcb->lstar);
	vmm_printf("CR0 = %x CR2 = %x\n",
		(unsigned long long) vmcb->cr0, (unsigned long long) vmcb->cr2);
	vmm_printf("CR3 = %x CR4 = %x\n",
		(unsigned long long) vmcb->cr3, (unsigned long long) vmcb->cr4);
	vmm_printf("RSP = %x  RIP = %x\n",
		(unsigned long long) vmcb->rsp, (unsigned long long) vmcb->rip);
	vmm_printf("RAX = %x  RFLAGS=%x\n",
		(unsigned long long) vmcb->rax, (unsigned long long) vmcb->rflags);
	vmm_printf("DR6 = %x, DR7 = %x\n",
		(unsigned long long) vmcb->dr6, (unsigned long long) vmcb->dr7);
	vmm_printf("CSTAR = %x SFMask = %x\n",
		(unsigned long long) vmcb->cstar, (unsigned long long) vmcb->sfmask);
	vmm_printf("KernGSBase = %x PAT = %x \n",
		(unsigned long long) vmcb->kerngsbase,
		(unsigned long long) vmcb->g_pat);

	/* print out all the selectors */
	seg_selector_dump("CS", &vmcb->cs);
	seg_selector_dump("DS", &vmcb->ds);
	seg_selector_dump("SS", &vmcb->ss);
	seg_selector_dump("ES", &vmcb->es);
	seg_selector_dump("FS", &vmcb->fs);
	seg_selector_dump("GS", &vmcb->gs);
	seg_selector_dump("GDTR", &vmcb->gdtr);
	seg_selector_dump("LDTR", &vmcb->ldtr);
	seg_selector_dump("IDTR", &vmcb->idtr);
	seg_selector_dump("TR", &vmcb->tr);
}

void print_vmexit_exitcode(struct vmcb * vmcb)
{
	vmm_printf("#VMEXIT: ");

	switch (vmcb->exitcode) {
	case VMEXIT_EXCEPTION_PF:
		vmm_printf("EXCP (page fault)"); break;
	case VMEXIT_NPF:
		vmm_printf("NPF (nested-paging: host-level page fault)"); break;
	case VMEXIT_INVALID:
		vmm_printf("INVALID"); break;
	default:
		vmm_printf("%x", ( unsigned long )vmcb->exitcode); break;
	}

	vmm_printf("\n" );
	vmm_printf("exitinfo1 (error_code) = %x, ", vmcb->exitinfo1);
	vmm_printf("exitinfo2 = %x, ", vmcb->exitinfo2);
	vmm_printf("exitINTinfo = %x\n", vmcb->exitintinfo );
}

/*****************************************************/
//manual vol 2 - 8.4.2 Page-Fault Error Code
// Note for NPF: p410 - 15.24.6 Nested versus Guest Page Faults, Fault Ordering
void print_page_errorcode(u64 errcode)
{
    if (errcode & 0x1) {
        vmm_printf("Page fault was caused by a page-protection violation.\n");
    } else {
        vmm_printf("Page fault was caused by a not-present page.\n");
    }

	if (errcode & 0x2) {
        vmm_printf("memory access was write\n");
	} else {
        vmm_printf("memory access was read\n");
    }

	if (errcode & 0x4) {
        vmm_printf("an access in user mode caused the page fault\n");
	} else {
        vmm_printf("an access in supervisor mode caused the page fault\n");
    }

	if (errcode & 0x8)
        vmm_printf("error caused by reading a '1' from reserved field, when CR4.PSE=1 or CR4.PAE=1\n");

	if (errcode & 0x10)
        vmm_printf("error caused by instruction fetch, when EFER.NXE=1 && CR4.PAE=1");
}
