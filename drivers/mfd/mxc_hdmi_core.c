/*
 * Copyright (C) 2016 Open Wide
 * Copyright (C) 2016 Institut de Recherche Technologique SystemX
 *
 * Copyright (C) 2011-2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @file mxc_hdmi_core.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/platform_device.h>

#include <video/mxc_hdmi.h>
#include <video/mxc_edid.h>
#include "../gpu/ipu-v3/ipu_prv.h"
#include <linux/mfd/mxc-hdmi-core.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/mfd/mxc-hdmi-core.h>

#define MODULE_DESC			"MXC HDMI Core"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			mxc_hdmi_core_init
#define	MODULE_EXIT			mxc_hdmi_core_exit

struct mxc_hdmi_data {
	struct vmm_device *dev;
	unsigned long __iomem *reg_base;
};

static void __iomem *hdmi_base;
static struct clk *isfr_clk;
static struct clk *iahb_clk;
static spinlock_t irq_spinlock;
static spinlock_t edid_spinlock;
static unsigned int sample_rate;
static unsigned long pixel_clk_rate;
static struct clk *pixel_clk;
static int hdmi_ratio;
int mxc_hdmi_ipu_id;
int mxc_hdmi_disp_id;
static struct mxc_edid_cfg hdmi_core_edid_cfg;
static int hdmi_core_init;
static unsigned int hdmi_dma_running;
static unsigned int hdmi_cable_state;
static unsigned int hdmi_blank_state;
static unsigned int hdmi_abort_state;
static spinlock_t hdmi_audio_lock, hdmi_blank_state_lock, hdmi_cable_state_lock;

/*
 * FIXME
 * The #if 0 ... #endif here comment out AUDIO.
 * You can safely uncomment when audio is supported.
 */

#if 0
static struct snd_pcm_substream *hdmi_audio_stream_playback;
#endif

unsigned int hdmi_set_cable_state(unsigned int state)
{
#if 0
	unsigned long flags;
	struct snd_pcm_substream *substream = hdmi_audio_stream_playback;

	spin_lock_irqsave(&hdmi_cable_state_lock, flags);
	hdmi_cable_state = state;
	spin_unlock_irqrestore(&hdmi_cable_state_lock, flags);

	if (check_hdmi_state() && substream && hdmi_abort_state) {
		hdmi_abort_state = 0;
		substream->ops->trigger(substream, SNDRV_PCM_TRIGGER_START);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(hdmi_set_cable_state);

unsigned int hdmi_set_blank_state(unsigned int state)
{
#if 0
	unsigned long flags;
	struct snd_pcm_substream *substream = hdmi_audio_stream_playback;

	spin_lock_irqsave(&hdmi_blank_state_lock, flags);
	hdmi_blank_state = state;
	spin_unlock_irqrestore(&hdmi_blank_state_lock, flags);

	if (check_hdmi_state() && substream && hdmi_abort_state) {
		hdmi_abort_state = 0;
		substream->ops->trigger(substream, SNDRV_PCM_TRIGGER_START);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(hdmi_set_blank_state);

#if 0
static void hdmi_audio_abort_stream(struct snd_pcm_substream *substream)
{
	unsigned long flags;

	snd_pcm_stream_lock_irqsave(substream, flags);

	if (snd_pcm_running(substream)) {
		hdmi_abort_state = 1;
		substream->ops->trigger(substream, SNDRV_PCM_TRIGGER_STOP);
	}

	snd_pcm_stream_unlock_irqrestore(substream, flags);
}
#endif

int mxc_hdmi_abort_stream(void)
{
#if 0
	unsigned long flags;
	spin_lock_irqsave(&hdmi_audio_lock, flags);
	if (hdmi_audio_stream_playback)
		hdmi_audio_abort_stream(hdmi_audio_stream_playback);
	spin_unlock_irqrestore(&hdmi_audio_lock, flags);
#endif
	return 0;
}
EXPORT_SYMBOL(mxc_hdmi_abort_stream);

int check_hdmi_state(void)
{
	unsigned long flags1, flags2;
	unsigned int ret;

	spin_lock_irqsave(&hdmi_cable_state_lock, flags1);
	spin_lock_irqsave(&hdmi_blank_state_lock, flags2);

	ret = hdmi_cable_state && hdmi_blank_state;

	spin_unlock_irqrestore(&hdmi_blank_state_lock, flags2);
	spin_unlock_irqrestore(&hdmi_cable_state_lock, flags1);

	return ret;
}
EXPORT_SYMBOL(check_hdmi_state);

#if 0
int mxc_hdmi_register_audio(struct snd_pcm_substream *substream)
{
	unsigned long flags, flags1;
	int ret = 0;

	snd_pcm_stream_lock_irqsave(substream, flags);

	if (substream && check_hdmi_state()) {
		spin_lock_irqsave(&hdmi_audio_lock, flags1);
		if (hdmi_audio_stream_playback) {
			pr_err("%s unconsist hdmi auido stream!\n", __func__);
			ret = -EINVAL;
		}
		hdmi_audio_stream_playback = substream;
		hdmi_abort_state = 0;
		spin_unlock_irqrestore(&hdmi_audio_lock, flags1);
	} else
		ret = -EINVAL;

	snd_pcm_stream_unlock_irqrestore(substream, flags);

	return ret;
}
EXPORT_SYMBOL(mxc_hdmi_register_audio);

void mxc_hdmi_unregister_audio(struct snd_pcm_substream *substream)
{
	unsigned long flags;

	spin_lock_irqsave(&hdmi_audio_lock, flags);
	hdmi_audio_stream_playback = NULL;
	hdmi_abort_state = 0;
	spin_unlock_irqrestore(&hdmi_audio_lock, flags);
}
EXPORT_SYMBOL(mxc_hdmi_unregister_audio);
#endif

u8 hdmi_readb(unsigned int reg)
{
	u8 value;

	value = __raw_readb(hdmi_base + reg);

	return value;
}
EXPORT_SYMBOL(hdmi_readb);

void hdmi_writeb(u8 value, unsigned int reg)
{
	__raw_writeb(value, hdmi_base + reg);
}
EXPORT_SYMBOL(hdmi_writeb);

void hdmi_mask_writeb(u8 data, unsigned int reg, u8 shift, u8 mask)
{
	u8 value = hdmi_readb(reg) & ~mask;
	value |= (data << shift) & mask;
	hdmi_writeb(value, reg);
}
EXPORT_SYMBOL(hdmi_mask_writeb);

unsigned int hdmi_read4(unsigned int reg)
{
	/* read a four byte address from registers */
	return (hdmi_readb(reg + 3) << 24) |
		(hdmi_readb(reg + 2) << 16) |
		(hdmi_readb(reg + 1) << 8) |
		hdmi_readb(reg);
}
EXPORT_SYMBOL(hdmi_read4);

void hdmi_write4(unsigned int value, unsigned int reg)
{
	/* write a four byte address to hdmi regs */
	hdmi_writeb(value & 0xff, reg);
	hdmi_writeb((value >> 8) & 0xff, reg + 1);
	hdmi_writeb((value >> 16) & 0xff, reg + 2);
	hdmi_writeb((value >> 24) & 0xff, reg + 3);
}
EXPORT_SYMBOL(hdmi_write4);

static void initialize_hdmi_ih_mutes(void)
{
	u8 ih_mute;

	/*
	 * Boot up defaults are:
	 * HDMI_IH_MUTE   = 0x03 (disabled)
	 * HDMI_IH_MUTE_* = 0x00 (enabled)
	 */

	/* Disable top level interrupt bits in HDMI block */
	ih_mute = hdmi_readb(HDMI_IH_MUTE) |
		  HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT |
		  HDMI_IH_MUTE_MUTE_ALL_INTERRUPT;

	hdmi_writeb(ih_mute, HDMI_IH_MUTE);

	/* by default mask all interrupts */
	hdmi_writeb(0xff, HDMI_VP_MASK);
	hdmi_writeb(0xff, HDMI_FC_MASK0);
	hdmi_writeb(0xff, HDMI_FC_MASK1);
	hdmi_writeb(0xff, HDMI_FC_MASK2);
	hdmi_writeb(0xff, HDMI_PHY_MASK0);
	hdmi_writeb(0xff, HDMI_PHY_I2CM_INT_ADDR);
	hdmi_writeb(0xff, HDMI_PHY_I2CM_CTLINT_ADDR);
	hdmi_writeb(0xff, HDMI_AUD_INT);
	hdmi_writeb(0xff, HDMI_AUD_SPDIFINT);
	hdmi_writeb(0xff, HDMI_AUD_HBR_MASK);
	hdmi_writeb(0xff, HDMI_GP_MASK);
	hdmi_writeb(0xff, HDMI_A_APIINTMSK);
	hdmi_writeb(0xff, HDMI_CEC_MASK);
	hdmi_writeb(0xff, HDMI_I2CM_INT);
	hdmi_writeb(0xff, HDMI_I2CM_CTLINT);

	/* Disable interrupts in the IH_MUTE_* registers */
	hdmi_writeb(0xff, HDMI_IH_MUTE_FC_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_FC_STAT1);
	hdmi_writeb(0xff, HDMI_IH_MUTE_FC_STAT2);
	hdmi_writeb(0xff, HDMI_IH_MUTE_AS_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_PHY_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_I2CM_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_CEC_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_VP_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_I2CMPHY_STAT0);
	hdmi_writeb(0xff, HDMI_IH_MUTE_AHBDMAAUD_STAT0);

	/* Enable top level interrupt bits in HDMI block */
	ih_mute &= ~(HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT |
		    HDMI_IH_MUTE_MUTE_ALL_INTERRUPT);
	hdmi_writeb(ih_mute, HDMI_IH_MUTE);
}

static void hdmi_set_clock_regenerator_n(unsigned int value)
{
	u8 val;

	if (!hdmi_dma_running) {
		hdmi_writeb(value & 0xff, HDMI_AUD_N1);
		hdmi_writeb(0, HDMI_AUD_N2);
		hdmi_writeb(0, HDMI_AUD_N3);
	}

	hdmi_writeb(value & 0xff, HDMI_AUD_N1);
	hdmi_writeb((value >> 8) & 0xff, HDMI_AUD_N2);
	hdmi_writeb((value >> 16) & 0x0f, HDMI_AUD_N3);

	/* nshift factor = 0 */
	val = hdmi_readb(HDMI_AUD_CTS3);
	val &= ~HDMI_AUD_CTS3_N_SHIFT_MASK;
	hdmi_writeb(val, HDMI_AUD_CTS3);
}

static void hdmi_set_clock_regenerator_cts(unsigned int cts)
{
	u8 val;

	if (!hdmi_dma_running) {
		hdmi_writeb(cts & 0xff, HDMI_AUD_CTS1);
		hdmi_writeb(0, HDMI_AUD_CTS2);
		hdmi_writeb(0, HDMI_AUD_CTS3);
	}

	/* Must be set/cleared first */
	val = hdmi_readb(HDMI_AUD_CTS3);
	val &= ~HDMI_AUD_CTS3_CTS_MANUAL;
	hdmi_writeb(val, HDMI_AUD_CTS3);

	hdmi_writeb(cts & 0xff, HDMI_AUD_CTS1);
	hdmi_writeb((cts >> 8) & 0xff, HDMI_AUD_CTS2);
	hdmi_writeb(((cts >> 16) & HDMI_AUD_CTS3_AUDCTS19_16_MASK) |
		    HDMI_AUD_CTS3_CTS_MANUAL, HDMI_AUD_CTS3);
}

static unsigned int hdmi_compute_n(unsigned int freq, unsigned long pixel_clk,
				   unsigned int ratio)
{
	unsigned int n = (128 * freq) / 1000;

	switch (freq) {
	case 32000:
		if (pixel_clk == 25174000)
			n = (ratio == 150) ? 9152 : 4576;
		else if (pixel_clk == 27020000)
			n = (ratio == 150) ? 8192 : 4096;
		else if (pixel_clk == 74170000 || pixel_clk == 148350000)
			n = 11648;
		else if (pixel_clk == 297000000)
			n = (ratio == 150) ? 6144 : 3072;
		else
			n = 4096;
		break;

	case 44100:
		if (pixel_clk == 25174000)
			n = 7007;
		else if (pixel_clk == 74170000)
			n = 17836;
		else if (pixel_clk == 148350000)
			n = (ratio == 150) ? 17836 : 8918;
		else if (pixel_clk == 297000000)
			n = (ratio == 150) ? 9408 : 4704;
		else
			n = 6272;
		break;

	case 48000:
		if (pixel_clk == 25174000)
			n = (ratio == 150) ? 9152 : 6864;
		else if (pixel_clk == 27020000)
			n = (ratio == 150) ? 8192 : 6144;
		else if (pixel_clk == 74170000)
			n = 11648;
		else if (pixel_clk == 148350000)
			n = (ratio == 150) ? 11648 : 5824;
		else if (pixel_clk == 297000000)
			n = (ratio == 150) ? 10240 : 5120;
		else
			n = 6144;
		break;

	case 88200:
		n = hdmi_compute_n(44100, pixel_clk, ratio) * 2;
		break;

	case 96000:
		n = hdmi_compute_n(48000, pixel_clk, ratio) * 2;
		break;

	case 176400:
		n = hdmi_compute_n(44100, pixel_clk, ratio) * 4;
		break;

	case 192000:
		n = hdmi_compute_n(48000, pixel_clk, ratio) * 4;
		break;

	default:
		break;
	}

	return n;
}

static unsigned int hdmi_compute_cts(unsigned int freq, unsigned long pixel_clk,
				     unsigned int ratio)
{
	unsigned int cts = 0;
	switch (freq) {
	case 32000:
		if (pixel_clk == 297000000) {
			cts = 222750;
			break;
		} else if (pixel_clk == 25174000) {
			cts = 28125;
			break;
		}
	case 48000:
	case 96000:
	case 192000:
		switch (pixel_clk) {
		case 25200000:
		case 27000000:
		case 54000000:
		case 74250000:
		case 148500000:
			cts = pixel_clk / 1000;
			break;
		case 297000000:
			cts = 247500;
			break;
		case 25174000:
			cts = 28125l;
			break;
		/*
		 * All other TMDS clocks are not supported by
		 * DWC_hdmi_tx. The TMDS clocks divided or
		 * multiplied by 1,001 coefficients are not
		 * supported.
		 */
		default:
			break;
		}
		break;
	case 44100:
	case 88200:
	case 176400:
		switch (pixel_clk) {
		case 25200000:
			cts = 28000;
			break;
		case 25174000:
			cts = 31250;
			break;
		case 27000000:
			cts = 30000;
			break;
		case 54000000:
			cts = 60000;
			break;
		case 74250000:
			cts = 82500;
			break;
		case 148500000:
			cts = 165000;
			break;
		case 297000000:
			cts = 247500;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if (ratio == 100)
		return cts;
	else
		return (cts * ratio) / 100;
}

static void hdmi_set_clk_regenerator(void)
{
	unsigned int clk_n, clk_cts;

	clk_n = hdmi_compute_n(sample_rate, pixel_clk_rate, hdmi_ratio);
	clk_cts = hdmi_compute_cts(sample_rate, pixel_clk_rate, hdmi_ratio);

	if (clk_cts == 0) {
		pr_debug("%s: pixel clock not supported: %d\n",
			__func__, (int)pixel_clk_rate);
		return;
	}

	pr_debug("%s: samplerate=%d  ratio=%d  pixelclk=%d  N=%d  cts=%d\n",
		__func__, sample_rate, hdmi_ratio, (int)pixel_clk_rate,
		clk_n, clk_cts);

	hdmi_set_clock_regenerator_cts(clk_cts);
	hdmi_set_clock_regenerator_n(clk_n);
}

static int hdmi_core_get_of_property(struct vmm_device *dev)
{
	int err;
	u32 ipu_id, disp_id;

	err = of_property_read_u32(dev->of_node, "ipu_id", &ipu_id);
	if (err) {
		dev_dbg(dev, "get of property ipu_id fail\n");
		return err;
	}
	err = of_property_read_u32(dev->of_node, "disp_id", &disp_id);
	if (err) {
		dev_dbg(dev, "get of property disp_id fail\n");
		return err;
	}

	mxc_hdmi_ipu_id = (int)ipu_id;
	mxc_hdmi_disp_id = (int)disp_id;

	return err;
}

/* Need to run this before phy is enabled the first time to prevent
 * overflow condition in HDMI_IH_FC_STAT2 */
void hdmi_init_clk_regenerator(void)
{
	if (pixel_clk_rate == 0) {
		pixel_clk_rate = 74250000;
		hdmi_set_clk_regenerator();
	}
}
EXPORT_SYMBOL(hdmi_init_clk_regenerator);

void hdmi_clk_regenerator_update_pixel_clock(u32 pixclock)
{

	/* Translate pixel clock in ps (pico seconds) to Hz  */
	pixel_clk_rate = PICOS2KHZ(pixclock) * 1000UL;
	hdmi_set_clk_regenerator();
}
EXPORT_SYMBOL(hdmi_clk_regenerator_update_pixel_clock);

void hdmi_set_dma_mode(unsigned int dma_running)
{
	hdmi_dma_running = dma_running;
	hdmi_set_clk_regenerator();
}
EXPORT_SYMBOL(hdmi_set_dma_mode);

void hdmi_set_sample_rate(unsigned int rate)
{
	sample_rate = rate;
}
EXPORT_SYMBOL(hdmi_set_sample_rate);

void hdmi_set_edid_cfg(struct mxc_edid_cfg *cfg)
{
	unsigned long flags;

	spin_lock_irqsave(&edid_spinlock, flags);
	memcpy(&hdmi_core_edid_cfg, cfg, sizeof(struct mxc_edid_cfg));
	spin_unlock_irqrestore(&edid_spinlock, flags);
}
EXPORT_SYMBOL(hdmi_set_edid_cfg);

void hdmi_get_edid_cfg(struct mxc_edid_cfg *cfg)
{
	unsigned long flags;

	spin_lock_irqsave(&edid_spinlock, flags);
	memcpy(cfg, &hdmi_core_edid_cfg, sizeof(struct mxc_edid_cfg));
	spin_unlock_irqrestore(&edid_spinlock, flags);
}
EXPORT_SYMBOL(hdmi_get_edid_cfg);

void hdmi_set_registered(int registered)
{
	hdmi_core_init = registered;
}
EXPORT_SYMBOL(hdmi_set_registered);

int hdmi_get_registered(void)
{
	return hdmi_core_init;
}
EXPORT_SYMBOL(hdmi_get_registered);

static int mxc_hdmi_core_probe(struct vmm_device *dev,
			       const struct vmm_devtree_nodeid *nid)
{
	struct mxc_hdmi_data *hdmi_data = NULL;
	unsigned long flags;
	virtual_addr_t base_va = 0L;
	int ret = VMM_EFAIL;

	hdmi_core_init = 0;
	hdmi_dma_running = 0;

	ret = vmm_devtree_request_regmap(dev->of_node, &base_va, 0,
					 "MXC HDMI Core");
	if (ret) {
		dev_err(dev, "failed to request regmap\n");
		goto fail;
	}

	ret = hdmi_core_get_of_property(dev);
	if (ret < 0) {
		dev_err(dev, "get hdmi of property fail\n");
		goto fail;
	}

	hdmi_data = vmm_devm_zalloc(dev, sizeof(struct mxc_hdmi_data));
	if (!hdmi_data) {
		dev_err(dev, "Couldn't allocate mxc hdmi mfd device\n");
		goto fail;
	}
	hdmi_data->dev = dev;

	pixel_clk = NULL;
	sample_rate = 48000;
	pixel_clk_rate = 0;
	hdmi_ratio = 100;

	spin_lock_init(&irq_spinlock);
	spin_lock_init(&edid_spinlock);
	spin_lock_init(&hdmi_cable_state_lock);
	spin_lock_init(&hdmi_blank_state_lock);
	spin_lock_init(&hdmi_audio_lock);

	spin_lock_irqsave(&hdmi_cable_state_lock, flags);
	hdmi_cable_state = 0;
	spin_unlock_irqrestore(&hdmi_cable_state_lock, flags);

	spin_lock_irqsave(&hdmi_blank_state_lock, flags);
	hdmi_blank_state = 0;
	spin_unlock_irqrestore(&hdmi_blank_state_lock, flags);

	spin_lock_irqsave(&hdmi_audio_lock, flags);
#if 0
	hdmi_audio_stream_playback = NULL;
#endif
	hdmi_abort_state = 0;
	spin_unlock_irqrestore(&hdmi_audio_lock, flags);

	isfr_clk = devm_clk_get(dev, "hdmi_isfr");
	if (IS_ERR(isfr_clk)) {
		ret = PTR_ERR(isfr_clk);
		dev_err(dev,
			"Unable to get HDMI isfr clk: %d\n", ret);
		goto fail;
	}

	ret = clk_prepare_enable(isfr_clk);
	if (ret < 0) {
		dev_err(dev, "Cannot enable HDMI clock: %d\n", ret);
		goto eclke;
	}

	pr_debug("%s isfr_clk:%lu\n", __func__, clk_get_rate(isfr_clk));

	iahb_clk = devm_clk_get(dev, "hdmi_iahb");
	if (IS_ERR(iahb_clk)) {
		ret = PTR_ERR(iahb_clk);
		dev_err(dev,
			"Unable to get HDMI iahb clk: %d\n", ret);
		goto eclkg2;
	}

	ret = clk_prepare_enable(iahb_clk);
	if (ret < 0) {
		dev_err(dev, "Cannot enable HDMI clock: %d\n", ret);
		goto eclke2;
	}

	hdmi_data->reg_base = (void *)base_va;
	hdmi_base = hdmi_data->reg_base;


	initialize_hdmi_ih_mutes();

	/* Disable HDMI clocks until video/audio sub-drivers are initialized */
	clk_disable_unprepare(isfr_clk);
	clk_disable_unprepare(iahb_clk);

	/* Replace platform data coming in with a local struct */
	vmm_devdrv_set_data(dev, hdmi_data);

	return ret;

eclke2:
	clk_put(iahb_clk);
eclkg2:
	clk_disable_unprepare(isfr_clk);
eclke:
	clk_put(isfr_clk);
fail:
	if (hdmi_data) vmm_free(hdmi_data);
	return ret;
}


static int __exit mxc_hdmi_core_remove(struct vmm_device *dev)
{
	return VMM_OK;
}

static const struct of_device_id imx_hdmi_dt_ids[] = {
	{ .compatible = "fsl,imx6q-hdmi-core", },
	{ .compatible = "fsl,imx6dl-hdmi-core", },
	{ /* sentinel */ }
};

static struct vmm_driver mxc_hdmi_core_driver = {
	.name = "mxc_hdmi_core",
	.match_table = imx_hdmi_dt_ids,
	.probe = mxc_hdmi_core_probe,
	.remove = mxc_hdmi_core_remove,
};

static int __init mxc_hdmi_core_init(void)
{
	return vmm_devdrv_register_driver(&mxc_hdmi_core_driver);
}

static void __exit mxc_hdmi_core_exit(void)
{
	vmm_devdrv_unregister_driver(&mxc_hdmi_core_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
