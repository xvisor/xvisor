/*
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file i8259.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief i8259 Interrupt Controller Emulator
 * @details This source file implements the i8259 PIC emulator.
 *
 * The source has been largely adapted from QEMU hw/intc/i8259.c

 * QEMU 8259 interrupt controller emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 */
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <vmm_vcpu_irq.h>

#include <arch_cpu_irq.h>
#include <emu/i8259.h>
#include <emu/apic_common.h>

#define MODULE_DESC			"i8259 PIC Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			i8259_emulator_init
#define	MODULE_EXIT			i8259_emulator_exit

/* debug PIC */
enum i8259_debug_log_levels {
	I8259_LOG_LVL_ERR,
	I8259_LOG_LVL_INFO,
	I8259_LOG_LVL_DEBUG,
	I8259_LOG_LVL_VERBOSE
};

static unsigned int default_log_lvl = I8259_LOG_LVL_INFO;

#define I8259_LOG(lvl, fmt, args...)				\
	do {							\
		if (I8259_LOG_LVL_##lvl <= default_log_lvl)	\
			vmm_printf("i8259: " fmt , ##args);	\
	} while (0);

#define DEBUG_IRQ_LATENCY	0
#define DEBUG_IRQ_COUNT		0

#if DEBUG_PIC || DEBUG_IRQ_COUNT
static int irq_level[16];
#endif
#if DEBUG_IRQ_COUNT
static u64 irq_count[16];
#endif
#if DEBUG_IRQ_LATENCY
static s64 irq_time[16];
#endif

#define PIC_ASSERT_INT		1
#define PIC_DEASSERT_INT	0

struct guest_pic_list {
	vmm_spinlock_t lock;
	struct dlist pic_list;
};

extern void arch_guest_halt(struct vmm_guest *);

/* return the highest priority found in mask (highest = smallest
   number). Return 8 if no irq */
static int get_priority(i8259_state_t *s, int mask)
{
	int priority;

	if (mask == 0) {
		return 8;
	}
	priority = 0;
	while ((mask & (1 << ((priority + s->priority_add) & 7))) == 0) {
		priority++;
	}
	return priority;
}

/* return the pic wanted interrupt. return -1 if none */
static int pic_get_irq(i8259_state_t *s)
{
	int mask, cur_priority, priority;

	mask = s->irr & ~s->imr;
	priority = get_priority(s, mask);
	if (priority == 8) {
		return -1;
	}
	/* compute current priority. If special fully nested mode on the
	   master, the IRQ coming from the slave is not taken into account
	   for the priority computation. */
	mask = s->isr;
	if (s->special_mask) {
		mask &= ~s->imr;
	}
	if (s->special_fully_nested_mode && s->master) {
		mask &= ~(1 << 2);
	}
	cur_priority = get_priority(s, mask);
	if (priority < cur_priority) {
		/* higher priority found: an irq should be generated */
		return (priority + s->priority_add) & 7;
	} else {
		return -1;
	}
}

/* Update INT output. Must be called every time the output may have changed. */
static void pic_update_irq(i8259_state_t *s)
{
	int irq;
	int action;
	struct vmm_vcpu *vcpu = vmm_manager_guest_vcpu(s->guest, 0);
	unsigned int level;

	if (!vcpu)
		return;

	irq = pic_get_irq(s);

	if (!s->master) {
		level = SLAVE_IRQ_ENCODE(0, 0, 0, irq, 0);
		vmm_devemu_emulate_irq(s->guest, s->parent_irq, level);
	} else {
		action = (irq >= 0 ? PIC_ASSERT_INT : PIC_DEASSERT_INT);

		/* change level on CPU interrupt line */
		if (action == PIC_DEASSERT_INT)
			vmm_vcpu_irq_deassert(vcpu, irq);
		else
			vmm_vcpu_irq_assert(vcpu, irq, 0);
	}
	I8259_LOG(VERBOSE, "pic%d: imr=%x irr=%x padd=%d int=0x%x action=%d\n",
		  s->master ? 0 : 1, s->imr, s->irr, s->priority_add, irq, action);
}

/* set irq level. If an edge is detected, then the IRR is set to 1 */
static void pic_set_irq(i8259_state_t *s, int irq, int level)
{
	int mask = 1 << irq;

#if DEBUG_PIC || DEBUG_IRQ_COUNT || DEBUG_IRQ_LATENCY
	int irq_index = s->master ? irq : irq + 8;
#endif
#if DEBUG_PIC || DEBUG_IRQ_COUNT
	if (level != irq_level[irq_index]) {
		I8259_LOG(DEBUG, "pic_set_irq: irq=%d level=%d\n", irq_index, level);
		irq_level[irq_index] = level;
#if DEBUG_IRQ_COUNT
		if (level == 1) {
			irq_count[irq_index]++;
		}
#endif
	}
#endif
#if DEBUG_IRQ_LATENCY
	if (level) {
		irq_time[irq_index] = vmm_timer_timestamp();
	}
#endif

	if (s->elcr & mask) {
		/* level triggered */
		if (level) {
			s->irr |= mask;
			s->last_irr |= mask;
		} else {
			s->irr &= ~mask;
			s->last_irr &= ~mask;
		}
	} else {
		/* edge triggered */
		if (level) {
			if ((s->last_irr & mask) == 0) {
				s->irr |= mask;
			}
			s->last_irr |= mask;
		} else {
			s->last_irr &= ~mask;
		}
	}
	pic_update_irq(s);
}

/* acknowledge interrupt 'irq' */
static void pic_intack(i8259_state_t *s, int irq)
{
	if (s->auto_eoi) {
		if (s->rotate_on_auto_eoi) {
			s->priority_add = (irq + 1) & 7;
		}
	} else {
		s->isr |= (1 << irq);
	}
	/* We don't clear a level sensitive interrupt here */
	if (!(s->elcr & (1 << irq))) {
		s->irr &= ~(1 << irq);
	}
	pic_update_irq(s);
}

i8259_state_t *get_slave_pic(struct i8259_state *s, u32 parent_irq)
{
	struct i8259_state *spic = NULL;
	struct guest_pic_list *plist = (struct guest_pic_list *)arch_get_guest_pic_list(s->guest);
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&plist->lock, flags);
	list_for_each_entry(spic, &plist->pic_list, head) {
		if (spic->parent_irq == parent_irq)
			break;
	}
	vmm_spin_unlock_irqrestore(&plist->lock, flags);

	return spic;
}

int pic_read_irq(i8259_state_t *s)
{
	int irq, irq2, intno;
	i8259_state_t *slave_pic;

	irq = pic_get_irq(s);
	if (irq >= 0) {
		if (irq == 2) {
			slave_pic = get_slave_pic(s, 2);

			if (!slave_pic) {
				I8259_LOG(ERR, "FATAL: Interrupt %d from slave PIC but no slave PIC registered on interrupt!\n", irq);
				arch_guest_halt(s->guest);
			}
	
			irq2 = pic_get_irq(slave_pic);
			if (irq2 >= 0) {
				pic_intack(slave_pic, irq2);
			} else {
				/* spurious IRQ on slave controller */
				irq2 = 7;
			}
			intno = slave_pic->int_base + irq2;
		} else {
			intno = s->int_base + irq;
		}
		pic_intack(s, irq);
	} else {
		/* spurious IRQ on host controller */
		irq = 7;
		intno = s->int_base + irq;
	}

#if DEBUG_PIC || DEBUG_IRQ_LATENCY
	if (irq == 2) {
		irq = irq2 + 8;
	}
#endif
#if DEBUG_IRQ_LATENCY
	vmm_printf("IRQ%d latency=%0.3fus\n",
		   irq,
		   (double)(vmm_timer_timestamp() -
			    irq_time[irq]) * 1000000.0 / 1000000000ul);
#endif
	I8259_LOG(DEBUG, "pic_interrupt: irq=%d\n", irq);
	return intno;
}

static void pic_init_reset(struct i8259_state *s)
{
	s->elcr = 0;
	s->last_irr = 0;
	s->irr &= s->elcr;
	s->imr = 0;
	s->isr = 0;
	s->priority_add = 0;
	s->read_reg_select = 0;
	s->poll = 0;
	s->special_mask = 0;
	s->init_state = 0;
	s->auto_eoi = 0;
	s->rotate_on_auto_eoi = 0;
	s->special_fully_nested_mode = 0;
	s->init4 = 0;
	s->single_mode = 0;
}

static int i8259_emulator_reset(struct vmm_emudev *edev)
{
	i8259_state_t *s = edev->priv;

	pic_init_reset(s);
	pic_update_irq(s);

	return VMM_OK;
}

static int pic_ioport_write(i8259_state_t *s, u32 addr,
			    u32 src_mask, u32 val)
{
	int priority, cmd, irq;

	I8259_LOG(DEBUG, "write: addr=0x%02x val=0x%02x\n", addr, val);
	if (addr == 0) {
		if (val & 0x10) {
			pic_init_reset(s);
			s->init_state = 1;
			s->init4 = val & 1;
			s->single_mode = val & 2;
			if (val & 0x08) {
				I8259_LOG(ERR, "level sensitive irq not supported");
				arch_guest_halt(s->guest);
			}
		} else if (val & 0x08) {
			if (val & 0x04) {
				s->poll = 1;
			}
			if (val & 0x02) {
				s->read_reg_select = val & 1;
			}
			if (val & 0x40) {
				s->special_mask = (val >> 5) & 1;
			}
		} else {
			cmd = val >> 5;
			switch (cmd) {
			case 0:
			case 4:
				s->rotate_on_auto_eoi = cmd >> 2;
				break;
			case 1: /* end of interrupt */
			case 5:
				priority = get_priority(s, s->isr);
				if (priority != 8) {
					irq = (priority + s->priority_add) & 7;
					s->isr &= ~(1 << irq);
					if (cmd == 5) {
						s->priority_add = (irq + 1) & 7;
					}
					pic_update_irq(s);
				}
				break;
			case 3:
				irq = val & 7;
				s->isr &= ~(1 << irq);
				pic_update_irq(s);
				break;
			case 6:
				s->priority_add = (val + 1) & 7;
				pic_update_irq(s);
				break;
			case 7:
				irq = val & 7;
				s->isr &= ~(1 << irq);
				s->priority_add = (irq + 1) & 7;
				pic_update_irq(s);
				break;
			default:
				/* no operation */
				break;
			}
		}
	} else {
		switch (s->init_state) {
		case 0:
			/* normal mode */
			s->imr = val;
			pic_update_irq(s);
			break;
		case 1:
			s->int_base = val & 0xf8;
			s->init_state = s->single_mode ? (s->init4 ? 3 : 0) : 2;
			break;
		case 2:
			if (s->init4) {
				s->init_state = 3;
			} else {
				s->init_state = 0;
			}
			break;
		case 3:
			s->special_fully_nested_mode = (val >> 4) & 1;
			s->auto_eoi = (val >> 1) & 1;
			s->init_state = 0;
			break;
		}
	}

	return VMM_OK;
}

static u64 pic_ioport_read(i8259_state_t *s, physical_addr_t addr, u32* dst)
{
	u32 ret;

	if (s->poll) {
		ret = pic_get_irq(s);
		if (ret >= 0) {
			pic_intack(s, ret);
			ret |= 0x80;
		} else {
			ret = 0;
		}
		s->poll = 0;
	} else {
		if (addr == 0) {
			if (s->read_reg_select) {
				ret = s->isr;
			} else {
				ret = s->irr;
			}
		} else {
			ret = s->imr;
		}
	}

	I8259_LOG(DEBUG, "read: addr=0x%02x val=0x%02x\n", addr, ret);

	*dst = ret;

	return VMM_OK;
}

static int i8259_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pic_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int i8259_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pic_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int i8259_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 *dst)
{
	return pic_ioport_read(edev->priv, offset, dst);
}

static int i8259_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u8 src)
{
	return pic_ioport_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int i8259_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u16 src)
{
	return pic_ioport_write(edev->priv, offset, 0xFFFF0000, src);
}

static int i8259_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u32 src)
{
	return pic_ioport_write(edev->priv, offset, 0x00000000, src);
}

#if 0
static void elcr_ioport_write(i8259_state_t *s, physical_addr_t addr,
                              u64 val, u32 size)
{
	s->elcr = val & s->elcr_mask;
}

static u64 elcr_ioport_read(i8259_state_t *s, physical_addr_t addr,
			    u32 size)
{
	return s->elcr;
}
#endif

/* Process IRQ asserted via device emulation framework */
void i8259_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	irq_flags_t flags;
	i8259_state_t *s = opaque;

	vmm_spin_lock_irqsave(&s->lock, flags);

	pic_set_irq(s, irq, level);

	vmm_spin_unlock_irqrestore(&s->lock, flags);
}

static int i8259_emulator_remove(struct vmm_emudev *edev)
{
	u32 i;
	i8259_state_t *s = edev->priv;

	if (s) {
		for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
			vmm_devemu_unregister_irq_handler(s->guest, i,
							  i8259_irq_handle, s);
		}
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static int i8259_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	i8259_state_t *s;
	struct guest_pic_list *plist = NULL;
	int i;
	irq_flags_t flags;

	plist = arch_get_guest_pic_list(guest);
	if (!plist) {
		plist = vmm_zalloc(sizeof(struct guest_pic_list));
		if (!plist) {
			return VMM_ENOMEM;
		}

		INIT_SPIN_LOCK(&plist->lock);
		INIT_LIST_HEAD(&plist->pic_list);
	}

	s = vmm_zalloc(sizeof(i8259_state_t));
	if (!s) {
		rc = VMM_ENOMEM;
		goto i8259_emulator_probe_done;
	}

	if (vmm_devtree_getattr(edev->node, "child_pic")) {
		s->master = FALSE;
		/* if child get parent irq */
		rc = vmm_devtree_read_u32(edev->node, "parent_irq", &s->parent_irq);
		if (rc) {
			goto i8259_emulator_probe_freestate_fail;
		}
	} else {
		s->master = TRUE;
	}

	if (vmm_devtree_read_u32(edev->node, "base_irq", &s->base_irq)) {
		I8259_LOG(ERR, "Base IRQ not defined!\n");
		goto i8259_emulator_probe_freestate_fail;
	}

	if (vmm_devtree_read_u32(edev->node, "num_irq", &s->num_irq)) {
		I8259_LOG(ERR, "Number of IRQ not defined!\n");
		goto i8259_emulator_probe_freestate_fail;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	INIT_LIST_HEAD(&s->head);

	edev->priv = s;

	vmm_spin_lock_irqsave(&plist->lock, flags);
	list_add_tail(&s->head, &plist->pic_list);
	vmm_spin_unlock_irqrestore(&plist->lock, flags);

	arch_set_guest_pic_list(guest, (void *)plist);

	if (s->master)
		arch_set_guest_master_pic(guest, s);

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_register_irq_handler(guest, i, 
						edev->node->name, 
						i8259_irq_handle, s);
	}

	goto i8259_emulator_probe_done;

 i8259_emulator_probe_freestate_fail:
	vmm_free(s);

 i8259_emulator_probe_done:
	return rc;
}

static struct vmm_devtree_nodeid i8259_emulator_emuid_table[] = {
	{
		.type = "pic",
		.compatible = "i8259a",
	},
	{ /* end of list */ },
};

static struct vmm_emulator i8259_emulator = {
	.name =        "i8259a",
	.match_table = i8259_emulator_emuid_table,
	.endian =      VMM_DEVEMU_LITTLE_ENDIAN,
	.probe =       i8259_emulator_probe,
	.read8 =       i8259_emulator_read8,
	.write8 =      i8259_emulator_write8,
	.read16 =      i8259_emulator_read16,
	.write16 =     i8259_emulator_write16,
	.read32 =      i8259_emulator_read32,
	.write32 =     i8259_emulator_write32,
	.reset =       i8259_emulator_reset,
	.remove =      i8259_emulator_remove,
};

static int __init i8259_emulator_init(void)
{
	return vmm_devemu_register_emulator(&i8259_emulator);
}

static void __exit i8259_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&i8259_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
