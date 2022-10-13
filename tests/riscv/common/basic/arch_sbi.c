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
 * @file arch_sbi.c
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief Supervisor binary interface (SBI) source
 */

#include <basic_stdio.h>
#include <basic_string.h>
#include <arch_sbi.h>

struct sbiret {
	long error;
	long value;
};

static struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
				unsigned long arg1, unsigned long arg2,
				unsigned long arg3, unsigned long arg4,
				unsigned long arg5)
{
	struct sbiret ret;

	register unsigned long a0 asm ("a0") = (arg0);
	register unsigned long a1 asm ("a1") = (arg1);
	register unsigned long a2 asm ("a2") = (arg2);
	register unsigned long a3 asm ("a3") = (arg3);
	register unsigned long a4 asm ("a4") = (arg4);
	register unsigned long a5 asm ("a5") = (arg5);
	register unsigned long a6 asm ("a6") = (fid);
	register unsigned long a7 asm ("a7") = (ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

static int sbi_probe_extension(long extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error && ret.value)
		return ret.value;

	return -1;
}

static unsigned long sbi_spec_version = SBI_SPEC_VERSION_DEFAULT;

int sbi_spec_is_0_1(void)
{
	return (sbi_spec_version == SBI_SPEC_VERSION_DEFAULT) ? 1 : 0;
}

unsigned long sbi_major_version(void)
{
	return (sbi_spec_version >> SBI_SPEC_VERSION_MAJOR_SHIFT) &
		SBI_SPEC_VERSION_MAJOR_MASK;
}

unsigned long sbi_minor_version(void)
{
	return sbi_spec_version & SBI_SPEC_VERSION_MINOR_MASK;
}

void sbi_console_putchar(int ch)
{
	sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

int sbi_console_getchar(void)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);

	return ret.error;
}

void sbi_set_timer(u64 stime_value)
{
#if __riscv_xlen == 32
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value, 0, 0, 0, 0, 0);
#endif
}

void sbi_clear_timer(void)
{
#if __riscv_xlen == 32
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, -1UL, -1UL, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, -1UL, 0, 0, 0, 0, 0);
#endif
}

void sbi_clear_ipi(void)
{
	sbi_ecall(SBI_EXT_0_1_CLEAR_IPI, 0, 0, 0, 0, 0, 0, 0);
}

void sbi_send_ipi(const unsigned long *hart_mask)
{
	sbi_ecall(SBI_EXT_0_1_SEND_IPI, 0, (unsigned long)hart_mask,
		  0, 0, 0, 0, 0);
}

void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_FENCE_I, 0,
		  (unsigned long)hart_mask, 0, 0, 0, 0, 0);
}

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start, unsigned long size)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA, 0,
		  (unsigned long)hart_mask, start, size, 0, 0, 0);
}

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start, unsigned long size,
				unsigned long asid)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID, 0,
		  (unsigned long)hart_mask, start, size, asid, 0, 0);
}

static void __sbi_srst_reset(unsigned long type, unsigned long reason)
{
	sbi_ecall(SBI_EXT_SRST, SBI_EXT_SRST_RESET, type, reason,
		  0, 0, 0, 0);
}

void sbi_shutdown(void)
{
	/* Must have SBI SRST extension */
	if (!sbi_spec_is_0_1() && (sbi_probe_extension(SBI_EXT_SRST) > 0)) {
		__sbi_srst_reset(SBI_SRST_RESET_TYPE_SHUTDOWN,
				 SBI_SRST_RESET_REASON_NONE);
	} else {
		sbi_ecall(SBI_EXT_0_1_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
	}
}

void sbi_reset(void)
{
	/* Must have SBI SRST extension */
	if (!sbi_spec_is_0_1() && (sbi_probe_extension(SBI_EXT_SRST) > 0)) {
		__sbi_srst_reset(SBI_SRST_RESET_TYPE_COLD_REBOOT,
				 SBI_SRST_RESET_REASON_NONE);
	} else {
		sbi_ecall(SBI_EXT_0_1_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
	}
}

#define SBI_EXT_XVISOR			(SBI_EXT_FIRMWARE_START + 0x2)
#define SBI_EXT_XVISOR_ISA_EXT		0x0

/*
 * Increse this to higher value as kernel support more ISA extensions.
 */
#define RISCV_ISA_EXT_MAX	64

/* The base ID for multi-letter ISA extensions */
#define RISCV_ISA_EXT_BASE 26

/*
 * This enum represent the logical ID for each multi-letter
 * RISC-V ISA extension. The logical ID should start from
 * RISCV_ISA_EXT_BASE and must not exceed RISCV_ISA_EXT_MAX.
 * 0-25 range is reserved for single letter extensions while
 * all the multi-letter extensions should define the next
 * available logical extension id.
 */
enum riscv_isa_ext_id {
	RISCV_ISA_EXT_SSAIA = RISCV_ISA_EXT_BASE,
	RISCV_ISA_EXT_SMAIA,
	RISCV_ISA_EXT_SSTC,
	RISCV_ISA_EXT_ID_MAX = RISCV_ISA_EXT_MAX,
};

unsigned long sbi_xvisor_isa_string(char *out_isa, unsigned long max_len)
{
	struct sbiret ret;
	unsigned long pos = 0;
	size_t i, valid_isa_len;
	const char *valid_isa_order = "iemafdqclbjtpvnhkorwyg";

	if (!out_isa || (max_len - pos) < 5)
		return pos;

	basic_memset(out_isa, 0, max_len);

#if __riscv_xlen == 64
	basic_strcpy(&out_isa[pos], "rv64");
#elif __riscv_xlen == 32
	basic_strcpy(&out_isa[pos], "rv32");
#else
#error "Unexpected __riscv_xlen"
#endif
	pos += 4;

	if (sbi_spec_is_0_1() || (sbi_probe_extension(SBI_EXT_XVISOR) <= 0)) {
		if (max_len > 3) {
			out_isa[pos++] = 'g';
			out_isa[pos++] = 'c';
			pos += 2;
			return pos;
		}
	}

	valid_isa_len = basic_strlen(valid_isa_order);
	for (i = 0; i < valid_isa_len && pos < max_len; i++) {
		ret = sbi_ecall(SBI_EXT_XVISOR, SBI_EXT_XVISOR_ISA_EXT,
				valid_isa_order[i] - 'a', 0, 0, 0, 0, 0);
		if (ret.error || !ret.value)
			continue;

		out_isa[pos++] = valid_isa_order[i];
	}

#define SET_ISA_EXT_MAP(__name, __bit)					\
	do {								\
		ret = sbi_ecall(SBI_EXT_XVISOR, SBI_EXT_XVISOR_ISA_EXT,	\
				__bit, 0, 0, 0, 0, 0);			\
		if (!ret.error && ret.value) {				\
			basic_strcat(&out_isa[pos], "_" __name);	\
			pos += basic_strlen("_" __name);		\
		}							\
	} while (0)							\

	SET_ISA_EXT_MAP("sstc", RISCV_ISA_EXT_SSTC);
#undef SET_ISA_EXT_MAP

	return pos;
}

static long sbi_ext_base_func(long fid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, fid, 0, 0, 0, 0, 0, 0);
	if (!ret.error)
		return ret.value;
	else
		return ret.error;
}

#define sbi_get_spec_version()		\
	sbi_ext_base_func(SBI_EXT_BASE_GET_SPEC_VERSION)

#define sbi_get_firmware_id()		\
	sbi_ext_base_func(SBI_EXT_BASE_GET_IMP_ID)

#define sbi_get_firmware_version()	\
	sbi_ext_base_func(SBI_EXT_BASE_GET_IMP_VERSION)

void sbi_init(void)
{
	int ret;

	ret = sbi_get_spec_version();
	if (ret > 0)
		sbi_spec_version = ret;

	basic_printf("RISC-V SBI specification v%d.%d detected\n",
		     (unsigned int)sbi_major_version(),
		     (unsigned int)sbi_minor_version());

	if (!sbi_spec_is_0_1()) {
		basic_printf("RISC-V SBI implementation ID=0x%lx Version=0x%lx\n",
			     sbi_get_firmware_id(),
			     sbi_get_firmware_version());
	}

	basic_printf("\n");
}
