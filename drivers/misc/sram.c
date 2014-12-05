/*
 * Generic on-chip SRAM allocation driver
 *
 * Copyright (C) 2012 Philipp Zabel, Pengutronix
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * @file sram.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Generic on-chip SRAM allocation driver
 */
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_devtree.h>
#include <vmm_devres.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <drv/clk.h>
#include <libs/genalloc.h>

#define MODULE_AUTHOR		"Jimmy Durand Wesolowski"
#define MODULE_LICENSE		"GPL"
#define MODULE_DESC		"Generic on-chip SRAM allocation driver"
#define MODULE_IPRIORITY	1
#define MODULE_INIT		sram_init
#define MODULE_EXIT		sram_exit

/* ilog2 of 4096 */
#define SRAM_GRANULARITY_LOG	12

struct sram_dev {
	struct gen_pool *pool;
	struct clk *clk;
};

static int sram_probe(struct vmm_device *dev,
		      const struct vmm_devtree_nodeid *nodeid)
{
	void *virt_base = NULL;
	struct sram_dev *sram = NULL;
	physical_addr_t start = 0;
	virtual_size_t size = 0;
	int ret = VMM_OK;

	ret = vmm_devtree_regaddr(dev->node, &start, 0);
	if (VMM_OK != ret) {
		vmm_printf("%s: Failed to get device base\n", dev->name);
		return ret;
	}

	ret = vmm_devtree_regsize(dev->node, &size, 0);
	if (VMM_OK != ret) {
		vmm_printf("%s: Failed to get device size\n", dev->name);
		goto err_out;
	}

	virt_base = (void *)vmm_host_iomap(start, size);
	if (NULL == virt_base) {
		vmm_printf("%s: Failed to get remap memory\n", dev->name);
		ret = VMM_ENOMEM;
		goto err_out;
	}

	sram = vmm_devm_zalloc(dev, sizeof(*sram));
	if (!sram) {
		vmm_printf("%s: Failed to allocate structure\n", dev->name);
		ret = VMM_ENOMEM;
		goto err_out;
	}

	sram->clk = devm_clk_get(dev, NULL);
	if (VMM_IS_ERR(sram->clk))
		sram->clk = NULL;
	else
		clk_prepare_enable(sram->clk);

	sram->pool = devm_gen_pool_create(dev, SRAM_GRANULARITY_LOG);
	if (!sram->pool) {
		vmm_printf("%s: Failed to create memory pool\n", dev->name);
		ret = VMM_ENOMEM;
	}

	ret = gen_pool_add_virt(sram->pool, (unsigned long)virt_base,
				start, size);
	if (ret < 0) {
		vmm_printf("%s: Failed to add memory chunk\n", dev->name);
		goto err_out;
	}

	vmm_devdrv_set_data(dev, sram);

	vmm_printf("%s: SRAM pool: %ld KiB @ 0x%p\n", dev->name, size / 1024,
		   virt_base);

	return 0;

err_out:
	if (sram->pool)
		gen_pool_destroy(sram->pool);

#if 0
	if (sram->clk)
		clk_disable_unprepare(sram->clk);
#endif /* 0 */

	if (sram)
		vmm_free(sram);
	sram = NULL;

	if (virt_base)
		vmm_host_iounmap((virtual_addr_t)virt_base);
	virt_base = NULL;

	return ret;
}

static int sram_remove(struct vmm_device *dev)
{
	struct sram_dev *sram = vmm_devdrv_get_data(dev);

	if (gen_pool_avail(sram->pool) < gen_pool_size(sram->pool))
		vmm_printf("%s: removed while SRAM allocated\n", dev->name);

	gen_pool_destroy(sram->pool);

	if (sram->clk)
		clk_disable_unprepare(sram->clk);

	return 0;
}

static struct vmm_devtree_nodeid sram_dt_ids[] = {
	{ .compatible = "mmio-sram" },
	{}
};

static struct vmm_driver sram_driver = {
	.name = "sram",
	.match_table = sram_dt_ids,
	.probe = sram_probe,
	.remove = sram_remove,
};

static int __init sram_init(void)
{
	return vmm_devdrv_register_driver(&sram_driver);
}

static void __exit sram_exit(void)
{
	vmm_devdrv_unregister_driver(&sram_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
