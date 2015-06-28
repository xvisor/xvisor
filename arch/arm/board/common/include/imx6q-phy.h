/**
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file imx6q-phy.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX6 Sabrelite board specific code for PHY
 *
 * Adapted from linux/arch/arm/mach-imx/mach-imx6q.c
 *
 * The original source is licensed under GPL.
 *
 */

#ifndef IMX6Q_PHY_H__
# define IMX6Q_PHY_H__

# ifdef CONFIG_PHYLIB

void __init imx6q_enet_phy_init(void);

# else /* CONFIG_PHYLIB */

static inline void __init imx6q_enet_phy_init(void)
{
}

# endif /* CONFIG_PHYLIB */
#endif /* IMX6Q_PHY_H__ */
