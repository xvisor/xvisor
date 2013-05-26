#ifndef _ASM_BYTEORDER_H
#define _ASM_BYTEORDER_H

#include <vmm_host_io.h>

#if defined(CONFIG_CPU_BE)

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __BIG_ENDIAN_BITFIELD
#define __BIG_ENDIAN_BITFIELD
#endif

#endif

#if defined(CONFIG_CPU_LE)

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __LITTLE_ENDIAN_BITFIELD
#define __LITTLE_ENDIAN_BITFIELD
#endif

#endif

/** Endianness related helper macros */
#define cpu_to_le16(data)	vmm_cpu_to_le16(data)
#define le16_to_cpu(data)	vmm_le16_to_cpu(data)
#define cpu_to_be16(data)	vmm_cpu_to_be16(data)
#define be16_to_cpu(data)	vmm_be16_to_cpu(data)
#define cpu_to_le32(data)	vmm_cpu_to_le32(data)
#define le32_to_cpu(data)	vmm_le32_to_cpu(data)
#define cpu_to_be32(data)	vmm_cpu_to_be32(data)
#define be32_to_cpu(data)	vmm_be32_to_cpu(data)
#define cpu_to_le64(data)	vmm_cpu_to_le64(data)
#define le64_to_cpu(data)	vmm_le64_to_cpu(data)
#define cpu_to_be64(data)	vmm_cpu_to_be64(data)
#define be64_to_cpu(data)	vmm_be64_to_cpu(data)

#define cpup_to_le16(p)		vmm_cpu_to_le16(*((u16 *)(p)))
#define le16_to_cpup(p)		vmm_le16_to_cpu(*((u16 *)(p)))
#define cpup_to_be16(p)		vmm_cpu_to_be16(*((u16 *)(p)))
#define be16_to_cpup(p)		vmm_be16_to_cpu(*((u16 *)(p)))
#define cpup_to_le32(p)		vmm_cpu_to_le32(*((u32 *)(p)))
#define le32_to_cpup(p)		vmm_le32_to_cpu(*((u32 *)(p)))
#define cpup_to_be32(p)		vmm_cpu_to_be32(*((u32 *)(p)))
#define be32_to_cpup(p)		vmm_be32_to_cpu(*((u32 *)(p)))
#define cpup_to_le64(p)		vmm_cpu_to_le64(*((u64 *)(p)))
#define le64_to_cpup(p)		vmm_le64_to_cpu(*((u64 *)(p)))
#define cpup_to_be64(p)		vmm_cpu_to_be64(*((u64 *)(p)))
#define be64_to_cpup(p)		vmm_be64_to_cpu(*((u64 *)(p)))

#endif /* defined(_ASM_BYTEORDER_H) */
