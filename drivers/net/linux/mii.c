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
 * @file mii.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief MII interface library
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/net/mii.c
 *
 * Maintained by Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2001,2002 Jeff Garzik
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <linux/mii.h>

/**
 * mii_check_gmii_support - check if the MII supports Gb interfaces
 * @mii: the MII interface
 */
int mii_check_gmii_support(struct mii_if_info *mii)
{
	int reg;

	reg = mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR);
	if (reg & BMSR_ESTATEN) {
		reg = mii->mdio_read(mii->dev, mii->phy_id, MII_ESTATUS);
		if (reg & (ESTATUS_1000_TFULL | ESTATUS_1000_THALF))
			return 1;
	}

	return 0;
}

/**
 * mii_link_ok - is link status up/ok
 * @mii: the MII interface
 *
 * Returns 1 if the MII reports link status up/ok, 0 otherwise.
 */
int mii_link_ok (struct mii_if_info *mii)
{
	/* first, a dummy read, needed to latch some MII phys */
	mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR);
	if (mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR) & BMSR_LSTATUS)
		return 1;
	return 0;
}

/**
 * mii_nway_restart - restart NWay (autonegotiation) for this interface
 * @mii: the MII interface
 *
 * Returns 0 on success, negative on error.
 */
int mii_nway_restart (struct mii_if_info *mii)
{
	int bmcr;
	int r = VMM_EINVALID;

	/* if autoneg is off, it's an error */
	bmcr = mii->mdio_read(mii->dev, mii->phy_id, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		bmcr |= BMCR_ANRESTART;
		mii->mdio_write(mii->dev, mii->phy_id, MII_BMCR, bmcr);
		r = 0;
	}

	return r;
}

/**
 * mii_check_link - check MII link status
 * @mii: MII interface
 *
 * If the link status changed (previous != current), call
 * netif_carrier_on() if current link status is Up or call
 * netif_carrier_off() if current link status is Down.
 */
void mii_check_link (struct mii_if_info *mii)
{
	int cur_link = mii_link_ok(mii);
	int prev_link = vmm_netif_carrier_ok(mii->dev);

	if (cur_link && !prev_link)
		vmm_netif_carrier_on(mii->dev);
	else if (prev_link && !cur_link)
		vmm_netif_carrier_off(mii->dev);
}

/**
 * mii_check_media - check the MII interface for a duplex change
 * @mii: the MII interface
 * @ok_to_print: OK to print link up/down messages
 * @init_media: OK to save duplex mode in @mii
 *
 * Returns 1 if the duplex mode changed, 0 if not.
 * If the media type is forced, always returns 0.
 */
unsigned int mii_check_media (struct mii_if_info *mii,
			      unsigned int ok_to_print,
			      unsigned int init_media)
{
	unsigned int old_carrier, new_carrier;
	int advertise, lpa, media, duplex;
	int lpa2 = 0;

	/* if forced media, go no further */
	if (mii->force_media)
		return 0; /* duplex did not change */

	/* check current and old link status */
	old_carrier = vmm_netif_carrier_ok(mii->dev) ? 1 : 0;
	new_carrier = (unsigned int) mii_link_ok(mii);

	/* if carrier state did not change, this is a "bounce",
	 * just exit as everything is already set correctly
	 */
	if ((!init_media) && (old_carrier == new_carrier))
		return 0; /* duplex did not change */

	/* no carrier, nothing much to do */
	if (!new_carrier) {
		vmm_netif_carrier_off(mii->dev);
		if (ok_to_print)
			vmm_printf("%s: link down\n", mii->dev->name);

		return 0; /* duplex did not change */
	}

	/*
	 * we have carrier, see who's on the other end
	 */
	vmm_netif_carrier_on(mii->dev);

	/* get MII advertise and LPA values */
	if ((!init_media) && (mii->advertising))
		advertise = mii->advertising;
	else {
		advertise = mii->mdio_read(mii->dev, mii->phy_id, MII_ADVERTISE);
		mii->advertising = advertise;
	}
	lpa = mii->mdio_read(mii->dev, mii->phy_id, MII_LPA);
	if (mii->supports_gmii)
		lpa2 = mii->mdio_read(mii->dev, mii->phy_id, MII_STAT1000);

	/* figure out media and duplex from advertise and LPA values */
	media = mii_nway_result(lpa & advertise);
	duplex = (media & ADVERTISE_FULL) ? 1 : 0;
	if (lpa2 & LPA_1000FULL)
		duplex = 1;
#if 0
	if (ok_to_print)
		printk(KERN_INFO "%s: link up, %sMbps, %s-duplex, lpa 0x%04X\n",
		       mii->dev->name,
		       lpa2 & (LPA_1000FULL | LPA_1000HALF) ? "1000" :
		       media & (ADVERTISE_100FULL | ADVERTISE_100HALF) ? "100" : "10",
		       duplex ? "full" : "half",
		       lpa);
#endif
	if (ok_to_print)
		vmm_printf("%s: link up, %sMbps, %s-duplex, lpa 0x%04X\n",
		       mii->dev->name,
		       lpa2 & (LPA_1000FULL | LPA_1000HALF) ? "1000" :
		       media & (ADVERTISE_100FULL | ADVERTISE_100HALF) ? "100" : "10",
		       duplex ? "full" : "half",
		       lpa);

	if ((init_media) || (mii->full_duplex != duplex)) {
		mii->full_duplex = duplex;
		return 1; /* duplex changed */
	}

	return 0; /* duplex did not change */
}

