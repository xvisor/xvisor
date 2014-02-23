#ifndef __CONTROL_REG_ACCESS_H
#define __CONTROL_REG_ACCESS_H

/*
 * Volatile isn't enough to prevent the compiler from reordering the
 * read/write functions for the control registers and messing everything up.
 * A memory clobber would solve the problem, but would prevent reordering of
 * all loads stores around it, which can hurt performance. Solution is to
 * use a variable and mimic reads and writes to it to enforce serialization
 */
extern unsigned long __force_order;

static inline unsigned long read_cr0(void)
{
	unsigned long val;
	asm volatile("mov %%cr0,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline void write_cr0(unsigned long val)
{
	asm volatile("mov %0,%%cr0": : "r" (val), "m" (__force_order));
}

static inline unsigned long read_cr2(void)
{
	unsigned long val;
	asm volatile("mov %%cr2,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline void write_cr2(unsigned long val)
{
	asm volatile("mov %0,%%cr2": : "r" (val), "m" (__force_order));
}

static inline unsigned long read_cr3(void)
{
	unsigned long val;
	asm volatile("mov %%cr3,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline void write_cr3(unsigned long val)
{
	asm volatile("mov %0,%%cr3": : "r" (val), "m" (__force_order));
}

static inline unsigned long read_cr4(void)
{
	unsigned long val;
	asm volatile("mov %%cr4,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline unsigned long read_cr4_safe(void)
{
	unsigned long val;
	/* This could fault if %cr4 does not exist. In x86_64, a cr4 always
	 * exists, so it will never fail. */
#ifdef CONFIG_X86_32
        asm volatile("1: mov %%cr4, %0\n"
		     "2:\n"
		     _ASM_EXTABLE(1b, 2b)
		     : "=r" (val), "=m" (__force_order) : "" (0));
#else
	val = read_cr4();
#endif
	return val;
}

static inline void write_cr4(unsigned long val)
{
	asm volatile("mov %0,%%cr4": : "r" (val), "m" (__force_order));
}

#ifdef CONFIG_X86_64
static inline unsigned long read_cr8(void)
{
	unsigned long cr8;
	asm volatile("movq %%cr8,%0" : "=r" (cr8));
	return cr8;
}

static inline void write_cr8(unsigned long val)
{
	asm volatile("movq %0,%%cr8" :: "r" (val) : "memory");
}
#endif

static inline void wbinvd(void)
{
	asm volatile("wbinvd": : :"memory");
}

static inline __always_inline void set_in_cr4 (unsigned long mask)
{
	//mmu_cr4_features |= mask;
	write_cr4(read_cr4() | mask);
}

static inline __always_inline void clear_in_cr4 (unsigned long mask)
{
	//mmu_cr4_features &= ~mask;
	write_cr4(read_cr4() & ~mask);
}

#endif /* __CONTROL_REG_ACCESS_H */
