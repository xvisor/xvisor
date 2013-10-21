#ifndef VERSATILE_CLOCK_H
#define VERSATILE_CLOCK_H

#include <icst.h>

struct clk_ops;

struct clk {
	unsigned long			rate;
	const struct clk_ops		*ops;
	const struct icst_params 	*params;
	void 				*vcoreg;
};

struct clk_ops {
	long	(*round)(struct clk *, unsigned long);
	int	(*set)(struct clk *, unsigned long);
	void	(*setvco)(struct clk *, struct icst_vco);
};

int icst_clk_set(struct clk *, unsigned long);
long icst_clk_round(struct clk *, unsigned long);

#endif
