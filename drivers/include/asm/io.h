#ifndef _ASM_IO_H_
#define _ASM_IO_H_

#include <vmm_host_aspace.h>
#include <vmm_host_io.h>

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
#define readl_relaxed			vmm_readl
#define writel_relaxed			vmm_writel

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

#define	__raw_readw			readw
#define	__raw_readl			readl
#define	__raw_writew			writew
#define	__raw_writel			writel

#define ioread8(addr)			readb(addr)
#define ioread16(addr)			readw(addr)
#define ioread16be(addr)		__be16_to_cpu(__raw_readw(addr))
#define ioread32(addr)			readl(addr)
#define ioread32be(addr)		__be32_to_cpu(__raw_readl(addr))

#define iowrite8(v, addr)		writeb((v), (addr))
#define iowrite16(v, addr)		writew((v), (addr))
#define iowrite16be(v, addr)		__raw_writew(__cpu_to_be16(v), addr)
#define iowrite32(v, addr)		writel((v), (addr))
#define iowrite32be(v, addr)		__raw_writel(__cpu_to_be32(v), addr)

#define ioread8_rep(p, dst, count) \
				insb((unsigned long) (p), (dst), (count))
#define ioread16_rep(p, dst, count) \
				insw((unsigned long) (p), (dst), (count))
#define ioread32_rep(p, dst, count) \
				insl((unsigned long) (p), (dst), (count))

#define iowrite8_rep(p, src, count) \
				outsb((unsigned long) (p), (src), (count))
#define iowrite16_rep(p, src, count) \
				outsw((unsigned long) (p), (src), (count))
#define iowrite32_rep(p, src, count) \
				outsl((unsigned long) (p), (src), (count))

static inline void iounmap(void __iomem *addr)
{
	vmm_host_iounmap((virtual_addr_t) addr);
}

#endif /* _ASM_IO_H_ */
