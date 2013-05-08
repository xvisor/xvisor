#ifndef _ASM_IO_H_
#define _ASM_IO_H_

#include <vmm_host_io.h>

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

/** I/O access functions (Assumed to be Little Endian) */
#define inb				vmm_inb
#define inw				vmm_inw
#define inl				vmm_inl
#define outb				vmm_outb
#define outw				vmm_outw
#define outl				vmm_outl

#define inb_p				vmm_inb_p
#define inw_p				vmm_inw_p
#define inl_p				vmm_inl_p
#define outb_p				vmm_outb_p
#define outw_p				vmm_outw_p
#define outl_p				vmm_outl_p

#define insb				vmm_insb
#define insw				vmm_insw
#define insl				vmm_insl
#define outsb				vmm_outsb
#define outsw				vmm_outsw
#define outsl				vmm_outsl

/** Memory read/write legacy functions (Assumed to be Little Endian) */
#define readb				vmm_readb
#define writeb				vmm_writeb
#define readw				vmm_readw
#define writew				vmm_writew
#define readl				vmm_readl
#define writel				vmm_writel

#define	readsl				vmm_readsl
#define	readsw				vmm_readsw
#define readsb				vmm_readsb
#define	writesl				vmm_writesl
#define	writesw				vmm_writesw
#define writesb				vmm_writesb

/** Memory read/write functions */
#define in_8				vmm_in_8
#define out_8				vmm_out_8
#define in_le16				vmm_in_le16
#define out_le16			vmm_out_le16
#define in_be16				vmm_in_be16
#define out_be16			vmm_out_be16
#define in_le32				vmm_in_le32
#define out_le32			vmm_out_le32
#define in_be32				vmm_in_be32
#define out_be32			vmm_out_be32
#define in_le64				vmm_in_le64
#define out_le64			vmm_out_le64
#define in_be64				vmm_in_be64
#define out_be64			vmm_out_be64

#define	__iomem

#endif /* _ASM_IO_H_ */
