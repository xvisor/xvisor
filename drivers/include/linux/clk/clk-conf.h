/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWid
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski for Xvisor port.
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file clk-conf.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Device tree clock configuration helper.
 */

#ifndef _CLK_CONF_H
# define _CLK_CONF_H

struct vmm_devtree_node;

#ifdef CONFIG_COMMON_CLK
int of_clk_set_defaults(struct vmm_devtree_node *node, bool clk_supplier);
#else /* !CONFIG_COMMON_CLK */
static inline of_clk_set_defaults(struct vmm_devtree_node *node,
				  bool clk_supplier)
{
	return 0;
}
#endif /* CONFIG_COMMON_CLK */

#endif /* !_CLK_CONF_H */
