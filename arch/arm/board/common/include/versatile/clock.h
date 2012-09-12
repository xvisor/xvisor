#ifndef VERSATILE_CLOCK_H
#define VERSATILE_CLOCK_H

#include <vmm_devdrv.h>
#include <icst.h>

struct versatile_clk_ops;

struct versatile_clk {
	unsigned long			rate;
	const struct versatile_clk_ops	*ops;
	const struct icst_params 	*params;
	void 				*vcoreg;
};

struct versatile_clk_ops {
	long	(*round)(struct versatile_clk *, unsigned long);
	int	(*set)(struct versatile_clk *, unsigned long);
	void	(*setvco)(struct versatile_clk *, struct icst_vco);
};

int versatile_clk_enable(struct vmm_devclk *clk);
void versatile_clk_disable(struct vmm_devclk *clk);
unsigned long versatile_clk_get_rate(struct vmm_devclk *clk);
long versatile_clk_round_rate(struct vmm_devclk *clk, unsigned long rate);
int versatile_clk_set_rate(struct vmm_devclk *clk, unsigned long rate);

int icst_clk_set(struct versatile_clk *, unsigned long);
long icst_clk_round(struct versatile_clk *, unsigned long);

#endif
