/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file apbh.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief AHB-to-APBH bridge with DMA emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"APBH-Bridge-DMA Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define MODULE_INIT			apbh_emulator_init
#define MODULE_EXIT			apbh_emulator_exit

#define CTRL0_NB 4

struct apbh_state {
	u32 ctrl[CTRL0_NB]; /* Only ctrl0 */

	vmm_spinlock_t lock;
};

static inline void _lock(struct apbh_state *s)
{
	vmm_spin_lock(&s->lock);
}

static inline void _unlock(struct apbh_state *s)
{
	vmm_spin_unlock(&s->lock);
}

static int apbh_emulator_read(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 *dst,
				u32 size)
{
	struct apbh_state *s = edev->priv;
	const unsigned int idx = offset >> 2;

	_lock(s);
	if (offset <= 0x00C) {
		*dst = s->ctrl[idx];
	} else if (offset == 0x800) {
		*dst = 0x03010000;
	} else if (offset == 0x050) {
		*dst = 0x00555555;
	} else if ((offset >= 0x150) &&
		   (umod64((offset - 0x150), 0x70) == 0)) {
		*dst = 0x00A00000;
	} else {
		vmm_lwarning("APBH",
			"reading from an unhanded register: %"PRIPADDR"\n",
			offset);
		*dst = 0x00000000;
	}
	_unlock(s);

	return VMM_OK;
}

static int apbh_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 regval,
				u32 mask,
				u32 size)
{
	struct apbh_state *s = edev->priv;
	const unsigned int idx = offset >> 2;

	_lock(s);
	if (offset <= 0x00C) {
		s->ctrl[idx] = regval;
	} else {
		vmm_lwarning("APBH",
			"writing in unhandled register: %"PRIPADDR"\n",
			offset);
	}
	_unlock(s);

	return VMM_OK;
}

static void _reset(struct apbh_state *s)
{
	unsigned int i;

	for (i = 0; i < CTRL0_NB; ++i) {
		s->ctrl[i] = 0xE0000000;
	}
}

static int apbh_emulator_reset(struct vmm_emudev *edev)
{
	struct apbh_state *s = edev->priv;

	_lock(s);
	_reset(s);
	_unlock(s);

	return VMM_OK;
}

static int apbh_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	struct apbh_state *s;
	int rc = VMM_OK;

	s = vmm_zalloc(sizeof(*s));
	if (NULL == s) {
		vmm_lerror("APBH", "failed to allocate memory");
		rc = VMM_ENOMEM;
		goto end;
	}

	_reset(s);
	INIT_SPIN_LOCK(&(s->lock));
	edev->priv = s;
end:
	return rc;
}

static int apbh_emulator_remove(struct vmm_emudev *edev)
{
	struct apbh_state *s = edev->priv;
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid apbh_emuid_table[] = {
	{ .type = "misc",
	  .compatible = "fsl,imx6q-dma-apbh",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(apbh_emulator,
				"apbh-bridge-dma",
				apbh_emuid_table,
				VMM_DEVEMU_NATIVE_ENDIAN,
				apbh_emulator_probe,
				apbh_emulator_remove,
				apbh_emulator_reset,
				apbh_emulator_read,
				apbh_emulator_write);

static int __init apbh_emulator_init(void)
{
	return vmm_devemu_register_emulator(&apbh_emulator);
}

static void __exit apbh_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&apbh_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
