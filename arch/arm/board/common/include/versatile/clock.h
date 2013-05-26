#ifndef VERSATILE_CLOCK_H
#define VERSATILE_CLOCK_H

#include <vmm_devdrv.h>
#include <icst.h>

struct arch_clk_ops;

struct arch_clk {
	unsigned long			rate;
	const struct arch_clk_ops	*ops;
	const struct icst_params 	*params;
	void 				*vcoreg;
};

struct arch_clk_ops {
	long	(*round)(struct arch_clk *, unsigned long);
	int	(*set)(struct arch_clk *, unsigned long);
	void	(*setvco)(struct arch_clk *, struct icst_vco);
};

int icst_clk_set(struct arch_clk *, unsigned long);
long icst_clk_round(struct arch_clk *, unsigned long);

#endif
