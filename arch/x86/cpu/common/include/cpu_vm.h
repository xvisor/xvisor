#ifndef __CPU_VM_H__
#define __CPU_VM_H__

#include <multiboot.h>
#include <vm/amd_vmcb.h>
#include <processor_flags.h>
#include <cpu_features.h>
#include <vmm_types.h>
#include <cpu_pgtbl_helper.h>
#include <libs/bitmap.h>

enum {
	VM_LOG_LVL_ERR,
	VM_LOG_LVL_INFO,
	VM_LOG_LVL_DEBUG,
	VM_LOG_LVL_VERBOSE
};
extern int vm_default_log_lvl;
#define VM_LOG(lvl, fmt, args...)					\
	do {								\
		if (VM_LOG_##lvl <= vm_default_log_lvl) {		\
			vmm_printf("(%s:%d) " fmt, __func__,		\
				   __LINE__, ##args);			\
		}							\
	}while(0);

enum guest_regs {
	GUEST_REGS_RAX = 0,
	GUEST_REGS_RCX = 1,
	GUEST_REGS_RDX = 2,
	GUEST_REGS_RBX = 3,
	GUEST_REGS_RSP = 4,
	GUEST_REGS_RBP = 5,
	GUEST_REGS_RSI = 6,
	GUEST_REGS_RDI = 7,
	GUEST_REGS_R8  = 8,
	GUEST_REGS_R9  = 9,
	GUEST_REGS_R10 = 10,
	GUEST_REGS_R11 = 11,
	GUEST_REGS_R12 = 12,
	GUEST_REGS_R13 = 13,
	GUEST_REGS_R14 = 14,
	GUEST_REGS_R15 = 15,
	GUEST_REGS_RIP,
	NR_GUEST_REGS
};

#define USER_CMD_ENABLE		0
#define USER_CMD_DISABLE	1
#define USER_CMD_TEST		9

#define USER_ITC_SWINT		(1 << 0)
#define USER_ITC_TASKSWITCH	(1 << 1)
#define USER_ITC_SYSCALL	(1 << 2)
#define USER_ITC_IRET		(1 << 3)
#define USER_SINGLE_STEPPING	(1 << 4)
#define USER_UNPACK		(1 << 5)
#define USER_ITC_ALL		(0xFF)

#define USER_TEST_SWITCHMODE	1

#define	GUEST_PADDR_MBI	0x2d0e0UL

#define IO_INTCPT_TBL_SZ	(12 << 10)
#define MSR_INTCPT_TBL_SZ	(8 << 10)

/**
 * \define The list of pages which are used in page tables itself.
 *
 * This is a slab of pages to be used in 2 fold page tables for
 * 32-bit guest. We just use 128 pages to map at most 512 mb of
 * a 32-bit guest. If the working set of guest is more than this
 * the thrashing will happen. We will kick off some used entries
 * to make room for new ones.
 */
#define NR_32BIT_PGLIST_PAGES	(128)

struct vcpu_intercept_table {
	physical_addr_t io_table_phys;
	physical_addr_t msr_table_phys;
	virtual_addr_t io_table_virt;
	virtual_addr_t msr_table_virt;
};

struct vcpu_hw_context {
	struct vmcb *vmcb;
	struct vmcs *vmcs;
	struct vmm_vcpu *assoc_vcpu; /**< vCPU associated to this hardware context */
	u64 g_regs[NR_GUEST_REGS];

	unsigned int asid;
	unsigned long n_cr3;  /* [Note] When #VMEXIT occurs with
			       * nested paging enabled, hCR3 is not
			       * saved back into the VMCB (vol2 p. 409)???*/
	struct page_table *shadow_pgt; /**< Shadow page table when EPT/NPT is not available in chip */
	union page32 *shadow32_pg_list; /**< Page list for 32-bit guest and paged real mode. */
	union page32 *shadow32_pgt; /**<32-bit page table */
	DEFINE_BITMAP(shadow32_pg_map, NR_32BIT_PGLIST_PAGES);
	u32 pgmap_free_cache;

	struct vcpu_intercept_table icept_table;

	/* Intel VMX only */
	unsigned int		msr_count;
	struct vmx_msr_entry	*msr_area;

	unsigned int		host_msr_count;
	struct vmx_msr_entry	*host_msr_area;

	int itc_flag;  /* flags specifying which interceptions were
			  registered for this vm. */
	int itc_skip_flag;
	u64 guest_start_pc; /* Guest will start execution from here (comes from DTS) */
	physical_addr_t vmcb_pa;

	/* on & exit handler */
	void (*vcpu_run) (struct vcpu_hw_context *context);
	void (*vcpu_exit) (struct vcpu_hw_context *context);
	void (*vcpu_emergency_shutdown)(struct vcpu_hw_context *context);
};

#define VMM_CS32	8   /* entry 1 of gdt ?? Xvisor also uses it. FIXME: */
#define VMM_DS32	16  /* entry 2 of gdt ?? Xvisor also uses it. FIXME: */
#define VMM_CS64	40  /* entry 7 of gdt */
#define VMM_DS64	56  /* entry 5 of gdt */

struct cpuid_response {
	u32 resp_eax;
	u32 resp_ebx;
	u32 resp_ecx;
	u32 resp_edx;
};

/*
 * Emulated CPU information for guest.
 * Contains MSR, related vm control block, etc.
 */
struct x86_vcpu_priv {
	u64 capabilities;
	struct cpuid_response extended_funcs[CPUID_EXTENDED_FUNC_LIMIT-CPUID_EXTENDED_BASE];
	struct cpuid_response standard_funcs[CPUID_BASE_FUNC_LIMIT];
	struct vcpu_hw_context *hw_context;
};

#define x86_vcpu_priv(vcpu) ((struct x86_vcpu_priv *)((vcpu)->arch_priv))

extern void print_page_errorcode(u64 errcode);

extern physical_addr_t cpu_create_vcpu_intercept_table(size_t size, virtual_addr_t *tbl_vaddr);
extern int cpu_free_vcpu_intercept_table(virtual_addr_t vaddr, size_t size);
extern void cpu_disable_vcpu_intercept(struct vcpu_hw_context *context, int flags);
extern void cpu_enable_vcpu_intercept(struct vcpu_hw_context *context, int flags);
extern int cpu_init_vcpu_hw_context(struct cpuinfo_x86 *cpuinfo, struct vcpu_hw_context *context);
extern void cpu_boot_vcpu(struct vcpu_hw_context *context);

extern int cpu_enable_vm_extensions(struct cpuinfo_x86 *cpuinfo);

#endif /* __CPU_VM_H__ */
