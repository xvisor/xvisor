/**
 * Copyright (C) 2014-2016 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file imx_serial.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Motorola/Freescale i.MX serial emulator.
 * @details This source file implements the i.MX serial emulator.
 *
 * The source has been largely adapted from PL011 emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vio/vmm_vserial.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"IMX Serial Emulator"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY+1)
#define	MODULE_INIT			imx_emulator_init
#define	MODULE_EXIT			imx_emulator_exit

/* Register definitions */
#define URXD0 0x0  /* Receiver Register */
#define URTX0 0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define IMX21_ONEMS 0xb0 /* One Millisecond register */

#define IMX_FIFO_SIZE			32

#define IMX1_UTS 0xd0 /* UART Test Register on i.mx1 */
#define IMX21_UTS 0xb4 /* UART Test Register on all other i.mx*/

/* UART Control Register Bit Fields.*/
#define URXD_DUMMY_READ (1<<16)
#define URXD_CHARRDY	(1<<15)
#define URXD_ERR	(1<<14)
#define URXD_OVRRUN	(1<<13)
#define URXD_FRMERR	(1<<12)
#define URXD_BRK	(1<<11)
#define URXD_PRERR	(1<<10)
#define URXD_RX_DATA	(0xFF<<0)
#define UCR1_ADEN	(1<<15) /* Auto detect interrupt */
#define UCR1_ADBR	(1<<14) /* Auto detect baud rate */
#define UCR1_TRDYEN	(1<<13) /* Transmitter ready interrupt enable */
#define UCR1_IDEN	(1<<12) /* Idle condition interrupt */
#define UCR1_ICD_REG(x) (((x) & 3) << 10) /* idle condition detect */
#define UCR1_RRDYEN	(1<<9)	/* Recv ready interrupt enable */
#define UCR1_RDMAEN	(1<<8)	/* Recv ready DMA enable */
#define UCR1_IREN	(1<<7)	/* Infrared interface enable */
#define UCR1_TXMPTYEN	(1<<6)	/* Transimitter empty interrupt enable */
#define UCR1_RTSDEN	(1<<5)	/* RTS delta interrupt enable */
#define UCR1_SNDBRK	(1<<4)	/* Send break */
#define UCR1_TDMAEN	(1<<3)	/* Transmitter ready DMA enable */
#define IMX1_UCR1_UARTCLKEN (1<<2) /* UART clock enabled, i.mx1 only */
#define UCR1_ATDMAEN    (1<<2)  /* Aging DMA Timer Enable */
#define UCR1_DOZE	(1<<1)	/* Doze */
#define UCR1_UARTEN	(1<<0)	/* UART enabled */
#define UCR2_ESCI	(1<<15)	/* Escape seq interrupt enable */
#define UCR2_IRTS	(1<<14)	/* Ignore RTS pin */
#define UCR2_CTSC	(1<<13)	/* CTS pin control */
#define UCR2_CTS	(1<<12)	/* Clear to send */
#define UCR2_ESCEN	(1<<11)	/* Escape enable */
#define UCR2_PREN	(1<<8)	/* Parity enable */
#define UCR2_PROE	(1<<7)	/* Parity odd/even */
#define UCR2_STPB	(1<<6)	/* Stop */
#define UCR2_WS		(1<<5)	/* Word size */
#define UCR2_RTSEN	(1<<4)	/* Request to send interrupt enable */
#define UCR2_ATEN	(1<<3)	/* Aging Timer Enable */
#define UCR2_TXEN	(1<<2)	/* Transmitter enabled */
#define UCR2_RXEN	(1<<1)	/* Receiver enabled */
#define UCR2_SRST	(1<<0)	/* SW reset */
#define UCR3_DTREN	(1<<13) /* DTR interrupt enable */
#define UCR3_PARERREN	(1<<12) /* Parity enable */
#define UCR3_FRAERREN	(1<<11) /* Frame error interrupt enable */
#define UCR3_DSR	(1<<10) /* Data set ready */
#define UCR3_DCD	(1<<9)	/* Data carrier detect */
#define UCR3_RI		(1<<8)	/* Ring indicator */
#define UCR3_ADNIMP	(1<<7)	/* Autobaud Detection Not Improved */
#define UCR3_RXDSEN	(1<<6)	/* Receive status interrupt enable */
#define UCR3_AIRINTEN	(1<<5)	/* Async IR wake interrupt enable */
#define UCR3_AWAKEN	(1<<4)	/* Async wake interrupt enable */
#define IMX21_UCR3_RXDMUXSEL	(1<<2)	/* RXD Muxed Input Select */
#define UCR3_INVT	(1<<1)	/* Inverted Infrared transmission */
#define UCR3_BPEN	(1<<0)	/* Preset registers enable */
#define UCR4_CTSTL_SHF	10	/* CTS trigger level shift */
#define UCR4_CTSTL_MASK	0x3F	/* CTS trigger is 6 bits wide */
#define UCR4_INVR	(1<<9)	/* Inverted infrared reception */
#define UCR4_ENIRI	(1<<8)	/* Serial infrared interrupt enable */
#define UCR4_WKEN	(1<<7)	/* Wake interrupt enable */
#define UCR4_REF16	(1<<6)	/* Ref freq 16 MHz */
#define UCR4_IDDMAEN    (1<<6)  /* DMA IDLE Condition Detected */
#define UCR4_IRSC	(1<<5)	/* IR special case */
#define UCR4_TCEN	(1<<3)	/* Transmit complete interrupt enable */
#define UCR4_BKEN	(1<<2)	/* Break condition interrupt enable */
#define UCR4_OREN	(1<<1)	/* Receiver overrun interrupt enable */
#define UCR4_DREN	(1<<0)	/* Recv data ready interrupt enable */
#define UFCR_RXTL_SHF	0	/* Receiver trigger level shift */
#define UFCR_DCEDTE	(1<<6)	/* DCE/DTE mode select */
#define UFCR_RFDIV	(7<<7)	/* Reference freq divider mask */
#define UFCR_RFDIV_REG(x)	(((x) < 7 ? 6 - (x) : 6) << 7)
#define UFCR_TXTL_SHF	10	/* Transmitter trigger level shift */
#define USR1_PARITYERR	(1<<15) /* Parity error interrupt flag */
#define USR1_RTSS	(1<<14) /* RTS pin status */
#define USR1_TRDY	(1<<13) /* Transmitter ready interrupt/dma flag */
#define USR1_RTSD	(1<<12) /* RTS delta */
#define USR1_ESCF	(1<<11) /* Escape seq interrupt flag */
#define USR1_FRAMERR	(1<<10) /* Frame error interrupt flag */
#define USR1_RRDY	(1<<9)	 /* Receiver ready interrupt/dma flag */
#define USR1_AGTIM	(1<<8)   /* Ageing timer interrfupt flag */
#define USR1_TIMEOUT	(1<<7)	 /* Receive timeout interrupt status */
#define USR1_RXDS	 (1<<6)	 /* Receiver idle interrupt flag */
#define USR1_AIRINT	 (1<<5)	 /* Async IR wake interrupt flag */
#define USR1_AWAKE	 (1<<4)	 /* Aysnc wake interrupt flag */
#define USR2_ADET	 (1<<15) /* Auto baud rate detect complete */
#define USR2_TXFE	 (1<<14) /* Transmit buffer FIFO empty */
#define USR2_DTRF	 (1<<13) /* DTR edge interrupt flag */
#define USR2_IDLE	 (1<<12) /* Idle condition */
#define USR2_IRINT	 (1<<8)	 /* Serial infrared interrupt flag */
#define USR2_WAKE	 (1<<7)	 /* Wake */
#define USR2_RTSF	 (1<<4)	 /* RTS edge interrupt flag */
#define USR2_TXDC	 (1<<3)	 /* Transmitter complete */
#define USR2_BRCD	 (1<<2)	 /* Break condition */
#define USR2_ORE	(1<<1)	 /* Overrun error */
#define USR2_RDR	(1<<0)	 /* Recv data ready */
#define UTS_FRCPERR	(1<<13) /* Force parity error */
#define UTS_LOOP	(1<<12)	 /* Loop tx and rx */
#define UTS_TXEMPTY	 (1<<6)	 /* TxFIFO empty */
#define UTS_RXEMPTY	 (1<<5)	 /* RxFIFO empty */
#define UTS_TXFULL	 (1<<4)	 /* TxFIFO full */
#define UTS_RXFULL	 (1<<3)	 /* RxFIFO full */
#define UTS_SOFTRST	 (1<<0)	 /* Software reset */

#define USR1_WR_MASK	(USR1_PARITYERR | USR1_RTSD | USR1_ESCF |   \
			 USR1_FRAMERR | USR1_AGTIM | USR1_TIMEOUT | \
			 USR1_AIRINT | USR1_AWAKE)
#define USR2_WR_MASK	(USR2_ADET | USR2_DTRF | USR2_IDLE | (1 << 11) | \
			 (1 << 10) | USR2_IRINT | USR2_WAKE | (1 << 6) | \
			 USR2_RTSF | USR2_BRCD | USR2_ORE)
#define UTS_WR_MASK	(UTS_FRCPERR | UTS_LOOP | (7 << 9) | UTS_TXEMPTY \
			 | UTS_RXEMPTY | UTS_TXFULL | UTS_RXFULL | UTS_SOFTRST)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))



struct imx_state {
	struct vmm_guest *guest;
	struct vmm_vserial *vser;
	vmm_spinlock_t lock;
	const struct imx_uart_data *data;
	u32 txirq;
	u32 rdirq;
	struct fifo *rd_fifo;
	u16 regs[(IMX21_ONEMS - UCR1) / 4 + 1];
	u16 uts;
	u8 tx;
};

/* i.MX21 type uart runs on all i.mx except i.MX1 and i.MX6q */
enum imx_uart_type {
	IMX1_UART,
	IMX21_UART,
	IMX6Q_UART,
};

/* device type dependent stuff */
struct imx_uart_data {
	unsigned uts_reg;
	enum imx_uart_type devtype;
};

static inline unsigned int _reg_offset_get(u32 reg)
{
        return (reg - UCR1) / 4;
}

static u16 _reg_read(const struct imx_state *s, u32 reg)
{
	const unsigned int offset = _reg_offset_get(reg);

	BUG_ON(offset >= ARRAY_SIZE(s->regs));
	return s->regs[offset];
}

static void _reg_write(struct imx_state *s, u32 reg, u16 val)
{
	const unsigned int offset = _reg_offset_get(reg);

	BUG_ON(offset >= ARRAY_SIZE(s->regs));
	s->regs[offset] = val;
}

static bool _txfe_irq_enabled(const struct imx_state *s)
{
	/* NOTE: we should also check for USR2_TXFE to be 1.
	 * If we want to emulate the hardware queues (which are not
	 * handled at the moment), we should add the following test:
	 *	_reg_read(s, USR2) & USR2_TXFE
	 */
	return (_reg_read(s, UCR1) & UCR1_TXMPTYEN);
}

static void _reg_set_mask(struct imx_state *s, u32 reg, u16 mask)
{
	const unsigned int offset = _reg_offset_get(reg);

	BUG_ON(offset >= ARRAY_SIZE(s->regs));
	s->regs[offset] |= mask;
}

static void _reg_clear_mask(struct imx_state *s, u32 reg, u16 mask)
{
	const unsigned int offset = _reg_offset_get(reg);

	BUG_ON(offset >= ARRAY_SIZE(s->regs));
	s->regs[offset] &= ~mask;
}

static void _reg_ack(struct imx_state *s, u32 reg, u16 mask)
{
	u16 tmp = 0;
	const unsigned int offset = _reg_offset_get(reg);

	BUG_ON(offset >= ARRAY_SIZE(s->regs));
	tmp = s->regs[offset] & mask;
	s->regs[offset] &= ~tmp;
}

static void imx_set_rdirq(struct imx_state *s, int level)
{
	vmm_devemu_emulate_irq(s->guest, s->rdirq, level);
}

static int _imx_reg_urxd0(struct imx_state *s, u32 *dst, int nb)
{
	int i = 0;
	u32 read_count = 0x0;
	u8 val;

	if (!(_reg_read(s, UCR1) | UCR1_UARTEN) ||
	    !(_reg_read(s, UCR2) & UCR2_RXEN)) {
		*dst = 0;
		return 0;
	}

	read_count = fifo_avail(s->rd_fifo);
	if (nb > read_count) {
		nb = read_count;
	}
	for (i = 0; i < nb; ++i) {
		fifo_dequeue(s->rd_fifo, &val);
		*dst = *dst << 8 | val;
	}
	read_count -= nb;

	if (read_count == 0) {
		_reg_clear_mask(s, USR2, USR2_RDR);
		s->uts &= ~UTS_RXEMPTY;
	}
	if (read_count < (_reg_read(s, UFCR) & 0x3f)) {
		_reg_clear_mask(s, USR1, USR1_RRDY);
	}
	s->uts &= ~UTS_RXFULL;

	/* Receiver ready fifo or DMA interrupt set and enabled */
	if ((_reg_read(s, USR1) & USR1_RRDY) &&
	    (_reg_read(s, UCR1) & (UCR1_RRDYEN | UCR1_RDMAEN))) {
		return 1;
	}

	return 0;
}

static int imx_reg_read(struct imx_state *s, u32 offset, u32 *dst, int nb)
{
	int rc = VMM_OK;
	const unsigned int reg = _reg_offset_get(offset);
	int level = 0;
	bool set_irq = FALSE;

	vmm_spin_lock(&s->lock);

	if (URXD0 == offset) {
		level = _imx_reg_urxd0(s, dst, nb);
		set_irq = TRUE;
	} else if (URTX0 == offset) {
		*dst = s->tx;
	} else if (reg < ARRAY_SIZE(s->regs)) {
		*dst = s->regs[reg];
	} else if (s->data->uts_reg == offset) {
		*dst = s->uts;
	} else {
		vmm_printf("i.MX UART unmanaged read at 0x%x\n", offset);
	}

	vmm_spin_unlock(&s->lock);

	if (set_irq) {
		imx_set_rdirq(s, level);
	}

	return rc;
}

static void imx_set_txirq(struct imx_state *s, int level)
{
	u32 irq = s->txirq;

	if (!irq) {
		irq = s->rdirq;
	}
	vmm_devemu_emulate_irq(s->guest, irq, level);
}

static int imx_reg_write(struct imx_state *s, u32 offset,
			   u32 src_mask, u32 src)
{
	const unsigned int reg = _reg_offset_get(offset);
	u16 ack = 0;
	u16 usr1 = 0;
	u16 usr2 = 0;
	bool recv_char = FALSE;
	u16 val = (u16)(src & ~src_mask);

	vmm_spin_lock(&s->lock);

	usr1 = _reg_read(s, USR1);
	usr2 = _reg_read(s, USR2);

	if (URXD0 == offset) {
		/* Do nothing */
	} else if (URTX0 == offset) {
		s->tx = (u8)src;
		recv_char = TRUE;
	} else if (USR1 == offset) {
		_reg_ack(s, USR1, val & USR1_WR_MASK);
		ack = 1;
	} else if (USR2 == offset) {
		_reg_ack(s, USR2, val & USR2_WR_MASK);
		ack = 1;
	} else {
		if (reg < ARRAY_SIZE(s->regs)) {
			s->regs[reg] = val;

			if (UCR2 == offset) {
				_reg_clear_mask(s, UCR2, UCR2_SRST);
			}
		} else if (s->data->uts_reg == offset) {
			s->uts = (s->uts & src_mask) | (val & UTS_WR_MASK);
		} else {
			vmm_printf("i.MX UART unmanaged read at 0x%x\n",
				   offset);
		}
	}

	if (ack && ((usr1 != _reg_read(s, USR1)) ||
		    (usr2 != _reg_read(s, USR2)))) {
		imx_set_txirq(s, 0);
	}

	vmm_spin_unlock(&s->lock);

	if (!(_reg_read(s, UCR1) | UCR1_UARTEN) ||
	    !(_reg_read(s, UCR2) | UCR2_TXEN)) {
		return VMM_ENOTAVAIL;
	}

	if (recv_char) {
		u8 *val = (u8 *)&src;
		u32 len = 0;

		if (0xFFFFFF00 == src_mask) {
			len = 1;
		} else if (0xFFFF0000 == src_mask) {
			len = 2;
		} else {
			len = 4;
		}

		/*
		 * TODO: Create a thread for transfering characters which would
		 * disable correctly set USR1_TRDY, USR2_TXDC, USR2_TXDC, and
		 * UTS_TX[FULL|EMPTY].
		 */
		vmm_vserial_receive(s->vser, val, len);
	}

	/* Is TX ready interrupt enabled? */
	if ((_reg_read(s, USR1) & USR1_TRDY) &&
	    (_reg_read(s, UCR1) & UCR1_TRDYEN)) {
		/*
		 * As we do not manage the vserial overflow, we are always have
		 * the TX ready interrupt.
		 */
		imx_set_txirq(s, 1);
	} else if (_txfe_irq_enabled(s)) {
		imx_set_txirq(s, 1);
	} else if (_reg_read(s, UCR1) & UCR1_RTSDEN) {
		imx_set_txirq(s, 0);
	}

	return VMM_OK;
}

static bool imx_vserial_can_send(struct vmm_vserial *vser)
{
	struct imx_state *s = vmm_vserial_priv(vser);
	return !fifo_isfull(s->rd_fifo);
}

static int imx_vserial_send(struct vmm_vserial *vser, u8 data)
{
	bool set_irq = FALSE;
	u32 rd_count;
	struct imx_state *s = vmm_vserial_priv(vser);

	if (!(_reg_read(s, UCR1) & UCR1_UARTEN) ||
	    !(_reg_read(s, UCR2) & UCR2_RXEN)) {
		return VMM_ENOTAVAIL;
	}

	vmm_spin_lock(&s->lock);

	if (fifo_isfull(s->rd_fifo)) {
		vmm_spin_unlock(&s->lock);
		return VMM_ENOTAVAIL;
	}

	fifo_enqueue(s->rd_fifo, &data, TRUE);
	rd_count = fifo_avail(s->rd_fifo);

	s->uts &= ~UTS_RXEMPTY;
	if (IMX_FIFO_SIZE == rd_count) {
		s->uts |= UTS_RXFULL;
	}

	_reg_set_mask(s, USR2, USR2_RDR);
	if (rd_count >= (_reg_read(s, UFCR) & 0x003f)) {
		_reg_set_mask(s, USR1, USR1_RRDY);
		if (_reg_read(s, UCR1) & UCR1_RRDYEN) {
			set_irq = TRUE;
		}
	}

	vmm_spin_unlock(&s->lock);

	if (set_irq) {
		imx_set_rdirq(s, 1);
	}

	return VMM_OK;
}

static int imx_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = imx_reg_read(edev->priv, offset, &regval, 1);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int imx_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = imx_reg_read(edev->priv, offset, &regval, 2);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int imx_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	return imx_reg_read(edev->priv, offset, dst, 4);
}

static int imx_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	return imx_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int imx_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	return imx_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int imx_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	return imx_reg_write(edev->priv, offset, 0x00000000, src);
}

static int imx_emulator_reset(struct vmm_emudev *edev)
{
	struct imx_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	_reg_write(s, UCR1, 0x00002000);
	_reg_write(s, UCR2, 0x00000001);
	_reg_write(s, UCR3, 0x00000700);
	_reg_write(s, UCR4, 0x00008000);
	_reg_write(s, UFCR, 0x00000801);
	_reg_write(s, USR1, 0x00002040);
	_reg_write(s, USR2, 0x00004028);
	_reg_write(s, UESC, 0x0000002B);
	_reg_write(s, UBRC, 0x00000004);
	s->uts = 0x00000060;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int imx_emulator_probe(struct vmm_guest *guest,
			      struct vmm_emudev *edev,
			      const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	struct imx_state *s;

	s = vmm_zalloc(sizeof(struct imx_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto imx_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	s->data = eid->data;

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->rdirq, 0);
	if (rc) {
		goto imx_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->txirq, 1);
	if (rc) {
		s->txirq = 0;
	}

	s->rd_fifo = fifo_alloc(1, IMX_FIFO_SIZE);
	if (!s->rd_fifo) {
		rc = VMM_EFAIL;
		goto imx_emulator_probe_freestate_fail;
	}

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto imx_emulator_probe_freerbuf_fail;
	}

	s->vser = vmm_vserial_create(name,
				     &imx_vserial_can_send,
				     &imx_vserial_send,
				     IMX_FIFO_SIZE, s);
	if (!(s->vser)) {
		goto imx_emulator_probe_freerbuf_fail;
	}

	edev->priv = s;
	imx_emulator_reset(edev);

	goto imx_emulator_probe_done;

imx_emulator_probe_freerbuf_fail:
	fifo_free(s->rd_fifo);
imx_emulator_probe_freestate_fail:
	vmm_free(s);
imx_emulator_probe_done:
	return rc;
}

static int imx_emulator_remove(struct vmm_emudev *edev)
{
	struct imx_state *s = edev->priv;

	if (s) {
		vmm_vserial_destroy(s->vser);
		fifo_free(s->rd_fifo);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct imx_uart_data imx_uart_devdata[] = {
	[IMX1_UART] = {
		.uts_reg = IMX1_UTS,
		.devtype = IMX1_UART,
	},
	[IMX21_UART] = {
		.uts_reg = IMX21_UTS,
		.devtype = IMX21_UART,
	},
	[IMX6Q_UART] = {
		.uts_reg = IMX21_UTS,
		.devtype = IMX6Q_UART,
	},
};

static struct vmm_devtree_nodeid imx_emuid_table[] = {
	{
		.type = "serial",
		.compatible = "fsl,imx1-uart",
		.data = &imx_uart_devdata[IMX1_UART]
	},
	{
		.type = "serial",
		.compatible = "fsl,imx21-uart",
		.data = &imx_uart_devdata[IMX21_UART],
	},
	{
		.type = "serial",
		.compatible = "fsl,imx6q-uart",
		.data = &imx_uart_devdata[IMX6Q_UART],
	},
	{ /* end of list */ },
};

static struct vmm_emulator imx_emulator = {
	.name = "imx",
	.match_table = imx_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = imx_emulator_probe,
	.read8 = imx_emulator_read8,
	.write8 = imx_emulator_write8,
	.read16 = imx_emulator_read16,
	.write16 = imx_emulator_write16,
	.read32 = imx_emulator_read32,
	.write32 = imx_emulator_write32,
	.reset = imx_emulator_reset,
	.remove = imx_emulator_remove,
};

static int __init imx_emulator_init(void)
{
	return vmm_devemu_register_emulator(&imx_emulator);
}

static void __exit imx_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
