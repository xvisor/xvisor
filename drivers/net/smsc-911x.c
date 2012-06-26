/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
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
 * @file smsc-911x.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Driver for SMSC-911x network controller.
 */

#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <net/vmm_net.h>
#include <net/vmm_netdev.h>
#include <net/vmm_netswitch.h>
#include <smsc-911x.h>

#define MODULE_VARID			smsc_911x_driver_module
#define MODULE_NAME			"SMSC 911x Ethernet Controller Driver"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY + 1)
#define	MODULE_INIT			smsc_911x_driver_init
#define	MODULE_EXIT			smsc_911x_driver_exit

static inline void vmm_netdev_set_priv(struct vmm_netdev *ndev, void *priv)
{
	if (ndev && priv)
		ndev->priv = priv;
}

static inline void *vmm_netdev_get_priv(struct vmm_netdev *ndev)
{
	if (ndev)
		return ndev->priv;

	return NULL;
}

/* Debugging options */
#if 0
#define ENABLE_SMC_DEBUG_RX             0
#define ENABLE_SMC_DEBUG_TX             0
#define ENABLE_SMC_DEBUG_DMA            0
#define ENABLE_SMC_DEBUG_PKTS           0
#define ENABLE_SMC_DEBUG_MISC           0
#define ENABLE_SMC_DEBUG_FUNC           0
#endif

#define ENABLE_SMC_DEBUG_RX             1
#define ENABLE_SMC_DEBUG_TX             1
#define ENABLE_SMC_DEBUG_DMA            1
#define ENABLE_SMC_DEBUG_PKTS           1
#define ENABLE_SMC_DEBUG_MISC           1
#define ENABLE_SMC_DEBUG_FUNC           1


#define SMC_DEBUG_RX            ((ENABLE_SMC_DEBUG_RX   ? 1 : 0) << 0)
#define SMC_DEBUG_TX            ((ENABLE_SMC_DEBUG_TX   ? 1 : 0) << 1)
#define SMC_DEBUG_DMA           ((ENABLE_SMC_DEBUG_DMA  ? 1 : 0) << 2)
#define SMC_DEBUG_PKTS          ((ENABLE_SMC_DEBUG_PKTS ? 1 : 0) << 3)
#define SMC_DEBUG_MISC          ((ENABLE_SMC_DEBUG_MISC ? 1 : 0) << 4)
#define SMC_DEBUG_FUNC          ((ENABLE_SMC_DEBUG_FUNC ? 1 : 0) << 5)

#ifndef SMC_DEBUG
#define SMC_DEBUG        ( SMC_DEBUG_RX   | \
                           SMC_DEBUG_TX   | \
                           SMC_DEBUG_DMA  | \
                           SMC_DEBUG_PKTS | \
                           SMC_DEBUG_MISC | \
                           SMC_DEBUG_FUNC   \
                         )
#endif


/*
 * Transmit timeout, default 5 seconds.
 */
static int watchdog = 5000;
static int tx_fifo_kb=8;

/*
 * The internal workings of the driver.  If you are changing anything
 * here with the SMC stuff, you should have the datasheet and know
 * what you are doing.
 */
#define CARDNAME "smc911x"

/*
 * Use power-down feature of the chip
 */
#define POWER_DOWN               1

#if SMC_DEBUG > 0
#define DBG(n, args...)                          \
	do {                                     \
		if (SMC_DEBUG & (n))             \
		vmm_printf(args);            \
	} while (0)

#define PRINTK(args...)   vmm_printf(args)
#else
#define DBG(n, args...)   do { } while (0)
#define PRINTK(args...)   vmm_printf(args)
#endif

#if 0
static struct smsc911x_platform_config smsc911x_config = {
	.flags          = SMSC911X_USE_32BIT,
	.irq_polarity   = SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_type       = SMSC911X_IRQ_TYPE_PUSH_PULL,
};
#endif

static int smsc_911x_init(struct vmm_netdev *ndev)
{
	int rc = VMM_OK;

	vmm_printf("Inside %s\n", __func__);

	return rc;
}

static struct vmm_netdev_ops smsc_911x_vmm_netdev_ops = {
	.ndev_init = smsc_911x_init,
};


static int smc911x_probe(struct vmm_netdev *dev)
{
        struct smc911x_local *lp;
        int i, retval = 0;
        unsigned int val, chip_id, revision;
        const char *version_string;
        unsigned long irq_flags;

	lp = vmm_netdev_get_priv(dev);

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	/* First, see if the endian word is recognized */
	val = SMC_GET_BYTE_TEST(lp);
	vmm_printf("%s: endian probe returned 0x%04x\n", CARDNAME, val);
	if (val != 0x87654321) {
		vmm_printf("Invalid chip endian 0x%08x\n",val);
		retval = VMM_ENODEV;
		goto err_out;
	}

	/*
	 * check if the revision register is something that I
	 * recognize.   These might need to be added to later,
	 * as future revisions could be added.
	 */
	chip_id = SMC_GET_PN(lp);
	vmm_printf("%s: id probe returned 0x%04x\n", CARDNAME, chip_id);
	for(i = 0; chip_ids[i].id != 0; i++) {
		if (chip_ids[i].id == chip_id)
			break;
	}
	if (!chip_ids[i].id) {
		vmm_printf("Unknown chip ID %04x\n", chip_id);
		retval = VMM_ENODEV;
		goto err_out;
	}
	version_string = chip_ids[i].name;

	revision = SMC_GET_REV(lp);
	vmm_printf("%s: revision = 0x%04x\n", CARDNAME, revision);

	/* At this point I'll assume that the chip is an SMC911x. */
	DBG(SMC_DEBUG_MISC, "%s: Found a %s\n", CARDNAME, chip_ids[i].name);

	/* Validate the TX FIFO size requested */
	if ((tx_fifo_kb < 2) || (tx_fifo_kb > 14)) {
		vmm_printf("Invalid TX FIFO size requested %d\n", tx_fifo_kb);
		retval = VMM_EINVALID;
		goto err_out;
	}

	/* fill in some of the fields */
	lp->version = chip_ids[i].id;
	lp->revision = revision;
	lp->tx_fifo_kb = tx_fifo_kb;
	/* Reverse calculate the RX FIFO size from the TX */
	lp->tx_fifo_size=(lp->tx_fifo_kb<<10) - 512;
	lp->rx_fifo_size= ((0x4000 - 512 - lp->tx_fifo_size) / 16) * 15;

	/* Set the automatic flow control values */
	switch(lp->tx_fifo_kb) {
		/*
		 *       AFC_HI is about ((Rx Data Fifo Size)*2/3)/64
		 *       AFC_LO is AFC_HI/2
		 *       BACK_DUR is about 5uS*(AFC_LO) rounded down
		 */
		case 2:/* 13440 Rx Data Fifo Size */
			lp->afc_cfg=0x008C46AF;break;
		case 3:/* 12480 Rx Data Fifo Size */
			lp->afc_cfg=0x0082419F;break;
		case 4:/* 11520 Rx Data Fifo Size */
			lp->afc_cfg=0x00783C9F;break;
		case 5:/* 10560 Rx Data Fifo Size */
			lp->afc_cfg=0x006E374F;break;
		case 6:/* 9600 Rx Data Fifo Size */
			lp->afc_cfg=0x0064328F;break;
		case 7:/* 8640 Rx Data Fifo Size */
			lp->afc_cfg=0x005A2D7F;break;
		case 8:/* 7680 Rx Data Fifo Size */
			lp->afc_cfg=0x0050287F;break;
		case 9:/* 6720 Rx Data Fifo Size */
			lp->afc_cfg=0x0046236F;break;
		case 10:/* 5760 Rx Data Fifo Size */
			lp->afc_cfg=0x003C1E6F;break;
		case 11:/* 4800 Rx Data Fifo Size */
			lp->afc_cfg=0x0032195F;break;
			/*
			 *       AFC_HI is ~1520 bytes less than RX Data Fifo Size
			 *       AFC_LO is AFC_HI/2
			 *       BACK_DUR is about 5uS*(AFC_LO) rounded down
			 */
		case 12:/* 3840 Rx Data Fifo Size */
			lp->afc_cfg=0x0024124F;break;
		case 13:/* 2880 Rx Data Fifo Size */
			lp->afc_cfg=0x0015073F;break;
		case 14:/* 1920 Rx Data Fifo Size */
			lp->afc_cfg=0x0006032F;break;
		default:
			PRINTK("%s: ERROR -- no AFC_CFG setting found",
					dev->name);
			break;
	}

	DBG(SMC_DEBUG_MISC | SMC_DEBUG_TX | SMC_DEBUG_RX,
			"%s: tx_fifo %d rx_fifo %d afc_cfg 0x%08x\n", CARDNAME,
			lp->tx_fifo_size, lp->rx_fifo_size, lp->afc_cfg);


err_out:

	return retval;
}



static int smsc_911x_driver_probe(struct vmm_device *dev,
				  const struct vmm_devid *devid)
{
	int rc;
	struct vmm_netdev *ndev;
	virtual_addr_t addr;
	struct smc911x_local *lp;
	const char *attr;
	int ret = 0;

	vmm_printf("Inside smsc_911x_driver_probe\n");

	lp = vmm_malloc(sizeof(struct smc911x_local));
	if (!lp) {
		vmm_printf("%s Failed to allocate private data structure for"
			"%s\n", __func__, dev->node->name);
			rc = VMM_EFAIL;
			goto free_nothing;
	}

	vmm_memset(lp, 0, sizeof(struct smc911x_local));

	ndev = vmm_netdev_alloc(dev->node->name);
	if (!ndev) {
		vmm_printf("%s Failed to allocate vmm_netdev for %s\n", __func__,
				dev->node->name);
		rc = VMM_EFAIL;
		goto free_lp;
	}


	dev->priv = (void *) ndev;
	ndev->dev_ops = &smsc_911x_vmm_netdev_ops;
	vmm_netdev_set_priv(ndev, lp);

	rc = vmm_devdrv_regmap(dev, &addr, 0);
	if (rc) {
		vmm_printf("Failed to ioreamp\n");
		goto free_ndev;
	}

	vmm_printf("vmm_devdrv_regmap success at address 0x%02X\n", addr);
	lp->base = (void *) addr;

	attr = vmm_devtree_attrval(dev->node, "irq");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_ioreamp_mem;
	}

	ndev->irq = *((u32 *) attr);
	vmm_printf("%s IRQ  0x%02X\n", ndev->name, ndev->irq);

#ifdef SMC_DYNAMIC_BUS_CONFIG
        lp->cfg.flags = SMC911X_USE_32BIT;
#endif

	ret = smc911x_probe(ndev);

	rc = vmm_netdev_register(ndev);
	if (rc != VMM_OK) {
		vmm_printf("%s Failed to register net device %s\n", __func__,
				dev->node->name);
		goto free_ioreamp_mem;

	}

	vmm_printf("Successfully registered Network Device %s\n", ndev->name);

	return VMM_OK;

free_ioreamp_mem:
	//vmm_devdrv_ioreunmap
free_ndev:
	vmm_free(ndev);
free_lp:
	vmm_free(lp);
free_nothing:
	return rc;
}

static int smsc_911x_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct vmm_netdev *ndev = (struct vmm_netdev *) dev->priv;

	if (ndev) {
		rc = vmm_netdev_unregister(ndev);
		vmm_free(ndev->priv);
		vmm_free(ndev);
		dev->priv = NULL;
	}

	return rc;
}


static struct vmm_devid smsc_911x_devid_table[] = {
	{ .type = "nic", .compatible = "smsc911x"},
	{ /* end of list */ },
};

static struct vmm_driver smsc_911x_driver = {
	.name = "smsc_911x_driver",
	.match_table = smsc_911x_devid_table,
	.probe = smsc_911x_driver_probe,
	.remove = smsc_911x_driver_remove,
};

static int __init smsc_911x_driver_init(void)
{
	return vmm_devdrv_register_driver(&smsc_911x_driver);
}

static void smsc_911x_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&smsc_911x_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		MODULE_NAME,
		MODULE_AUTHOR,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
