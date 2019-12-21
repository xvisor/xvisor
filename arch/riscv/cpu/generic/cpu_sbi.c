/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_sbi.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Supervisor binary interface (SBI) helper functions source
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_cpumask.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#include <cpu_sbi.h>
#include <riscv_sbi.h>

static unsigned long sbi_spec_version = SBI_SPEC_VERSION_DEFAULT;

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register ulong a0 asm ("a0") = (ulong)(arg0);
	register ulong a1 asm ("a1") = (ulong)(arg1);
	register ulong a2 asm ("a2") = (ulong)(arg2);
	register ulong a3 asm ("a3") = (ulong)(arg3);
	register ulong a4 asm ("a4") = (ulong)(arg4);
	register ulong a5 asm ("a5") = (ulong)(arg5);
	register ulong a6 asm ("a6") = (ulong)(fid);
	register ulong a7 asm ("a7") = (ulong)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

int sbi_err_map_xvisor_errno(int err)
{
	switch (err) {
	case SBI_SUCCESS:
		return 0;
	case SBI_ERR_DENIED:
		return VMM_EACCESS;
	case SBI_ERR_INVALID_PARAM:
		return VMM_EINVALID;
	case SBI_ERR_INVALID_ADDRESS:
		return VMM_EFAULT;
	case SBI_ERR_NOT_SUPPORTED:
	case SBI_ERR_FAILURE:
	default:
		return VMM_ENOTSUPP;
	};
}

void sbi_cpumask_to_hartmask(const struct vmm_cpumask *cmask,
			     struct vmm_cpumask *hmask)
{
	int rc;
	u32 cpu;
	unsigned long hart;

	if (!cmask || !hmask)
		return;

	vmm_cpumask_clear(hmask);
	for_each_cpu(cpu, cmask) {
		rc = vmm_smp_map_hwid(cpu, &hart);
		if (rc || (CONFIG_CPU_COUNT <= hart)) {
			vmm_lwarning("SBI", "invalid hart=%lu for cpu=%d\n",
				     hart, cpu);
			continue;
		}
		vmm_cpumask_set_cpu(hart, hmask);
	}
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

void sbi_shutdown(void)
{
	sbi_ecall(SBI_EXT_0_1_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
}

void sbi_clear_ipi(void)
{
	sbi_ecall(SBI_EXT_0_1_CLEAR_IPI, 0, 0, 0, 0, 0, 0, 0);
}

void sbi_send_ipi(const unsigned long *hart_mask)
{
	sbi_ecall(SBI_EXT_0_1_SEND_IPI, 0,
		  (unsigned long)hart_mask, 0, 0, 0, 0, 0);
}

void sbi_set_timer(u64 stime_value)
{
#ifdef CONFIG_64BIT
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value, 0, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
#endif
}

void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_FENCE_I, 0,
		  (unsigned long)hart_mask, 0, 0, 0, 0, 0);
}

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start,
			   unsigned long size)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA, 0,
		  (unsigned long)hart_mask, start, size, 0, 0, 0);
}

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start,
				unsigned long size,
				unsigned long asid)
{
	sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID, 0,
		  (unsigned long)hart_mask, start, size, asid, 0, 0);
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

int sbi_probe_extension(long extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error && ret.value)
		return ret.value;

	return VMM_ENOTSUPP;
}

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

int __init sbi_init(void)
{
	int ret;

	ret = sbi_get_spec_version();
	if (ret > 0)
		sbi_spec_version = ret;

	vmm_init_printf("SBI specification v%lu.%lu detected\n",
			sbi_major_version(), sbi_minor_version());

	if (!sbi_spec_is_0_1()) {
		vmm_init_printf("SBI implementation ID=0x%lx Version=0x%lx\n",
			sbi_get_firmware_id(), sbi_get_firmware_version());
	}

	return 0;
}
