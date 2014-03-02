/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "hw_lcd_common.h"

#define LCD_DEVICE_NAME "mipi_cmd_otm8009a_fwvga"

static lcd_panel_type lcd_panel_fwvga = LCD_NONE;

/* increase the DSI bit clock to 490 MHz */
static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db_otm8009a_fwvga = {
    /* DSI Bit Clock at 490 MHz, 2 lane, RGB888 */ 
	/* regulator */ 
	{0x03, 0x01, 0x01, 0x00}, 
	/* timing */ 
	{0x88, 0x32, 0x14, 0x00, 0x44, 0x4F, 0x18, 0x35, 
	0x17, 0x3, 0x04, 0x00},
	/* phy ctrl */ 
	{0x7f, 0x00, 0x00, 0x00}, 
	/* strength */ 
	{0xbb, 0x02, 0x06, 0x00}, 
	/* pll control */ 
	{0x1, 0xE3, 0x31, 0xd2, 0x00, 0x40, 0x37, 0x62, 
	0x01, 0x0f, 0x07, 
	0x05, 0x14, 0x03, 0x0, 0x0, 0x0, 0x20, 0x0, 0x02, 0x0}, 
};

static struct dsi_buf otm8009a_fwvga_tx_buf;
static struct sequence * otm8009a_lcd_init_table_debug = NULL;

static const struct sequence otm8009a_fwvga_standby_enter_table[]= 
{
	{0x00028,MIPI_DCS_COMMAND,0},
    {0x00010,MIPI_DCS_COMMAND,20},
    {0x00010,MIPI_TYPE_END,120},
};
static const struct sequence otm8009a_fwvga_standby_exit_table[]= 
{
	{0x00011,MIPI_DCS_COMMAND,0},
    {0x00029,MIPI_DCS_COMMAND,120},
	{0x00029,MIPI_TYPE_END,20},
};
/*lcd resume function*/

static int mipi_otm8009a_fwvga_lcd_on(struct platform_device *pdev)
{
	
	/*delete some lines */
	boolean para_debug_flag = FALSE;
	uint32 para_num = 0;
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL; 

	/*delete some lines */
	para_debug_flag = lcd_debug_malloc_get_para( "otm8009a_lcd_init_table_debug", 
            (void**)&otm8009a_lcd_init_table_debug,&para_num);

    if( (TRUE == para_debug_flag) && (NULL != otm8009a_lcd_init_table_debug))
    {
        process_mipi_table(mfd,&otm8009a_fwvga_tx_buf,otm8009a_lcd_init_table_debug,
		     para_num, lcd_panel_fwvga);
    }
	else
	{
        /* low power mode*/
        mipi_set_tx_power_mode(1);
		process_mipi_table(mfd,&otm8009a_fwvga_tx_buf,(struct sequence*)&otm8009a_fwvga_standby_exit_table,
		 	ARRAY_SIZE(otm8009a_fwvga_standby_exit_table), lcd_panel_fwvga);
		mipi_set_tx_power_mode(0);
		/*delete some lines */
	}

	if((TRUE == para_debug_flag)&&(NULL != otm8009a_lcd_init_table_debug))
	{
		lcd_debug_free_para((void *)otm8009a_lcd_init_table_debug);
	}
	
	LCD_DEBUG("leave mipi_otm8009a_fwvga_lcd_on \n");
	return 0;
}
/*lcd suspend function*/
static int mipi_otm8009a_fwvga_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	process_mipi_table(mfd,&otm8009a_fwvga_tx_buf,(struct sequence*)&otm8009a_fwvga_standby_enter_table,
		 ARRAY_SIZE(otm8009a_fwvga_standby_enter_table), lcd_panel_fwvga);
	LCD_DEBUG("leave mipi_otm8009a_fwvga_lcd_off \n");
	return 0;
}

#ifdef CONFIG_FB_AUTO_CABC
static struct sequence otm8009a_fwvga_auto_cabc_set_table[] =
{	
	{0x00053,MIPI_DCS_COMMAND,0},
	{0x00024,TYPE_PARAMETER,0},

    {0x00055,MIPI_DCS_COMMAND,0}, 
	{0x00001,TYPE_PARAMETER,0},

	{0xFFFFF,MIPI_TYPE_END,0}, /* the end flag,it don't sent to driver IC */
};
/***************************************************************
Function: otm8009a_fwvga_config_auto_cabc
Description: Set CABC configuration
Parameters:
	struct msmfb_cabc_config cabc_cfg: CABC configuration struct
Return:
	0: success
***************************************************************/
static int otm8009a_fwvga_config_auto_cabc(struct msmfb_cabc_config cabc_cfg, struct msm_fb_data_type *mfd)
{
	int ret = 0;

	switch(cabc_cfg.mode)
	{
		case CABC_MODE_UI:
			otm8009a_fwvga_auto_cabc_set_table[1].reg=0x00024;
			otm8009a_fwvga_auto_cabc_set_table[3].reg=0x00001;
			break;
		case CABC_MODE_MOVING:
		case CABC_MODE_STILL:
			otm8009a_fwvga_auto_cabc_set_table[1].reg=0x0002C;
			otm8009a_fwvga_auto_cabc_set_table[3].reg=0x00003;
			break;
		default:
			LCD_DEBUG("%s: invalid cabc mode: %d\n", __func__, cabc_cfg.mode);
	        ret = -EINVAL;
			break;
	}
	if(likely(0 == ret))
	{
		process_mipi_table(mfd,&otm8009a_fwvga_tx_buf,(struct sequence*)&otm8009a_fwvga_auto_cabc_set_table,
			                ARRAY_SIZE(otm8009a_fwvga_auto_cabc_set_table), lcd_panel_fwvga);
	}

	LCD_DEBUG("%s: change cabc mode to %d\n",__func__,cabc_cfg.mode);
	return ret;
}
#endif /* CONFIG_FB_AUTO_CABC */

static int __devinit mipi_otm8009a_fwvga_lcd_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);

	return 0;
}

static struct sequence otm8009a_fwvga_cabc_enable_table[] =
{	
	{0x00051,MIPI_DCS_COMMAND,0}, 		
	{0x000ff,TYPE_PARAMETER,0},
	
	{0x00029,MIPI_TYPE_END,0},
};
void otm8009a_fwvga_set_cabc_backlight(struct msm_fb_data_type *mfd,uint32 bl_level)
{	
	otm8009a_fwvga_cabc_enable_table[1].reg = bl_level; // 1 will be changed if modify init code

	process_mipi_table(mfd,&otm8009a_fwvga_tx_buf,(struct sequence*)&otm8009a_fwvga_cabc_enable_table,
		 ARRAY_SIZE(otm8009a_fwvga_cabc_enable_table), lcd_panel_fwvga);
}

static struct platform_driver this_driver = {
	.probe  = mipi_otm8009a_fwvga_lcd_probe,
	.driver = {
		.name   = LCD_DEVICE_NAME,
	},
};
static struct msm_fb_panel_data otm8009a_fwvga_panel_data = {
	.on					= mipi_otm8009a_fwvga_lcd_on,
	.off					= mipi_otm8009a_fwvga_lcd_off,
	.set_backlight 		= pwm_set_backlight,
	/*add cabc control backlight*/
	.set_cabc_brightness 	= otm8009a_fwvga_set_cabc_backlight,
	#ifdef CONFIG_FB_AUTO_CABC
	.config_cabc = otm8009a_fwvga_config_auto_cabc,
    #endif
};
static struct platform_device this_device = {
	.name   = LCD_DEVICE_NAME,
	.id	= 0,
	.dev	= {
		.platform_data = &otm8009a_fwvga_panel_data,
	}
};
static int __init mipi_cmd_otm8009a_fwvga_init(void)
{
	int ret = 0;
	struct msm_panel_info *pinfo = NULL;
	
	lcd_panel_fwvga = get_lcd_panel_type();
	if (MIPI_CMD_OTM8009A_CHIMEI_FWVGA != lcd_panel_fwvga)
	{
		return 0;
	}
	LCD_DEBUG("enter mipi_cmd_otm8009a_fwvga_init \n");
	mipi_dsi_buf_alloc(&otm8009a_fwvga_tx_buf, DSI_BUF_SIZE);

	ret = platform_driver_register(&this_driver);
	if (!ret)
	{
	 	pinfo = &otm8009a_fwvga_panel_data.panel_info;
		pinfo->xres = 480;
		pinfo->yres = 854;
		pinfo->type = MIPI_CMD_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 24;		
		pinfo->bl_max = 255;
		pinfo->bl_min = 30;		
		pinfo->fb_num = 3;
        /* increase the DSI bit clock to 490 MHz */
        pinfo->clk_rate = 490000000;
		pinfo->lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */

		pinfo->mipi.mode = DSI_CMD_MODE;
		pinfo->mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
		pinfo->mipi.vc = 0;
		pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
		pinfo->mipi.data_lane0 = TRUE;
		pinfo->mipi.data_lane1 = TRUE;
		pinfo->mipi.t_clk_post = 0x7f;
		pinfo->mipi.t_clk_pre = 0x2f;
		pinfo->mipi.stream = 0; /* dma_p */
		pinfo->mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
		pinfo->mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
		/*set hw te sync*/
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->mipi.te_sel = 1; /* TE from vsync gpio */
		pinfo->mipi.interleave_max = 1;
		pinfo->mipi.insert_dcs_cmd = TRUE;
		pinfo->mipi.wr_mem_continue = 0x3c;
		pinfo->mipi.wr_mem_start = 0x2c;
		pinfo->mipi.dsi_phy_db = &dsi_cmd_mode_phy_db_otm8009a_fwvga;
		pinfo->mipi.tx_eot_append = 0x01;
		pinfo->mipi.rx_eot_ignore = 0;
		pinfo->mipi.dlane_swap = 0x1;

		ret = platform_device_register(&this_device);
		if (ret)
			LCD_DEBUG("%s: failed to register device!\n", __func__);
	}
	return ret;
}

module_init(mipi_cmd_otm8009a_fwvga_init);
