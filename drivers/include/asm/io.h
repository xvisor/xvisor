#ifndef _ASM_IO_H_
#define _ASM_IO_H_

#include <vmm_host_io.h>

/** I/O read/write legacy functions (Assumed to be Little Endian) */
#define ioreadb				vmm_ioreadb
#define iowriteb			vmm_iowriteb
#define ioreadw				vmm_ioreadw
#define iowritew			vmm_iowritew
#define ioreadl				vmm_ioreadl
#define iowritel			vmm_iowritel

/** Memory read/write legacy functions (Assumed to be Little Endian) */
#define readb				vmm_readb
#define writeb				vmm_writeb
#define readw				vmm_readw
#define writew				vmm_writew
#define readl				vmm_readl
#define writel				vmm_writel
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

#endif /* _ASM_IO_H_ */
