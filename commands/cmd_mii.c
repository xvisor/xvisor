/**
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Created by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * from mii-tool.c, v 1.9 2006-09-27 20:59:18
 *   Copyright (C) 2000 David A. Hinds -- dhinds@pcmcia.sourceforge.org
 *
 *   mii-diag is written/copyright 1997-2000 by Donald Becker
 *      <becker@scyld.com>
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
 * @file cmd_mii.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Media Independent Interface (MII) commands.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#include <linux/types.h>
#include <linux/phy.h>
#include <linux/sockios.h>
#include <uapi/linux/mii.h>

/* network interface ioctl's for MII commands */
#ifndef SIOCGMIIPHY
#warning "SIOCGMIIPHY is not defined by your kernel source"
#define SIOCGMIIPHY (SIOCDEVPRIVATE)	/* Read from current PHY */
#define SIOCGMIIREG (SIOCDEVPRIVATE+1) 	/* Read any PHY register */
#define SIOCSMIIREG (SIOCDEVPRIVATE+2) 	/* Write any PHY register */
#define SIOCGPARAMS (SIOCDEVPRIVATE+3) 	/* Read operational parameters */
#define SIOCSPARAMS (SIOCDEVPRIVATE+4) 	/* Set operational parameters */
#endif

#define ADVERTISE_ABILITY_MASK	0x07e0

#define MODULE_DESC			"MII-tool commands"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_mii_init
#define	MODULE_EXIT			cmd_mii_exit

#define MAX_ETH				32

/* Table of known MII's */
static const struct {
    u_short	id1, id2;
    char	*name;
} mii_id[] = {
    { 0x0022, 0x5610, "AdHoc AH101LF" },
    { 0x0022, 0x5520, "Altimata AC101LF" },
    { 0x0000, 0x6b90, "AMD 79C901A HomePNA" },
    { 0x0000, 0x6b70, "AMD 79C901A 10baseT" },
    { 0x0181, 0xb800, "Davicom DM9101" },
    { 0x0043, 0x7411, "Enable EL40-331" },
    { 0x0015, 0xf410, "ICS 1889" },
    { 0x0015, 0xf420, "ICS 1890" },
    { 0x0015, 0xf430, "ICS 1892" },
    { 0x02a8, 0x0150, "Intel 82555" },
    { 0x7810, 0x0000, "Level One LXT970/971" },
    { 0x2000, 0x5c00, "National DP83840A" },
    { 0x0181, 0x4410, "Quality QS6612" },
    { 0x0282, 0x1c50, "SMSC 83C180" },
    { 0x0300, 0xe540, "TDK 78Q2120" },
    { 0x0141, 0x0c20, "Yukon 88E1011" },
    { 0x0141, 0x0cc0, "Yukon-EC 88E1111" },
    { 0x0141, 0x0c90, "Yukon-2 88E1112" },
};
#define NMII (sizeof(mii_id)/sizeof(mii_id[0]))

static int mdio_read(struct phy_device *phy, int location)
{
	return mdiobus_read(phy->bus, phy->addr, location);
}

static void mdio_write(struct phy_device *phy, int location, int value)
{
	mdiobus_write(phy->bus, phy->addr, location, value);
}

const struct {
    char	*name;
    u_short	value[2];
} media[] = {
    /* The order through 100baseT4 matches bits in the BMSR */
    { "10baseT-HD",	{ADVERTISE_10HALF} },
    { "10baseT-FD",	{ADVERTISE_10FULL} },
    { "100baseTx-HD",	{ADVERTISE_100HALF} },
    { "100baseTx-FD",	{ADVERTISE_100FULL} },
    { "100baseT4",	{ADVERTISE_100BASE4} },
    { "100baseTx",	{ADVERTISE_100FULL | ADVERTISE_100HALF} },
    { "10baseT",	{ADVERTISE_10FULL | ADVERTISE_10HALF} },

    { "1000baseT-HD",	{0, ADVERTISE_1000HALF} },
    { "1000baseT-FD",	{0, ADVERTISE_1000FULL} },
    { "1000baseT",	{0, ADVERTISE_1000HALF|ADVERTISE_1000FULL} },
};
#define NMEDIA (sizeof(media)/sizeof(media[0]))

/* Parse an argument list of media types */
static int parse_media(struct vmm_chardev *cdev, char *arg)
{
	int mask, i;
	char *s;
	char *save;
	mask = strtoul(arg, &s, 16);
	if ((*arg != '\0') && (*s == '\0')) {
		if ((mask & ADVERTISE_ABILITY_MASK) &&
		    !(mask & ~ADVERTISE_ABILITY_MASK)) {
			return mask;
		}
		goto failed;
	}
	mask = 0;
	s = strtok_r(arg, ", ", &save);
	do {
		for (i = 0; i < NMEDIA; i++) {
			if (s && strcasecmp(media[i].name, s) == 0) {
				break;
			}
		}
		if (i == NMEDIA) {
			goto failed;
		}
		mask |= media[i].value[0];
	} while ((s = strtok_r(NULL, ", ", &save)) != NULL);

	return mask;
failed:
	vmm_cprintf(cdev, "Invalid media specification '%s'.\n", arg);
	return -1;
}

static void cmd_mii_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   mii restart [interface ...]\n");
	vmm_cprintf(cdev, "   mii reset [interface ...]\n");
	vmm_cprintf(cdev, "   mii advertise media [interface ...]\n");
	vmm_cprintf(cdev, "   mii speed media [interface ...]\n");
	vmm_cprintf(cdev, "   mii watch [interface ...]\n");
	vmm_cprintf(cdev, "media: 1000baseTx-HD, 1000baseTx-FD,\n"
		    "       100baseT4, 100baseTx-FD, 100baseTx-HD,\n"
		    "       10baseT-FD, 10baseT-HD,\n"
		    "       (to advertise both HD and FD) 1000baseTx, "
		    "100baseTx, 10baseT\n");
}

struct cmd_mii_list_priv;
typedef int (*cmd_fct)(struct phy_device *phy,
		       struct cmd_mii_list_priv *priv);

struct cmd_mii_list_priv {
	struct vmm_chardev *cdev;
	int interface;
	cmd_fct fct;
	int argc;
	char **argv;
};

static int cmd_mii_reset(struct phy_device *phy,
			     struct cmd_mii_list_priv *priv)
{
	vmm_cprintf(priv->cdev, "Reseting %s\n", phy->dev.name);

	mdio_write(phy, MII_BMCR, BMCR_RESET);
	return VMM_OK;
}

static int cmd_mii_restart(struct phy_device *phy,
			       struct cmd_mii_list_priv *priv)
{
	vmm_cprintf(priv->cdev, "Restarting %s\n", phy->dev.name);

	mdio_write(phy, MII_BMCR, 0x0000);
	mdio_write(phy, MII_BMCR, BMCR_ANENABLE|BMCR_ANRESTART);
	return VMM_OK;
}

static int cmd_mii_advertise(struct phy_device *phy,
				 struct cmd_mii_list_priv *priv)
{
	int nway_advertise = 0;

	vmm_cprintf(priv->cdev, "Advertising %s\n", phy->dev.name);
	if (priv->argc < 2) {
		cmd_mii_usage(priv->cdev);
	}
	nway_advertise = parse_media(priv->cdev, priv->argv[2]);
	mdio_write(phy, MII_ADVERTISE, nway_advertise | 1);
	cmd_mii_restart(phy, priv);
	return VMM_OK;
}

static int cmd_mii_speed(struct phy_device *phy,
			     struct cmd_mii_list_priv *priv)
{
	int bmcr = 0;
	int fixed_speed = 0;

	vmm_cprintf(priv->cdev, "Setting %s speed\n", phy->dev.name);
	if (priv->argc < 2) {
		cmd_mii_usage(priv->cdev);
	}
	fixed_speed = parse_media(priv->cdev, priv->argv[2]);
	if (fixed_speed & (ADVERTISE_100FULL|ADVERTISE_100HALF))
	    bmcr |= BMCR_SPEED100;
	if (fixed_speed & (ADVERTISE_100FULL|ADVERTISE_10FULL))
	    bmcr |= BMCR_FULLDPLX;
	mdio_write(phy, MII_BMCR, bmcr);
	return VMM_OK;
}

static const char *media_list(struct vmm_chardev *cdev, unsigned mask,
			      unsigned mask2, int best)
{
	int i;

	if (mask & BMCR_SPEED1000) {
		if (mask2 & ADVERTISE_1000HALF) {
			vmm_cprintf(cdev, " ");
			vmm_cprintf(cdev, "1000baseT-HD");
			if (best) goto out;
		}
		if (mask2 & ADVERTISE_1000FULL) {
			vmm_cprintf(cdev, " ");
			vmm_cprintf(cdev, "1000baseT-FD");
			if (best) goto out;
		}
	}
	mask >>= 5;
	for (i = 4; i >= 0; i--) {
		if (mask & (1<<i)) {
			vmm_cprintf(cdev, " ");
			vmm_cprintf(cdev, media[i].name);
			if (best) break;
		}
	}
out:
	if (mask & (1<<5))
		vmm_cprintf(cdev, " flow-control");
	return VMM_OK;
}

static int show_basic_mii(struct vmm_chardev *cdev, struct phy_device *phy)
{
	int i, mii_val[32];
	unsigned bmcr, bmsr, advert, lkpar, bmcr2, lpa2;

	/* Some bits in the BMSR are latched, but we can't rely on being
	   the only reader, so only the current values are meaningful */
	mdio_read(phy, MII_BMSR);
	for (i = 0; i < 32; i++) {
		mii_val[i] = mdio_read(phy, i);
	}

	if (mii_val[MII_BMCR] == 0xffff  || mii_val[MII_BMSR] == 0x0000) {
		vmm_cprintf(cdev, "  No MII transceiver present!.\n");
		return -1;
	}

	/* Descriptive rename. */
	bmcr = mii_val[MII_BMCR];
	bmsr = mii_val[MII_BMSR];
	advert = mii_val[MII_ADVERTISE];
	lkpar = mii_val[MII_LPA];
	bmcr2 = mii_val[MII_CTRL1000];
	lpa2 = mii_val[MII_STAT1000];

	if (bmcr & BMCR_ANENABLE) {
		if (bmsr & BMSR_ANEGCOMPLETE) {
			if (advert & lkpar) {
				vmm_cprintf(cdev, (lkpar & ADVERTISE_LPACK) ?
					    "negotiated" :
					    "no autonegotiation,");
				media_list(cdev, advert & lkpar,
					   bmcr2 & lpa2>>2, 1);
				vmm_cprintf(cdev, ", ");
			} else {
				vmm_cprintf(cdev, "autonegotiation failed, ");
			}
		} else if (bmcr & BMCR_ANRESTART) {
			vmm_cprintf(cdev, "autonegotiation restarted, ");
		}
	} else {
		vmm_cprintf(cdev, "%s Mbit, %s duplex, ",
			    ((bmcr2 & (ADVERTISE_1000HALF |
				       ADVERTISE_1000FULL)) &
			     lpa2 >> 2) ? "1000" :
			    (bmcr & BMCR_SPEED100) ? "100" : "10",
			    (bmcr & BMCR_FULLDPLX) ? "full" : "half");
	}
	vmm_cprintf(cdev, (bmsr & BMSR_LSTATUS) ? "link ok" :
		    "no link");

	vmm_cprintf(cdev, "  registers for MII PHY %d: ", phy->phy_id);
	for (i = 0; i < 32; i++)
		vmm_cprintf(cdev, "%s %04x", ((i % 8) ? "" : "\n   "),
			    mii_val[i]);
	vmm_cprintf(cdev, "\n");

	vmm_cprintf(cdev, "  product info: ");
	for (i = 0; i < NMII; i++)
		if ((mii_id[i].id1 == mii_val[2]) &&
		    (mii_id[i].id2 == (mii_val[3] & 0xfff0)))
			break;
	if (i < NMII) {
		vmm_cprintf(cdev, "%s rev %d\n", mii_id[i].name,
			    mii_val[3]&0x0f);
	} else {
		vmm_cprintf(cdev, "vendor %02x:%02x:%02x, model %d rev %d\n",
			    mii_val[2]>>10, (mii_val[2]>>2)&0xff,
			    ((mii_val[2]<<6)|(mii_val[3]>>10))&0xff,
			    (mii_val[3]>>4)&0x3f, mii_val[3]&0x0f);
	}
	vmm_cprintf(cdev, "  basic mode:   ");
	if (bmcr & BMCR_RESET) {
		vmm_cprintf(cdev, "software reset, ");
	}
	if (bmcr & BMCR_LOOPBACK) {
		vmm_cprintf(cdev, "loopback, ");
	}
	if (bmcr & BMCR_ISOLATE) {
		vmm_cprintf(cdev, "isolate, ");
	}
	if (bmcr & BMCR_CTST) {
		vmm_cprintf(cdev, "collision test, ");
	}
	if (bmcr & BMCR_ANENABLE) {
		vmm_cprintf(cdev, "autonegotiation enabled\n");
	} else {
		vmm_cprintf(cdev, "%s Mbit, %s duplex\n",
			    (bmcr & BMCR_SPEED100) ? "100" : "10",
			    (bmcr & BMCR_FULLDPLX) ? "full" : "half");
	}
	vmm_cprintf(cdev, "  basic status: ");
	if (bmsr & BMSR_ANEGCOMPLETE)
		vmm_cprintf(cdev, "autonegotiation complete, ");
	else if (bmcr & BMCR_ANRESTART)
		vmm_cprintf(cdev, "autonegotiation restarted, ");
	if (bmsr & BMSR_RFAULT)
		vmm_cprintf(cdev, "remote fault, ");
	vmm_cprintf(cdev, (bmsr & BMSR_LSTATUS) ? "link ok" :
		    "no link");
	vmm_cprintf(cdev, "\n  capabilities: ");
	media_list(cdev, bmsr >> 6, bmcr2, 0);
	vmm_cprintf(cdev, "\n  advertising: ");
	media_list(cdev, advert, lpa2 >> 2, 0);
	if (lkpar & ADVERTISE_ABILITY_MASK) {
		vmm_cprintf(cdev, "\n  link partner: ");
		media_list(cdev, lkpar, bmcr2, 0);
	}
	vmm_cprintf(cdev, "\n");

    return 0;
}

static int cmd_mii_watch(struct phy_device *phy,
			     struct cmd_mii_list_priv *priv)
{
	vmm_cprintf(priv->cdev, "Watching %s\n", phy->dev.name);
	show_basic_mii(priv->cdev, phy);

	return VMM_OK;
}

static int cmd_mii_exec_fct(struct vmm_device *dev, void *data)
{
	struct phy_device *phy = to_phy_device(dev);
	struct cmd_mii_list_priv *priv = data;

	if ((-1 != priv->interface) && (priv->interface != phy->addr)) {
		return VMM_OK;
	}
	return priv->fct(phy, priv);
}

static int cmd_mii_match_if(struct vmm_device *dev, void *data)
{
	struct phy_device *phy = to_phy_device(dev);
	struct cmd_mii_list_priv *priv = data;

	return (priv->interface == phy->addr);
}

static int cmd_mii_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int ret = VMM_EFAIL;
	int args = 0;
	struct cmd_mii_list_priv priv;
	cmd_fct fct;

	if (argc < 2) {
		cmd_mii_usage(cdev);
		return VMM_EFAIL;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_mii_usage(cdev);
		return VMM_OK;
	} else if (strcmp(argv[1], "reset") == 0) {
		fct = cmd_mii_reset;
		args = 2;
	} else if (strcmp(argv[1], "advertise") == 0) {
		fct = cmd_mii_advertise;
		args = 3;
	} else if (strcmp(argv[1], "restart") == 0) {
		fct = cmd_mii_restart;
		args = 2;
	} else if (strcmp(argv[1], "speed") == 0) {
		fct = cmd_mii_speed;
		args = 3;
	} else if (strcmp(argv[1], "watch") == 0) {
		fct = cmd_mii_watch;
		args = 2;
	} else {
		cmd_mii_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc == args + 1) {
		priv.interface = atoi(argv[args]);
	} else {
		priv.interface = -1;
	}
	priv.cdev = cdev;
	priv.argc = argc;
	priv.argv = argv;
	priv.fct = fct;

	if (-1 != priv.interface) {
		struct vmm_device *dev = NULL;

		dev = vmm_devdrv_bus_find_device(&mdio_bus_type,
						 NULL, &priv,
						 cmd_mii_match_if);
		return cmd_mii_exec_fct(dev, &priv);
	} else {
		if (cmd_mii_watch == priv.fct) {
			ret = VMM_OK;
			/* while (VMM_OK == ret) { */
				ret = vmm_devdrv_bus_device_iterate(
					&mdio_bus_type, NULL, &priv,
					cmd_mii_exec_fct);
			/* } */
			return ret;
		}
		return vmm_devdrv_bus_device_iterate(&mdio_bus_type, NULL,
						     &priv,
						     cmd_mii_exec_fct);
	}
}

static struct vmm_cmd cmd_mii = {
	.name = "mii",
	.desc = MODULE_DESC,
	.usage = cmd_mii_usage,
	.exec = cmd_mii_exec,
};

static int __init cmd_mii_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_mii);
}

static void __exit cmd_mii_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_mii);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
