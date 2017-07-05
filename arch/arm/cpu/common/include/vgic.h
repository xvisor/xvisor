/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vgic.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hardware assisted GICv2 emulator header
 */

#ifndef __VGIC_H__
#define __VGIC_H__

#include <vmm_types.h>

#define VGIC_V2_MAX_LRS		(1 << 6)
#define VGIC_V3_MAX_LRS		16

#define VGIC_MAX_LRS		VGIC_V2_MAX_LRS

#define VGIC_V3_MAX_CPUS	255
#define VGIC_V2_MAX_CPUS	8

#define VGIC_MAX_IRQS		1024

enum vgic_type {
	VGIC_V2,		/* Good old GICv2 */
	VGIC_V3,		/* New fancy GICv3 */
};

#define VGIC_LR_STATE_PENDING	(1 << 0)
#define VGIC_LR_STATE_ACTIVE	(1 << 1)
#define VGIC_LR_STATE_MASK	(3 << 0)
#define VGIC_LR_HW		(1 << 2)
#define VGIC_LR_EOI_INT		(1 << 3)

struct vgic_lr {
	u16 virtid;
	u16 physid;
	u16 cpuid;
	u8 prio;
	u8 flags;
};

struct vgic_v2_hw_state {
	u32 hcr;
	u32 vmcr;
	u32 apr;
	u32 lr[VGIC_V2_MAX_LRS];
};

struct vgic_v3_hw_state {
	u32 hcr;
	u32 vmcr;
	u32 sre; /* Restored only, change ignored */
	u32 ap0r[4];
	u32 ap1r[4];
	u64 lr[VGIC_V3_MAX_LRS];
};

struct vgic_hw_state {
	union {
		struct vgic_v2_hw_state v2;
		struct vgic_v3_hw_state v3;
	};
};

struct vgic_params {
	enum vgic_type type;

	bool can_emulate_gic_v2;
	bool can_emulate_gic_v3;

	physical_addr_t vcpu_pa;

	u32 maint_irq;
	u32 lr_cnt;
};

struct vgic_ops {
	void (*reset_state)(struct vgic_hw_state *state);
	void (*save_state)(struct vgic_hw_state *state);
	void (*restore_state)(struct vgic_hw_state *state);
	bool (*check_underflow)(void);
	void (*enable_underflow)(void);
	void (*disable_underflow)(void);
	void (*read_elrsr)(u32 *elrsr0, u32 *elrsr1);
	void (*set_lr)(u32 lr, struct vgic_lr *lrv);
	void (*get_lr)(u32 lr, struct vgic_lr *lrv);
	void (*clear_lr)(u32 lr);
};

int vgic_v2_probe(struct vgic_ops *ops, struct vgic_params *params);
void vgic_v2_remove(struct vgic_ops *ops, struct vgic_params *params);

#endif /* __VGIC_H__ */
