#ifndef _ASM_SWAB_H
#define _ASM_SWAB_H

#include <vmm_host_io.h>

#if defined(CONFIG_CPU_BE)
#define swab16(data)	vmm_cpu_to_le16(data)
#define swab32(data)	vmm_cpu_to_le32(data)
#define swab64(data)	vmm_cpu_to_le64(data)
#endif

#if defined(CONFIG_CPU_LE)
#define swab16(data)	vmm_cpu_to_be16(data)
#define swab32(data)	vmm_cpu_to_be32(data)
#define swab64(data)	vmm_cpu_to_be64(data)
#endif

#endif
