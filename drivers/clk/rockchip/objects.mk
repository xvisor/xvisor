#
# Rockchip Clock specific Makefile
#

drivers-objs-$(CONFIG_COMMON_CLK_RK3399) += clk/rockchip/clk-rockchip-drv.o

clk-rockchip-drv-y	+= clk-rockchip.o
clk-rockchip-drv-y	+= clk.o
clk-rockchip-drv-y	+= clk-pll.o
clk-rockchip-drv-y	+= clk-cpu.o
clk-rockchip-drv-y	+= clk-inverter.o
clk-rockchip-drv-y	+= clk-mmc-phase.o
clk-rockchip-drv-y	+= clk-muxgrf.o
#clk-rockchip-drv-y	+= clk-ddr.o
clk-rockchip-drv-y	+= clk-half-divider.o
#clk-rockchip-drv-y	+= clk-pvtm.o
clk-rockchip-drv-$(CONFIG_RESET_CONTROLLER)	+= softrst.o

clk-rockchip-drv-y	+= clk-rk3399.o

%/clk-rockchip-drv.o: $(foreach obj,$(clk-rockchip-drv-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/clk-rockchip-drv.dep: $(foreach dep,$(clk-rockchip-drv-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

