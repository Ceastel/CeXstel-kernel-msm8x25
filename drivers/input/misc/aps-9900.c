/* drivers/input/misc/aps-12d.c
 *
 * Copyright (C) 2010 HUAWEI, Inc.
 * Author: Benjamin Gao <gaohuajiang@huawei.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hardware_self_adapt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include "aps-9900.h"
#include <asm/mach-types.h>
#include <linux/hardware_self_adapt.h>
#include <mach/vreg.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/light.h>
#include <linux/sensors.h>
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef PROXIMITY_DB
#ifdef PROXIMITY_DB
#define PROXIMITY_DEBUG(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define PROXIMITY_DEBUG(fmt, args...)
#endif

static struct workqueue_struct *aps_wq;

struct aps_data {
    uint16_t addr;
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct mutex  mlock;
    struct hrtimer timer;
    struct work_struct  work;
	/* delete user_irq */
    int (*power)(int on);
};

/*code unitary*/
/*lsensor is for all the product,different phone has different table in .h */
static uint16_t lsensor_adc_table[LSENSOR_MAX_LEVEL] = {
	8, 25, 110, 400, 750, 1200, 3000
};

/* add the macro of log */
static int aps9900_debug_mask;
module_param_named(aps9900_debug, aps9900_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define APS9900_DBG(x...) do {\
    if (aps9900_debug_mask) \
        printk(KERN_DEBUG x);\
    } while (0)

struct aps_init_regdata{
      int reg;
      uint8_t data;
};
static struct aps_data  *this_aps_data;

static int p_h = 0x3c0;
static int p_l = 0x3bf;
struct input_dev *sensor_9900_dev=NULL;
static int aps_9900_delay = 1000;     /*1s*/
/* delete this part */
static int aps_first_read = 1;
/* use this to make sure which device is open and make a wake lcok*/
static int aps_open_flag=0;
/* decline the default min_proximity_value */
static int origin_prox = 822;
static int min_proximity_value;
static int light_device_minor = 0;
static int proximity_device_minor = 0;
/*return the modify*/
static struct wake_lock proximity_wake_lock;
static atomic_t l_flag;
static atomic_t p_flag;
/* to be same with ICS, move the values up */
static int proximity_data_value = 0;
static int light_data_value = 0;

/*init the value of reg 0*/
static u8  reg0_value = 0x38; 

static char light_device_id[] = "AVAGO-TAOS-9900";

/* an arithmometer to mark how much times the aps device opened*/
static int open_count = 0;
/*modify the value*/
static int apds_9900_pwindows_value = 200;
static int apds_9900_pwave_value = 100; 

static struct aps_init_regdata aps9900_init_regdata[]=
{
    {APDS9900_ENABLE_REG, 0x0},
    {APDS9900_ATIME_REG,   0xdb},
    {APDS9900_PTIME_REG,   0xff},
    /*modify the value*/
    {APDS9900_WTIME_REG,  0xb6},
    /* modify the ppcount from 8 to 4 */
    {APDS9900_PPCOUNT_REG, 0x04},
    {APDS9900_CONTROL_REG, 0x60},
    {APDS9900_ENABLE_REG, 0x38},
    {APDS9900_PERS_REG, 0x12}
};

/* Coefficients in open air: 
  * GA:Glass (or Lens) Attenuation Factor
  * DF:Device Factor
  * alsGain: ALS Gain
  * aTime: ALS Timing
  * ALSIT = 2.72ms * (256 �C ATIME) = 2.72ms * (256-0xDB) =  100ms
  */
static int aTime = 0xDB; 
static int alsGain = 1;
/* modify the parameter by FAE */
/*
static int ga=48;
static int coe_b=223;
static int coe_c=7;
static int coe_d=142;
*/
/* modify the parameter */
static int ga=515;
static int coe_b=1948;
static int coe_c=613;
static int coe_d=1163;
static int DF=52;

static int  set_9900_register(struct aps_data  *aps, u8 reg, u16 value, int flag)
{
    int ret;

    mutex_lock(&aps->mlock);
    /*  flag=1 means reading the world value */
    if (flag)
    {
        ret = i2c_smbus_write_word_data(aps->client, CMD_WORD | reg, value);
    }
    /*  flag=0 means reading the byte value */
    else
    {
        ret = i2c_smbus_write_byte_data(aps->client, CMD_BYTE | reg, (u8)value);
    }

    mutex_unlock(&aps->mlock);
	/* delete some lines */

    return ret;
}
static int get_9900_register(struct aps_data  *aps, u8 reg, int flag)
{
    int ret;

    mutex_lock(&aps->mlock);
    if (flag)
    {
        ret = i2c_smbus_read_word_data(aps->client, CMD_WORD | reg);
    }
    else
    {
        ret = i2c_smbus_read_byte_data(aps->client, CMD_BYTE | reg);
    }
    mutex_unlock(&aps->mlock);
    if (ret < 0)
    {
        printk(KERN_ERR "%s, line %d: read register fail!(reg=0x%x, flag=%d, ret=0x%x)", __func__, __LINE__, reg, flag, ret);
    }
    return ret;
}

static int aps_9900_open(struct inode *inode, struct file *file)
{ 
    int ret = 0;
    /* when the device is open use this if light open report -1 when proximity open then lock it*/
    if( light_device_minor == iminor(inode) ){
        PROXIMITY_DEBUG("%s:light sensor open\n", __func__);
        /*it's not necessary to set this flag everytime*/
        /*aps_first_read = 1;*/
    }
    if( proximity_device_minor == iminor(inode) ){
        PROXIMITY_DEBUG("%s:proximity_device_minor == iminor(inode)\n", __func__);
        wake_lock( &proximity_wake_lock);
        /* 0 is close, 1 is far */
        input_report_abs(this_aps_data->input_dev, ABS_DISTANCE, 1);
        input_sync(this_aps_data->input_dev);
    }
    /* when open_count come to max, the aps device reset the value of min_proximity_value*/
    if( OPEN_COUNT_MAX == open_count )
    {
        min_proximity_value = origin_prox;
        open_count = 0;
    }
    open_count ++;
    APS9900_DBG(KERN_ERR "%s:flag is %d,open_count = %d\n", __func__,aps_open_flag,open_count);
    if(p_h != get_9900_register(this_aps_data, APDS9900_PIHTL_REG, 1) \
     ||p_l != get_9900_register(this_aps_data, APDS9900_PILTL_REG, 1) )
    {
        ret  = set_9900_register(this_aps_data, APDS9900_PILTL_REG, p_l, 1);
        ret |= set_9900_register(this_aps_data, APDS9900_PIHTL_REG, p_h, 1);
        if (ret)
        {
            printk(KERN_ERR "%s:set_9900_register is error(%d)!", __func__, ret);
        }
        printk("%s:reset PH and PL\n!",__func__);
    }
    if (!aps_open_flag)
    {
        u8 value_reg0;
        int ret;
        
        /*set bit 0 of reg 0 ==1*/
        value_reg0 = reg0_value;
        /* if power on ,will not set PON=1 again */
        if (APDS9900_POWER_OFF == (value_reg0 & APDS9900_POWER_MASK))
        {
            value_reg0 = value_reg0 | APDS9900_POWER_ON;
            ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0 , 0);
            if (ret)
            {
                printk(KERN_ERR "%s:set_9900_register is error(%d)!", __func__, ret);
            }
            else
            {
                reg0_value = value_reg0;
            }
        }
        if (this_aps_data->client->irq)   
        {
            enable_irq(this_aps_data->client->irq);
        }
    }
    aps_open_flag++;
	return nonseekable_open(inode, file);
}

static int aps_9900_release(struct inode *inode, struct file *file)
{
    int ret;
    aps_open_flag--;
    aps_9900_delay = 1000;//1s
    if(p_h != get_9900_register(this_aps_data, APDS9900_PIHTL_REG, 1) \
     ||p_l != get_9900_register(this_aps_data, APDS9900_PILTL_REG, 1) )
    {
        ret  = set_9900_register(this_aps_data, APDS9900_PILTL_REG, p_l, 1);
        ret |= set_9900_register(this_aps_data, APDS9900_PIHTL_REG, p_h, 1);
        if (ret)
        {
            printk(KERN_ERR "%s:set_9900_register is error(%d)!", __func__, ret);
        }
        printk("%s:reset PH and PL\n!",__func__);
    }
    /*when proximity is released then unlock it*/
    if( proximity_device_minor == iminor(inode) ){
        PROXIMITY_DEBUG("%s: proximity_device_minor == iminor(inode)\n", __func__);
        wake_unlock( &proximity_wake_lock);
    }
    if( light_device_minor == iminor(inode) ){
        PROXIMITY_DEBUG("%s: light_device_minor == iminor(inode)\n", __func__);
    }
    if (!aps_open_flag)
    {
        int value_reg0,ret;
        
        /*set bit 0 of reg 0 ==0*/
        value_reg0 = reg0_value & APDS9900_REG0_POWER_OFF;
        ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0, 0);
        if (ret)
        {
            printk(KERN_ERR "%s:set_9900_register is error(%d)!", __func__, ret);
        }
        else
        {
            reg0_value = value_reg0;
        }
        if (this_aps_data->client->irq) 
        {
            disable_irq(this_aps_data->client->irq);
        }
    }
    return 0;
}

static long
aps_9900_ioctl(struct file *file, unsigned int cmd,
     unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int flag;
    int value_reg0;
    int set_flag;
    int ret;
	/*delete one line */
    switch (cmd) 
    {
        case ECS_IOCTL_APP_SET_LFLAG:   /* app set  light sensor flag */
        {
            if (copy_from_user(&flag, argp, sizeof(flag)))
            {
                return -EFAULT;
            }
            atomic_set(&l_flag, flag);
            set_flag = atomic_read(&l_flag) ? 1 : 0;

			/*set bit 1 of reg 0 by set_flag */
            /* set AEN,if l_flag=1 then enable ALS.if l_flag=0 then disable ALS */
			if (set_flag)
			{
				value_reg0 = reg0_value | (set_flag << APDS9900_AEN_BIT_SHIFT);
           		ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0, 0);
			}
			else
			{
				value_reg0 = reg0_value & APDS9900_REG0_AEN_OFF;
           		ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0, 0);
			}
            if (ret)
            {
                printk(KERN_ERR "%s:set ECS_IOCTL_APP_SET_LFLAG flag is error(%d)!", __func__, ret);
            }
			else
			{
				reg0_value = value_reg0;
			}
            break;
        }
        case ECS_IOCTL_APP_GET_LFLAG:  /*app  get open light sensor flag*/
        {
            flag = atomic_read(&l_flag);
            if (copy_to_user(argp, &flag, sizeof(flag)))
            {
                return -EFAULT;
            }
            break;
        }
        case ECS_IOCTL_APP_SET_PFLAG:
        {
            if (copy_from_user(&flag, argp, sizeof(flag)))
            {
                return -EFAULT;
            }
            atomic_set(&p_flag, flag);
            set_flag = atomic_read(&p_flag) ? 1 : 0;
			/*set bit 1 of reg 0 by set_flag */
            /* set PEN,if p_flag=1 then enable proximity.if p_flag=0 then disable proximity */
			if (set_flag)
			{
				value_reg0 = reg0_value | (set_flag << APDS9900_PEN_BIT_SHIFT);
          		ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0, 0);
			}
			else
			{
				value_reg0 = reg0_value & APDS9900_REG0_PEN_OFF;
           		ret = set_9900_register(this_aps_data, APDS9900_ENABLE_REG, value_reg0, 0);
			}
            if (ret)
            {
                printk(KERN_ERR "%s:set ECS_IOCTL_APP_SET_PFLAG flag is error(%d)!", __func__, ret);
            }	
			else
			{
				reg0_value = value_reg0;
			}
            break;
        }
        case ECS_IOCTL_APP_GET_PFLAG:  /*get open acceleration sensor flag*/
        {
            flag = atomic_read(&p_flag);
            if (copy_to_user(argp, &flag, sizeof(flag)))
            {
                return -EFAULT;
            }
            break;
        }
        case ECS_IOCTL_APP_SET_DELAY:
        {
            if (copy_from_user(&flag, argp, sizeof(flag)))
            {
                return -EFAULT;
            }
            if(flag)
                aps_9900_delay = flag;
            else
                aps_9900_delay = 20;   /*20ms*/
            break;
        }
        case ECS_IOCTL_APP_GET_DELAY:
        {
            flag = aps_9900_delay;
            if (copy_to_user(argp, &flag, sizeof(flag)))
            {
                return -EFAULT;
            }
            break;
        }
		/*get value of proximity and light*/
		case ECS_IOCTL_APP_GET_PDATA_VALVE:
        {
            flag = proximity_data_value;
            if (copy_to_user(argp, &flag, sizeof(flag)))
            {
                return -EFAULT;
            }
            break;
        }
		case ECS_IOCTL_APP_GET_LDATA_VALVE:
        {
            flag = light_data_value;
            if (copy_to_user(argp, &flag, sizeof(flag)))
            {
                return -EFAULT;
            }
            break;
        }
		case ECS_IOCTL_APP_GET_APSID:
        {
            if (copy_to_user(argp, light_device_id, strlen(light_device_id)+1))
                return -EFAULT;
            break;
        }
        default:
        {
            break;
        }
		
    }
    return 0;
}

static struct file_operations aps_9900_fops = {
    .owner = THIS_MODULE,
    .open = aps_9900_open,
    .release = aps_9900_release,
    .unlocked_ioctl = aps_9900_ioctl,
};

static struct miscdevice light_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "light",
    .fops = &aps_9900_fops,
};

static struct miscdevice proximity_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "proximity",
    .fops = &aps_9900_fops,
};

static int luxcalculation(int cdata, int irdata)
{
    int luxValue=0;
	int first_half, sec_half;
    int iac1=0;
    int iac2=0;
    int iac=0;

    /*
     *Lux Equation:
     *       IAC1 = CH0DATA -B * CH1DATA                (IAC:IR Adjusted Count)
     *       IAC2 = C * CH0DATA - D * CH1DATA
     *       IAC = Max (IAC1, IAC2, 0)
     *       LPC = GA * DF / (ALSIT * AGAIN)               (LPC:Lux per Count)
     *       Lux = IAC * LPC
     *Coefficients in open air:
     *       GA = 0.48
     *       B = 2.23
     *       C = 0.7
     *       D = 1.42
    */
	/*adjust data by parameter*/
    iac1 = (int) (cdata - (coe_b*irdata/1000));
    iac2 = (int) ((coe_c*cdata/1000) - (coe_d*irdata/1000));
    
    if (iac1 > iac2)
    {
        iac = iac1;
    }
    else if (iac1 <= iac2)
    {
        iac = iac2;
    }
    else
    {
        iac = 0;
    }
	
    first_half = iac*ga*DF/100;
    sec_half = ((272*(256-aTime))*alsGain)/100;
    luxValue = first_half/sec_half;
	 APS9900_DBG("first_half====%d  ,sec_half ===%d,iac===%d\n", first_half,sec_half,iac);
    return luxValue;
}

static void aps_9900_work_func(struct work_struct *work)
{
    int pdata = 0;/* proximity data*/
    /* change irdate to static, cdate to dynamic */
    int cdata = 0;/* ch0 data  */
    int irdata = 0;/* ch1 data  */
    int cdata_high = 0, cdata_low = 0;
    int lux; 
    int status;
    int ret;
    int als_level = 0;
    int i = 0;
    struct aps_data *aps = container_of(work, struct aps_data, work);

    status = get_9900_register(aps,APDS9900_STATUS_REG,0);
    /* proximity flag is open and the interrupt belongs to proximity */
    if (atomic_read(&p_flag) && (status & APDS9900_STATUS_PROXIMITY_BIT))
    {
        int pthreshold_h=0, pthreshold_l;
        /* read the proximity data  */
        APS9900_DBG("Into prox init! \n");
        pdata = get_9900_register(aps, APDS9900_PDATAL_REG, 1);
        /* protect the proximity fuction when the sunlight is very strong */
        if( pdata < 0 )
        {
            /* the number "200" is a value to make sure there is a valid value */
            pdata = 200 ;
            printk(KERN_ERR "%s, line %d: pdate<0, reset to %d\n", __func__, __LINE__, pdata);
        }
        /* delete this part ,because modify sunlight problem bring a new problem,so we should delete it*/
        /* add the arithmetic of setting the proximity thresholds automatically */

        if ((pdata + apds_9900_pwave_value) < min_proximity_value)
        {
            min_proximity_value = pdata + apds_9900_pwave_value;
            ret = set_9900_register(aps, APDS9900_PILTL_REG, min_proximity_value, 1);
            ret |= set_9900_register(aps, APDS9900_PIHTL_REG, (min_proximity_value + apds_9900_pwindows_value), 1);
            if (ret)
            {
                printk(KERN_ERR "%s, line %d: set APDS9900_PILTL_REG register is error(min=%d, window=%d, ret=%d)\n", \
                       __func__, __LINE__, min_proximity_value, apds_9900_pwindows_value, ret);
            }
            APS9900_DBG("%s:min_proximity_value=%d\n", __func__, min_proximity_value);
        }
        pthreshold_h = get_9900_register(aps, APDS9900_PIHTL_REG, 1);
        pthreshold_l = get_9900_register(aps, APDS9900_PILTL_REG, 1);
        /* add some logs */
        APS9900_DBG("%s:pdata=%d pthreshold_h=%d pthreshold_l=%d\n", __func__, pdata, pthreshold_h, pthreshold_l);
        /* clear proximity interrupt bit */
        ret = set_9900_register(aps, 0x65, 0, 0);
        if (ret)
        {
            printk(KERN_ERR "%s, line %d: set_9900_register is error(%d),clear failed!", __func__, __LINE__, ret);
        }
		/*get value of proximity*/
         proximity_data_value = pdata;

        /* if more than the value of  proximity high threshold we set*/

        if (pdata >= pthreshold_h) 
		{
            ret = set_9900_register(aps, APDS9900_PILTL_REG, min_proximity_value, 1);
            if (ret)
            {
                printk(KERN_ERR "%s, line %d: set APDS9900_PILTL_REG register is error(min=%d, ret=%d)!", __func__, __LINE__, min_proximity_value, ret);
            }
            input_report_abs(aps->input_dev, ABS_DISTANCE, 0);
            input_sync(aps->input_dev);
        }
        /* if less than the value of  proximity low threshold we set*/
        /* the condition of pdata==pthreshold_l is valid */
        else if (pdata <=  pthreshold_l)
        {
            ret = set_9900_register(aps, APDS9900_PILTL_REG, 0, 1);
            if (ret)
            {
                printk(KERN_ERR "%s, line %d: set APDS9900_PILTL_REGs register is error(%d)!", __func__, __LINE__, ret);
            }
            input_report_abs(aps->input_dev, ABS_DISTANCE, 1);
            input_sync(aps->input_dev);
        }
        /*on 27a platform ,bug info is a lot*/
        else
        {
            printk(KERN_ERR "%s, line %d: Wrong status!\n",  __func__, __LINE__);
            ret = set_9900_register(aps, APDS9900_PILTL_REG, min_proximity_value, 1);
            if (ret)
            {
                printk(KERN_ERR "%s, line %d: set APDS9900_PILTL_REG register is error(%d)!", __func__, __LINE__, ret);
            }
        }
        pthreshold_h = get_9900_register(aps, APDS9900_PIHTL_REG, 1);
        pthreshold_l = get_9900_register(aps, APDS9900_PILTL_REG, 1);
        p_h = pthreshold_h;
        p_l = pthreshold_l;
        APS9900_DBG("%s:min = %d,apds_9900 = %d\n",__func__,min_proximity_value,apds_9900_pwindows_value);
        APS9900_DBG("%s:after reset the pdata=%d pthreshold_h=%d pthreshold_l=%d\n", __func__, pdata, pthreshold_h, pthreshold_l);
    }
    /* p_flag is close, and no proximity interrupt: normal, just add for debug */ 
    else if ( !atomic_read(&p_flag) && !(status & APDS9900_STATUS_PROXIMITY_BIT) )
    {
        APS9900_DBG("%s, line %d: [APS_OK]p_flag is close and no prox interrupt(status=0x%x).\n", __func__, __LINE__, status);
    }
    /* p_flag is open, but no proximity interrupt: show registers value */
    else if (atomic_read(&p_flag) && !(status & APDS9900_STATUS_PROXIMITY_BIT) )
    {
        int pthreshold_h = 0;
        int pthreshold_l = 0;
        
        /* get pdata, p_h, p_l value from registers */
        pdata        = get_9900_register(aps, APDS9900_PDATAL_REG, 1);
        pthreshold_h = get_9900_register(aps, APDS9900_PIHTL_REG, 1);
        pthreshold_l = get_9900_register(aps, APDS9900_PILTL_REG, 1);
        
        /* normal */
        if ( (0 == pthreshold_l && pdata < pthreshold_h)       /* near, but less than pthreshold_h  */
            || (pthreshold_l > 0 && pdata > pthreshold_l) )    /* far, but bigger than pthreshold_l */
        {
            APS9900_DBG("%s, line %d: [APS_OK]p_flag is open, but no prox int(STATUS=0x%x,ENABLE=0x%x,PDATA=%d,PILT=%d,PIHT=%d)\n", \
                        __func__, __LINE__, status, get_9900_register(aps,APDS9900_ENABLE_REG,0), pdata, pthreshold_l, pthreshold_h);
        }
        /* abnormal */
        else
        {
            int reg_enable = get_9900_register(aps,APDS9900_ENABLE_REG,0);
            printk(KERN_ERR "%s, line %d: [APS_ERR]p_flag is open, but no prox int(STATUS=0x%x,ENABLE=0x%x,PDATA=%d,PILT=%d,PIHT=%d)\n", \
                   __func__, __LINE__, status, reg_enable, pdata, pthreshold_l, pthreshold_h);
        }
    }
    /* p_flag is close, but raise proximity interrupt: abnormal, clear the prox interrupt bit */ 
    else if ( !atomic_read(&p_flag) && (status & APDS9900_STATUS_PROXIMITY_BIT) )
    {
        /* clear proximity interrupt bit */
        ret = set_9900_register(aps, 0x65, 0, 0);
        if (ret)
        {
            printk(KERN_ERR "%s, line %d: clear proximity interrupt bit failed(%d)!\n", __func__, __LINE__, ret);
        }
        else
        {
            printk(KERN_ERR "%s, line %d: p_flag is close, but raise prox interrupt, clear prox interrupt bit.\n", __func__, __LINE__);
        }
    }
    /* ALS flag is open and the interrupt belongs to ALS */
    if (atomic_read(&l_flag) && (status & APDS9900_STATUS_ALS_BIT)) 
    {
        /* read the CH0 data and CH1 data  */
        APS9900_DBG("into light_init!!\n");
        cdata = get_9900_register(aps, APDS9900_CDATAL_REG, 1);
        irdata = get_9900_register(aps, APDS9900_IRDATAL_REG, 1);
        /* set ALS high threshold = ch0(cdata) + 20%,low threshold = ch0(cdata) - 20% */
        cdata_high = (cdata *  600)/500;
        cdata_low = (cdata *  400)/500;
        /* the max value of cdata_high == 0xffff */
        if(0xffff <= cdata_high )
        {
            cdata_high=0xffff;
            printk(KERN_ERR "%s, line %d: 0xffff <= cdata_high, reset to 0x%x!\n",  __func__, __LINE__, cdata_high);
        }
        /* clear als interrupt bit */
        ret = set_9900_register(aps, 0x66,0, 0);
        ret |= set_9900_register(aps, APDS9900_AILTL_REG, cdata_low, 1);
        ret |= set_9900_register(aps, APDS9900_AIHTL_REG, cdata_high, 1);
        if (ret)
        {
            printk(KERN_ERR "%s, line %d: set APDS9900_AILTL_REG register is error(cdata_low=%d,cdata_high=%d,ret=%d)!", __func__, __LINE__, cdata_low, cdata_high, ret);
        }
        /* convert the raw pdata and irdata to the value in units of lux */
        lux = luxcalculation(cdata, irdata);
        APS9900_DBG("%s:cdata=%d irdata=%d lux=%d\n",__func__, cdata, irdata, lux);
        if (lux >= 0) 
        {
            /*get value of light*/
             light_data_value = lux;
            /* lux=0 is valid */
            als_level = LSENSOR_MAX_LEVEL - 1;
            for (i = 0; i < ARRAY_SIZE(lsensor_adc_table); i++)
            {
                if (lux < lsensor_adc_table[i])
                {
                    als_level = i;
                    break;
                }
            }
            APS9900_DBG("%s:cdata=%d irdata=%d lux=%d,als_level==%d\n",__func__, cdata, irdata, lux,als_level);

            if(aps_first_read)
            {
                aps_first_read = 0;
                input_report_abs(aps->input_dev, ABS_LIGHT, -1);
                input_sync(aps->input_dev);
            }
            else
            {
                input_report_abs(aps->input_dev, ABS_LIGHT, als_level);
                input_sync(aps->input_dev);
            }
        }
        /* if lux<0,we need to change the gain which we can set register 0x0f */
        else {
                printk(KERN_ERR "%s, line %d: Need to change gain(lux=%2d)\n", __func__, __LINE__, lux);
        }
    }   
    /* l_flag is close, but raise als interrupt: abnormal, clear the als interrupt bit */
    else if ((status & APDS9900_STATUS_ALS_BIT) && (!atomic_read(&l_flag)))
    {
        /* clear als interrupt bit */
        ret = set_9900_register(aps, 0x66,0, 0);
        if (ret)
        {
            printk(KERN_ERR "%s, line %d: clear als interrupt bit failed(%d)!", __func__, __LINE__, ret);
        }
        else
        {
            printk(KERN_ERR "%s, line %d: l_flag is close, but raise als interrupt, clear als interrupt bit.\n", __func__, __LINE__);
        }
    }

    /* delete the condition */
    if (aps->client->irq)
    {
        enable_irq(aps->client->irq);
    }
}
	/* delete some lines */

static inline int aps_i2c_reg_init(struct aps_data *aps)
{
    int ret;
    int i;
    int revid;
    int id;

    for (i=0; i< ARRAY_SIZE(aps9900_init_regdata);i++)
    {
        ret = set_9900_register(aps, CMD_BYTE|aps9900_init_regdata[i].reg, aps9900_init_regdata[i].data, 0);
        if (ret < 0)
        {
            printk(KERN_ERR "Ret of init aps9900 regs(%d - %d) is %d\n", aps9900_init_regdata[i].reg, aps9900_init_regdata[i].data, get_9900_register(aps, aps9900_init_regdata[i].reg, 0));
            break;
        }
    }
    /* ID check ,if not equal, return -1 */
    revid = get_9900_register(aps, APDS9900_REV_REG, 0);
    id = get_9900_register(aps, APDS9900_ID_REG, 0);
    if ((APDS_9900_REV_ID != revid)  || (APDS_9900_ID != id)) 
    {
        printk(KERN_ERR "%s:The ID of checking failed!(revid=%.2x id=%.2x)", __func__, revid, id);
        ret = -1;
    }

    return ret;
}

irqreturn_t aps_irq_handler(int irq, void *dev_id)
{
    struct aps_data *aps = dev_id;

    disable_irq_nosync(aps->client->irq);
    queue_work(aps_wq, &aps->work);

    return IRQ_HANDLED;
}

/* delete aps_timer_func function */
static int aps_9900_probe(
    struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    struct aps_data *aps;
	uint16_t *p = &lsensor_adc_table[0];
	int i = 0;
    struct aps9900_hw_platform_data *platform_data = NULL;

    if (client->dev.platform_data == NULL)
    {
        pr_err("%s:platform data is NULL. exiting.\n", __func__);
        ret = -ENODEV;
        goto err_exit;
    }
	/*add all GP's embranchment */
    if(machine_is_msm7x27a_U8655() || machine_is_msm7x27a_U8655_EMMC())
    {
        apds_9900_pwindows_value = U8655_WINDOW;
        apds_9900_pwave_value = U8655_WAVE;
        p = &lsensor_adc_table_u8655[0];
    }
    else if(machine_is_msm7x27a_U8815())
    {
        /*merge 8815's parameters to TA and main branch*/
        apds_9900_pwindows_value = U8815_WINDOW;
        apds_9900_pwave_value = U8815_WAVE;
        p = &lsensor_adc_table_u8815[0];
    }
    else if(machine_is_msm7x27a_C8655_NAND())
    {
        apds_9900_pwindows_value = C8655_WINDOW;
        apds_9900_pwave_value = C8655_WAVE;
        p = &lsensor_adc_table_c8655[0];
    }
    else if(machine_is_msm7x27a_M660())
    {
        apds_9900_pwindows_value = M660_WINDOW;
        apds_9900_pwave_value = M660_WAVE;
        p = &lsensor_adc_table_m660[0];
    }
    /* C8812 is another name of C8820 */
    else if( machine_is_msm7x27a_C8820() )
    {
        p = &lsensor_adc_table_c8812[0];	
    }
    else if( machine_is_msm8255_u8730())
    {
        //sunlight = U8730_SUNLIGHT;
        p = &lsensor_adc_table_u8730[0];
    }
    else if ( machine_is_msm8255_u8680())
    {
        p = &lsensor_adc_table_u8680[0];
    }
    else if( machine_is_msm8255_u8667())
    {
        p = &lsensor_adc_table_u8667[0];
    }
    /* add U8825 & 8825D proximit and ambient  parameters */
    else if( machine_is_msm8x25_U8825()
        || machine_is_msm8x25_U8825D()
        /*delete G510 Y300*/
        || machine_is_msm8x25_C8825D())
    {
        apds_9900_pwindows_value = CU8825_WINDOW;
        apds_9900_pwave_value = CU8825_WAVE; 
        p = &lsensor_adc_table_cu8825[0];
    }
    else if( machine_is_msm8x25_C8812P() )
    {
        apds_9900_pwindows_value = C8812E_WINDOW;
        apds_9900_pwave_value = C8812E_WAVE;
        p = &lsensor_adc_table_c8812e[0];
    }
    /* add U8950 & C8950 proximit and ambient parameters */
    else if( machine_is_msm8x25_C8950D()
        || machine_is_msm8x25_U8950D()
        || machine_is_msm8x25_U8950() )
    {
        apds_9900_pwindows_value = CU8950_WINDOW;
        apds_9900_pwave_value = CU8950_WAVE; 
        p = &lsensor_adc_table_cu8950[0];
    }
    /*G510C*/
    else if( machine_is_msm8x25_C8813() )
    {
        apds_9900_pwindows_value = G510C_WINDOW;
        apds_9900_pwave_value = G510C_WAVE; 
        p = &lsensor_adc_table_g510c[0];	
    }
    /*G510*/
    else if( machine_is_msm8x25_U8951D()
        || machine_is_msm8x25_U8951()
        /* move C8813 to above */
        )
    {
        apds_9900_pwindows_value = G510_WINDOW;
        apds_9900_pwave_value = G510_WAVE; 
        p = &lsensor_adc_table_g510[0];	
        aps9900_init_regdata[PPCOUNT_REG_INDEX].data = 0x0C;
    }
    /*Y300*/
    else if( machine_is_msm8x25_C8833D()
        || machine_is_msm8x25_U8833D()
        || machine_is_msm8x25_U8833() )
    {
        apds_9900_pwindows_value = Y300_WINDOW;
        apds_9900_pwave_value = Y300_WAVE; 
        p = &lsensor_adc_table_y300[0];
        aps9900_init_regdata[PPCOUNT_REG_INDEX].data = 0x0A;
    }
    else if( machine_is_msm7x27a_H867G() 
          || machine_is_msm7x27a_H868C())
    {
        apds_9900_pwindows_value = H867G_WINDOW;
        apds_9900_pwave_value = H867G_WAVE;
        aps9900_init_regdata[4].data = 8; //to increase the sensor sensitivity
        p = &lsensor_adc_table_H867G[0];
    }
    else if( machine_is_msm8x25_H881C() 
        || machine_is_msm8x25_Y301_A1())
    {
       	apds_9900_pwindows_value = H881C_WINDOW;
       	apds_9900_pwave_value = H881C_WAVE;
       	p = &lsensor_adc_table_h881c[0];
    }
    else
    {
        /* do nothing */
    }
    for(i = 0;i < ARRAY_SIZE(lsensor_adc_table) ; i++ )
    {
        lsensor_adc_table[i] = p[i];
    }
    origin_prox = MAX_ADC_PROX_VALUE - apds_9900_pwindows_value;
    min_proximity_value = origin_prox;

    platform_data = client->dev.platform_data;
    /*27A doesn't need match power*/
    if (platform_data->aps9900_power)
    {
    #ifdef CONFIG_ARCH_MSM7X30
        ret = platform_data->aps9900_power(IC_PM_ON);
        if (ret < 0)
        {
            pr_err("%s:aps9900 power on error!\n", __func__);
            goto err_exit ;
        }
    #endif
    }
    mdelay(5);
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        printk(KERN_ERR "%s, line %d: need I2C_FUNC_I2C\n", __func__, __LINE__);
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }
    /* delete some lines */

    aps = kzalloc(sizeof(*aps), GFP_KERNEL);
    if (aps == NULL) {
        printk(KERN_ERR "%s, line %d: kzalloc fail!\n", __func__, __LINE__);
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }

    mutex_init(&aps->mlock);
    INIT_WORK(&aps->work, aps_9900_work_func);
    aps->client = client;
    i2c_set_clientdata(client, aps);

    PROXIMITY_DEBUG(KERN_INFO "ghj aps_9900_probe send command 2\n ");

    ret = aps_i2c_reg_init(aps);
    if (ret <0)
    {
        printk(KERN_ERR "aps_i2c_reg_init: Failed to init aps_i2c_reg_init!(%d)\n",ret);
        goto err_detect_failed;
    }
    /* delete mdelay(12) */
    if (client->irq) 
    {
        if (platform_data->aps9900_gpio_config_interrupt)
        {
            ret = platform_data->aps9900_gpio_config_interrupt();
            if (ret) 
            {
                printk(KERN_ERR "%s, line %d: gpio_tlmm_config error\n", __func__, __LINE__);
                goto err_gpio_config_failed;
            }
        }
        /*DELETE*/
        if (request_irq(client->irq, aps_irq_handler,IRQF_TRIGGER_LOW, client->name, aps) >= 0) 
        {
            PROXIMITY_DEBUG("Received IRQ!\n");
    	    disable_irq(aps->client->irq);
            #if 0
            if (set_irq_wake(client->irq, 1) < 0)
            {
                printk(KERN_ERR "failed to set IRQ wake\n");
            }
            #endif
        }
        else 
        {
            printk("Failed to request IRQ!\n");
        }
         /* set the threshold of proximity and ALS */
         ret = set_9900_register(aps, APDS9900_AILTL_REG, 0, 1);
         ret |= set_9900_register(aps, APDS9900_AIHTL_REG, 0, 1);
         ret |= set_9900_register(aps, APDS9900_PILTL_REG, 0, 1);
         /* modify the high proximity threshold from 500 to 800 */
         /*modify the value*/
         ret |= set_9900_register(aps, APDS9900_PIHTL_REG, 0x3c0, 1);
		 /* set the low thresthold of 1023 to make sure make an interrupt */
         /*modify the value*/
         ret |= set_9900_register(aps, APDS9900_PILTL_REG, 0x3bf, 1);
         if (ret)
         {
             printk(KERN_ERR "%s:set the threshold of proximity and ALS failed(%d)!", __func__, ret);
         }
		 
    }
    /* if not define irq,then error */
    else
    {
        printk(KERN_ERR "please set the irq num!\n");
        goto err_detect_failed;
    }
    if (sensor_9900_dev == NULL) 
    {
         aps->input_dev = input_allocate_device();
         if (aps->input_dev == NULL) {
         ret = -ENOMEM;
         printk(KERN_ERR "%s, line %d: Failed to allocate input device\n", __func__, __LINE__);
         goto err_input_dev_alloc_failed;
         }
        aps->input_dev->name = "sensors_aps";
        aps->input_dev->id.bustype = BUS_I2C;
        input_set_drvdata(aps->input_dev, aps);
        
        ret = input_register_device(aps->input_dev);
        if (ret) {
            printk(KERN_ERR "aps_9900_probe: Unable to register %s input device\n", aps->input_dev->name);
            goto err_input_register_device_failed;
        }
        sensor_9900_dev = aps->input_dev;
    } else {
        printk(KERN_INFO "sensor_dev is not null+++++++++++++++++++++++\n");
        aps->input_dev = sensor_9900_dev;
    }
    set_bit(EV_ABS, aps->input_dev->evbit);
    input_set_abs_params(aps->input_dev, ABS_LIGHT, 0, 10240, 0, 0);
    input_set_abs_params(aps->input_dev, ABS_DISTANCE, 0, 1, 0, 0);

    ret = misc_register(&light_device);
    if (ret) {
        printk(KERN_ERR "aps_9900_probe: light_device register failed\n");
        goto err_light_misc_device_register_failed;
    }

    ret = misc_register(&proximity_device);
    if (ret) {
        printk(KERN_ERR "aps_9900_probe: proximity_device register failed\n");
        goto err_proximity_misc_device_register_failed;
    }

    if( light_device.minor != MISC_DYNAMIC_MINOR ){
        light_device_minor = light_device.minor;
    }


    if( proximity_device.minor != MISC_DYNAMIC_MINOR ){
        proximity_device_minor = proximity_device.minor ;
    }

    wake_lock_init(&proximity_wake_lock, WAKE_LOCK_SUSPEND, "proximity");
    aps_wq = create_singlethread_workqueue("aps_wq");

    if (!aps_wq) 
    {
        ret = -ENOMEM;
        goto err_create_workqueue_failed;
    }

    this_aps_data =aps;

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_APS);
    #endif
    ret = set_sensor_input(PS, aps->input_dev->dev.kobj.name);
    if (ret) {
        dev_err(&client->dev, "%s set_sensor_input failed\n", __func__);
        goto err_create_workqueue_failed;
    }
    ret = set_sensor_input(ALS, aps->input_dev->dev.kobj.name);
    if (ret) {
        dev_err(&client->dev, "%s set_sensor_input failed\n", __func__);
        goto err_create_workqueue_failed;
    }
    printk(KERN_INFO "aps_9900_probe: Start Proximity Sensor APS-9900\n");

    set_sensors_list(L_SENSOR + P_SENSOR);
    return 0;

err_create_workqueue_failed:
    misc_deregister(&proximity_device);
err_proximity_misc_device_register_failed:
    misc_deregister(&light_device);
err_light_misc_device_register_failed:
err_input_register_device_failed:
    input_free_device(aps->input_dev);
err_input_dev_alloc_failed:
err_gpio_config_failed:
err_detect_failed:
    kfree(aps);
err_alloc_data_failed:
err_check_functionality_failed:
	#ifdef CONFIG_ARCH_MSM7X30
    if(platform_data->aps9900_power)
    {
        platform_data->aps9900_power(IC_PM_OFF);
    }
	#endif
err_exit:
    return ret;
  
}
static int aps_9900_remove(struct i2c_client *client)
{
    struct aps_data *aps = i2c_get_clientdata(client);

    PROXIMITY_DEBUG("ghj aps_9900_remove enter\n ");
    if (aps->client->irq)
    {
        disable_irq(aps->client->irq);
    }
    free_irq(client->irq, aps);
    misc_deregister(&light_device);
    misc_deregister(&proximity_device);
    input_unregister_device(aps->input_dev);

    kfree(aps);
    return 0;
}

/*set  reg 0 */
static int aps_9900_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret;
    struct aps_data *aps = i2c_get_clientdata(client);

    PROXIMITY_DEBUG("ghj aps_9900_suspend enter\n ");
    if (aps->client->irq)
    {
        disable_irq(aps->client->irq);
    }
    ret = cancel_work_sync(&aps->work);

    /* set [PON] bit =0 ,meaning disables the oscillator */
    /*reconfig reg before supspend*/
    ret = set_9900_register(aps, APDS9900_ENABLE_REG,  APDS9900_POWER_OFF,0);
    if (ret)
    {
        printk(KERN_ERR "%s:set APDS9900_ENABLE_REG register[PON=OFF] failed(%d)!", __func__, ret);
    }
    return 0;
}
static int aps_9900_resume(struct i2c_client *client)
{
    int ret;
    struct aps_data *aps = i2c_get_clientdata(client);
    
    PROXIMITY_DEBUG("ghj aps_9900_resume enter\n ");
    /* Command 0 register: set [PON] bit =1 */
    ret = set_9900_register(aps, APDS9900_ENABLE_REG, reg0_value,0);
    if (ret)
    {
        printk(KERN_ERR "%s:set APDS9900_ENABLE_REG register[PON=ON] failed(%d)!", __func__, ret);
    }
    if (aps->client->irq)
    {
        enable_irq(aps->client->irq);
    }
    return 0;
}

static const struct i2c_device_id aps_id[] = {
    { "aps-9900", 0 },
    { }
};

static struct i2c_driver aps_driver = {
    .probe      = aps_9900_probe,
    .remove     = aps_9900_remove,
    .suspend    = aps_9900_suspend,
    .resume     = aps_9900_resume,
    .id_table   = aps_id,
    .driver = {
        .name   ="aps-9900",
    },
};

static int __devinit aps_9900_init(void)
{
    return i2c_add_driver(&aps_driver);
}

static void __exit aps_9900_exit(void)
{
    i2c_del_driver(&aps_driver);
    if (aps_wq)
    {
        destroy_workqueue(aps_wq);
    }
}

device_initcall_sync(aps_9900_init);
module_exit(aps_9900_exit);
MODULE_DESCRIPTION("Proximity Driver");
MODULE_LICENSE("GPL");
