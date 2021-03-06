
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/input/ft5346.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <misc/app_info.h>

#ifdef CONFIG_LOG_JANK
#include <huawei_platform/log/log_jank.h>
#endif

#if  WT_ADD_CTP_INFO
#include <linux/hardware_info.h>
#endif

#ifdef WT_CTP_GESTURE_SUPPORT
#include <linux/input.h>

#define PANELID_MAX_ITEM_LONGTH	128
#define FT_GESTURE_MASK					0x2781
#define FT_GESTURE_BIT(i)					(1<< i)
#define FT_GESTURE_MASK_DOBLUECLICK		FT_GESTURE_BIT(0)
#define FT_GESTURE_MASK_C					FT_GESTURE_BIT(13)
#define FT_GESTURE_MASK_E					FT_GESTURE_BIT(13)
#define FT_GESTURE_MASK_M					FT_GESTURE_BIT(13)
#define FT_GESTURE_MASK_W					FT_GESTURE_BIT(13)

static u16 ft_easy_wakeup_gesture = 0;
static unsigned short coordinate_x[150] = {0};
static unsigned short coordinate_y[150] = {0};
static struct i2c_client *gesture_client = NULL;

static char panelid[PANELID_MAX_ITEM_LONGTH];
static bool check_single_gesture_onoff(u16 mask)
{
if((ft_easy_wakeup_gesture & mask ) != 0)
	return true;
else
	return false;
}
#endif

#ifdef WT_CTP_GESTURE_SUPPORT
bool ft_gesture_onoff(void)
{
if( (ft_easy_wakeup_gesture & FT_GESTURE_MASK ) != 0)
	return true;
else
	return false;
}
#else
bool ft_gesture_onoff(void)
{
return false;
}
EXPORT_SYMBOL(ft_gesture_onoff);
#endif

#if CTP_CHARGER_DETECT
#include <linux/power_supply.h>
#endif

#ifdef CONFIG_HUAWEI_DSM
#include <dsm/dsm_pub.h>
#endif/*CONFIG_HUAWEI_DSM*/
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define FT_SUSPEND_LEVEL 1
#endif

#define FT_DEBUG_DIR_NAME   "ts_debug"

#define TPD_MAX_POINTS_5    5
#define TPD_MAX_POINTS_10   10

#define TPD_MAX_POINTS_2    2
#define AUTO_CLB_NEED   1
#define AUTO_CLB_NONEED     0
struct Upgrade_Info fts_updateinfo_curr;
static struct Upgrade_Info fts_updateinfo[] =
{
    {0x55,"FT5x06",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
    {0x08,"FT5606",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x06, 100, 2000},
    {0x0a,"FT5x16",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x07, 1, 1500},
    {0x05,"FT6208",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,60, 30, 0x79, 0x05, 10, 2000},
    {0x06,"FT6x06",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x08, 10, 2000},
    {0x36,"FT6x36",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x18, 10, 2000},//CHIP ID error
    {0x55,"FT5x06i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
    {0x14,"FT5336",TPD_MAX_POINTS_10,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
    {0x13,"FT3316",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
    {0x12,"FT5436i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
    {0x11,"FT5336i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
    {0x54,"FT5x46",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,2, 2, 0x54, 0x2c, 10, 1350},
    {0x86,"FT8607",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,2, 2, 0x86, 0xA7, 20, 2000},//F8607
};

#define FT_STORE_TS_INFO(buf, id, name, max_tch, group_id, fw_vkey_support, \
            fw_name, fw_maj, fw_min, fw_sub_min) \
            snprintf(buf, FT_INFO_MAX_LEN, \
                "controller\t= focaltech\n" \
                "model\t\t= 0x%x\n" \
                "name\t\t= %s\n" \
                "max_touches\t= %d\n" \
                "drv_ver\t\t= 0x%x\n" \
                "group_id\t= 0x%x\n" \
                "fw_vkey_support\t= %s\n" \
                "fw_name\t\t= %s\n" \
                "fw_ver\t\t= %d.%d.%d\n", id, name, \
                max_tch, FT_DRIVER_VERSION, group_id, \
                fw_vkey_support, fw_name, fw_maj, fw_min, \
                fw_sub_min)

#if CTP_PROC_INTERFACE
#define CTP_PARENT_PROC_NAME  "touchscreen"
#define CTP_OPEN_PROC_NAME        "ctp_openshort_test"
#define FTS_INI_FILE_NAME		     "fts_tcl.ini"

static DEFINE_MUTEX(fts_mutex);
struct i2c_client *fts_i2c_client = NULL;
EXPORT_SYMBOL(fts_i2c_client);
extern int fts_open_short_test(char *ini_file_name, char *bufdest, ssize_t* pinumread);
#ifdef CONFIG_HUAWEI_DSM
static struct dsm_dev dsm_touch_ft5346 = {
	.name = "dsm_i2c_bus",
//	.device_name = "TP",
	.ic_name = "NNN",
	.module_name = "NNN",
	.fops = NULL,
	.buff_size = 1024,
};
static struct dsm_client *touch_dclient = NULL;

#endif/*CONFIG_HUAWEI_DSM*/

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos);
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos);
static const struct file_operations ctp_open_procs_fops =
{
    .write = ctp_open_proc_write,
    .read = ctp_open_proc_read,
    .owner = THIS_MODULE,
};

#endif

#if FTS_PROC_APK_DEBUG
#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER	2
#define PROC_RAWDATA			3
#define PROC_AUTOCLB			4

#define PROC_NAME	"ft5x0x-debug"
static unsigned char proc_operate_mode = PROC_RAWDATA;
static struct proc_dir_entry *ft5x0x_proc_entry = NULL;
#endif

static u8 is_ic_update_crash = 0;
static struct i2c_client *update_client = NULL;

#if CTP_CHARGER_DETECT
extern int power_supply_get_battery_charge_state(struct power_supply *psy);
static struct power_supply		*batt_psy =NULL;
static u8 is_charger_plug = 0;
static u8 pre_charger_status = 0;
//static u8 current_diff_state = 0;
#endif

static int ft5x06_i2c_read(struct i2c_client *client, char *writebuf,
                           int writelen, char *readbuf, int readlen)
{
    int ret;

    if (writelen > 0)
    {
        struct i2c_msg msgs[] =
        {
            {
                .addr = client->addr,
                .flags = 0,
                .len = writelen,
                .buf = writebuf,
            },
            {
                .addr = client->addr,
                .flags = I2C_M_RD,
                .len = readlen,
                .buf = readbuf,
            },
        };
        ret = i2c_transfer(client->adapter, msgs, 2);
        if (ret < 0)
            dev_err(&client->dev, "%s: i2c read error.\n",__func__);
    }
    else
    {
        struct i2c_msg msgs[] =
        {
            {
                .addr = client->addr,
                .flags = I2C_M_RD,
                .len = readlen,
                .buf = readbuf,
            },
        };
        ret = i2c_transfer(client->adapter, msgs, 1);
        if (ret < 0)
            dev_err(&client->dev, "%s:i2c read error.\n", __func__);
    }
#ifdef CONFIG_HUAWEI_DSM
	if(ret<0) {
		dsm_client_record(touch_dclient,"i2c read err number:%d\n, ret = %d\n", ret);
		dsm_client_notify(touch_dclient, DSM_TP_I2C_RW_ERROR_NO);
	}
#endif/*CONFIG_HUAWEI_DSM*/
    return ret;
}

static int ft5x06_i2c_write(struct i2c_client *client, char *writebuf,
                            int writelen)
{
    int ret;

    struct i2c_msg msgs[] =
    {
        {
            .addr = client->addr,
            .flags = 0,
            .len = writelen,
            .buf = writebuf,
        },
    };
    ret = i2c_transfer(client->adapter, msgs, 1);
    if (ret < 0)
        dev_err(&client->dev, "%s: i2c write error.\n", __func__);

#ifdef CONFIG_HUAWEI_DSM
	if(ret<0) {
		dsm_client_record(touch_dclient,"i2c write err ret = %d\n", ret);
		dsm_client_notify(touch_dclient, DSM_TP_I2C_RW_ERROR_NO);
	}
#endif/*CONFIG_HUAWEI_DSM*/
    return ret;
}

static int ft5x0x_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
    u8 buf[2] = {0};

    buf[0] = addr;
    buf[1] = val;

    return ft5x06_i2c_write(client, buf, sizeof(buf));
}

static int ft5x0x_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
    return ft5x06_i2c_read(client, &addr, 1, val, 1);
}

static void ft5x06_update_fw_vendor_id(struct ft5x06_ts_data *data)
{
    struct i2c_client *client = data->client;
    u8 reg_addr;
    int err;

    reg_addr = FT_REG_FW_VENDOR_ID;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_vendor_id, 1);
    if (err < 0)
        dev_err(&client->dev, "fw vendor id read failed");
}

static void ft5x06_update_fw_ver(struct ft5x06_ts_data *data)
{
    struct i2c_client *client = data->client;
    u8 reg_addr;
    int err;

    reg_addr = FT_REG_FW_VER;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[0], 1);
    if (err < 0)
        dev_err(&client->dev, "fw major version read failed");

    reg_addr = FT_REG_FW_MIN_VER;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[1], 1);
    if (err < 0)
        dev_err(&client->dev, "fw minor version read failed");

    reg_addr = FT_REG_FW_SUB_MIN_VER;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[2], 1);
    if (err < 0)
        dev_err(&client->dev, "fw sub minor version read failed");

    dev_info(&client->dev, "Firmware version = %d.%d.%d\n",
             data->fw_ver[0], data->fw_ver[1], data->fw_ver[2]);
}

#if WT_CTP_GESTURE_SUPPORT
/************************************************************************
 * Name: check_gesture
 * Brief: report gesture id
 * Input: gesture id
 * Output: no
 * Return: no
 ***********************************************************************/
static void check_gesture(int gesture_id, struct input_dev *ip_dev)
{
	switch(gesture_id)
	{
		case 0x24://k
			if(check_single_gesture_onoff(FT_GESTURE_MASK_DOBLUECLICK))
			{
				input_report_key(ip_dev, KEY_F1, 1);
				input_sync(ip_dev);
				input_report_key(ip_dev, KEY_F1, 0);
				input_sync(ip_dev);
				#ifdef CONFIG_LOG_JANK
				LOG_JANK_D(JLID_WAKEUP_DBCLICK, "%s", "JL_WAKEUP_DBCLICK");
				#endif
			}
			break;
		case 0x34://c
			if(check_single_gesture_onoff(FT_GESTURE_MASK_C))
			{
				input_report_key(ip_dev, KEY_F8, 1);
				input_sync(ip_dev);
				input_report_key(ip_dev, KEY_F8, 0);
				input_sync(ip_dev);
			}
			break;
		case 0x33://e
			if(check_single_gesture_onoff(FT_GESTURE_MASK_E))
			{
				input_report_key(ip_dev, KEY_F9, 1);
				input_sync(ip_dev);
				input_report_key(ip_dev, KEY_F9, 0);
				input_sync(ip_dev);
			}
			break;
		case 0x32://m
			if(check_single_gesture_onoff(FT_GESTURE_MASK_M))
			{
				input_report_key(ip_dev, KEY_F10,1);
				input_sync(ip_dev);
				input_report_key(ip_dev, KEY_F10, 0);
				input_sync(ip_dev);
			}
			break;
		case 0x31://w
			if(check_single_gesture_onoff(FT_GESTURE_MASK_W))
			{
				input_report_key(ip_dev, KEY_F11, 1);
				input_sync(ip_dev);
				input_report_key(ip_dev, KEY_F11, 0);
				input_sync(ip_dev);
			}
			break;
	   default:
		   break;
    }


}
/************************************************************************
 * Name: fts_read_Gestruedata
 * Brief: read data from TP register
 * Input: no
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_read_Gestruedata(struct input_dev *ip_dev)
{
	unsigned char buf[FTS_GESTRUE_POINTS * 3] = { 0 };
	int ret = -1;
	int i = 0;
	int gesture_id = 0;
	short pointnum = 0;
	buf[0] = 0xd3;

	pointnum = 0;
	ret = ft5x06_i2c_read(gesture_client, buf, 1, buf, FTS_GESTRUE_POINTS_HEADER);
	if (ret < 0)
	{
		CTP_ERROR( "%s read touchdata failed.\n", __func__);
		return ret;
	}

	#ifdef CONFIG_LOG_JANK
	LOG_JANK_D(JLID_TP_GESTURE_KEY , "%s", "JL_TP_GESTURE_KEY ");
	#endif

	/* FW */
	//if (fts_updateinfo_curr.CHIP_ID==0x54|| fts_updateinfo_curr.CHIP_ID==0x58)
	{
		gesture_id = buf[0];
		pointnum = (short)(buf[1]) & 0xff;
		buf[0] = 0xd3;


		if((pointnum * 4 + 8)<255)
		{
			ret = ft5x06_i2c_read(gesture_client, buf, 1, buf, (pointnum * 4 + 8));
		}
		else
		{
			ret = ft5x06_i2c_read(gesture_client, buf, 1, buf, 255);
			ret = ft5x06_i2c_read(gesture_client, buf, 0, buf+255, (pointnum * 4 + 8) -255);
		}
		if (ret < 0)
		{
			CTP_ERROR( "%s read touchdata failed.\n", __func__);
			return ret;
		}

		check_gesture(gesture_id,ip_dev);
		CTP_INFO("Inpot report key is ok!\n");

		for(i = 0;i < pointnum;i++)
		{
			coordinate_x[i] =  (((s16) buf[2 + (4 * i)]) & 0x0F) <<
				8 | (((s16) buf[3 + (4 * i)])& 0xFF);
			coordinate_y[i] = (((s16) buf[4 + (4 * i)]) & 0x0F) <<
				8 | (((s16) buf[5 + (4 * i)]) & 0xFF);
		}
		return -1;
	}

	return -1;
}
#endif

static irqreturn_t ft5x06_ts_interrupt(int irq, void *dev_id)
{
    struct ft5x06_ts_data *data = dev_id;
    struct input_dev *ip_dev;
    int rc, i;
    u32 id, x, y, status, num_touches;
    u8 reg = 0x00, *buf;
    bool update_input = false;

#if WT_CTP_GESTURE_SUPPORT
int ret;
u8 state;
#endif

 //   printk("%s: ======== ft5x06_ts_interrupt  ===== \n", __func__);

    if (!data)
    {
        CTP_ERROR("%s: Invalid data\n", __func__);
        return IRQ_HANDLED;
    }

#if CTP_CHARGER_DETECT
	if (!batt_psy)
	{
		//CTP_ERROR("tp interrupt battery supply not found\n");
		batt_psy = power_supply_get_by_name("usb");
	}
	else{
		is_charger_plug = (u8)power_supply_get_battery_charge_state(batt_psy);
		
		//CTP_DEBUG("1 is_charger_plug %d, prev %d", is_charger_plug, pre_charger_status);
		if(is_charger_plug != pre_charger_status){
			pre_charger_status = is_charger_plug;
			ft5x0x_write_reg(update_client, 0x8B, is_charger_plug);
			//CTP_DEBUG("2 is_charger_plug %d, prev %d", is_charger_plug, pre_charger_status);
		}
	}
	
#endif

    ip_dev = data->input_dev;
    buf = data->tch_data;

    rc = ft5x06_i2c_read(data->client, &reg, 1,
                         buf, data->tch_data_len);
    if (rc < 0)
    {
        dev_err(&data->client->dev, "%s: read data fail\n", __func__);
        return IRQ_HANDLED;
    }

#if WT_CTP_GESTURE_SUPPORT
		if(ft_gesture_onoff())
		{
				ret = ft5x0x_read_reg(gesture_client, 0xd0,&state);
				printk("in event gesture:%d\n", state);
				if (ret<0) {
					printk("read value fail");
				}
				if(state ==1){
					fts_read_Gestruedata(ip_dev);
					return IRQ_HANDLED;
				}
		}
#endif

    for (i = 0; i < data->pdata->num_max_touches; i++)
    {
        id = (buf[FT_TOUCH_ID_POS + FT_ONE_TCH_LEN * i]) >> 4;
        if (id >= FT_MAX_ID)
            break;

        update_input = true;

        x = (buf[FT_TOUCH_X_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
            (buf[FT_TOUCH_X_L_POS + FT_ONE_TCH_LEN * i]);
        y = (buf[FT_TOUCH_Y_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
            (buf[FT_TOUCH_Y_L_POS + FT_ONE_TCH_LEN * i]);
		
        status = buf[FT_TOUCH_EVENT_POS + FT_ONE_TCH_LEN * i] >> 6;

        num_touches = buf[FT_TD_STATUS] & FT_STATUS_NUM_TP_MASK;

        /* invalid combination */
        if (!num_touches && !status && !id)
            break;
/*
		if(y == 2000){
			
			y = 1344;
			
          switch(x)
          	{
          	   case 180: x = 150;break;
			   case 540: x = 360;break;
			   case 900: x = 580;break;
			   default:  break;
          	}
		
}
*/	
        input_mt_slot(ip_dev, id);
        if (status == FT_TOUCH_DOWN || status == FT_TOUCH_CONTACT)
        {
            input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
            input_report_abs(ip_dev, ABS_MT_POSITION_X, x);
            input_report_abs(ip_dev, ABS_MT_POSITION_Y, y);
        }
        else
        {
            input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
        }
      }

    if (update_input)
    {
        input_mt_report_pointer_emulation(ip_dev, false);
        input_sync(ip_dev);
    }

	if(num_touches == 0)
	{
		 for (i = 0; i < data->pdata->num_max_touches; i++)
		{
		    input_mt_slot(ip_dev, i);
		    input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
		}
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}
    return IRQ_HANDLED;
}

static int ft5x06_power_on(struct ft5x06_ts_data *data, bool on)
{
    int rc;

    if (!on)
        goto power_off;

    rc = regulator_enable(data->vdd);
    if (rc)
    {
        dev_err(&data->client->dev,
                "Regulator vdd enable failed rc=%d\n", rc);
        return rc;
    }

    rc = regulator_enable(data->vcc_i2c);
    if (rc)
    {
        dev_err(&data->client->dev,
                "Regulator vcc_i2c enable failed rc=%d\n", rc);
        regulator_disable(data->vdd);
    }

    return rc;

power_off:
    rc = regulator_disable(data->vdd);
    if (rc)
    {
        dev_err(&data->client->dev,
                "Regulator vdd disable failed rc=%d\n", rc);
        return rc;
    }

    rc = regulator_disable(data->vcc_i2c);
    if (rc)
    {
        dev_err(&data->client->dev,
                "Regulator vcc_i2c disable failed rc=%d\n", rc);
        rc = regulator_enable(data->vdd);
        if (rc)
        {
            dev_err(&data->client->dev,
                    "Regulator vdd enable failed rc=%d\n", rc);
        }
    }

    return rc;
}

static int ft5x06_power_init(struct ft5x06_ts_data *data, bool on)
{
    int rc;

    if (!on)
        goto pwr_deinit;

    data->vdd = regulator_get(&data->client->dev, "vdd");
    if (IS_ERR(data->vdd))
    {
        rc = PTR_ERR(data->vdd);
        dev_err(&data->client->dev,
                "Regulator get failed vdd rc=%d\n", rc);
        return rc;
    }

    if (regulator_count_voltages(data->vdd) > 0)
    {
        rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
                                   FT_VTG_MAX_UV);
        if (rc)
        {
            dev_err(&data->client->dev,
                    "Regulator set_vtg failed vdd rc=%d\n", rc);
            goto reg_vdd_put;
        }
    }

    data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
    if (IS_ERR(data->vcc_i2c))
    {
        rc = PTR_ERR(data->vcc_i2c);
        dev_err(&data->client->dev,
                "Regulator get failed vcc_i2c rc=%d\n", rc);
        goto reg_vdd_set_vtg;
    }

    if (regulator_count_voltages(data->vcc_i2c) > 0)
    {
        rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
                                   FT_I2C_VTG_MAX_UV);
        if (rc)
        {
            dev_err(&data->client->dev,
                    "Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
            goto reg_vcc_i2c_put;
        }
    }

    return 0;

reg_vcc_i2c_put:
    regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
    regulator_put(data->vdd);
    return rc;

pwr_deinit:
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);

    regulator_put(data->vdd);

    if (regulator_count_voltages(data->vcc_i2c) > 0)
        regulator_set_voltage(data->vcc_i2c, 0, FT_I2C_VTG_MAX_UV);

    regulator_put(data->vcc_i2c);
    return 0;
}

#if 0
static int ft5x06_ts_pinctrl_init(struct ft5x06_ts_data *ft5x06_data)
{
    int retval;

    /* Get pinctrl if target uses pinctrl */
    ft5x06_data->ts_pinctrl = devm_pinctrl_get(&(ft5x06_data->client->dev));
    if (IS_ERR_OR_NULL(ft5x06_data->ts_pinctrl))
    {
        dev_dbg(&ft5x06_data->client->dev,
                "Target does not use pinctrl\n");
        retval = PTR_ERR(ft5x06_data->ts_pinctrl);
        ft5x06_data->ts_pinctrl = NULL;
        return retval;
    }

    ft5x06_data->gpio_state_active
        = pinctrl_lookup_state(ft5x06_data->ts_pinctrl,
                               "pmx_ts_active");
    if (IS_ERR_OR_NULL(ft5x06_data->gpio_state_active))
    {
        dev_dbg(&ft5x06_data->client->dev,
                "Can not get ts default pinstate\n");
        retval = PTR_ERR(ft5x06_data->gpio_state_active);
        ft5x06_data->ts_pinctrl = NULL;
        return retval;
    }

    ft5x06_data->gpio_state_suspend
        = pinctrl_lookup_state(ft5x06_data->ts_pinctrl,
                               "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(ft5x06_data->gpio_state_suspend))
    {
        dev_err(&ft5x06_data->client->dev,
                "Can not get ts sleep pinstate\n");
        retval = PTR_ERR(ft5x06_data->gpio_state_suspend);
        ft5x06_data->ts_pinctrl = NULL;
        return retval;
    }

    return 0;
}
#endif

static int ft5x06_ts_pinctrl_select(struct ft5x06_ts_data *ft5x06_data,
                                    bool on)
{
    struct pinctrl_state *pins_state;
    int ret;

    pins_state = on ? ft5x06_data->gpio_state_active
                 : ft5x06_data->gpio_state_suspend;
    if (!IS_ERR_OR_NULL(pins_state))
    {
        ret = pinctrl_select_state(ft5x06_data->ts_pinctrl, pins_state);
        if (ret)
        {
            dev_err(&ft5x06_data->client->dev,
                    "can not set %s pins\n",
                    on ? "pmx_ts_active" : "pmx_ts_suspend");
            return ret;
        }
    }
    else
    {
        dev_err(&ft5x06_data->client->dev,
                "not a valid '%s' pinstate\n",
                on ? "pmx_ts_active" : "pmx_ts_suspend");
    }

    return 0;
}


#ifdef CONFIG_PM
static int ft5x06_ts_suspend(struct device *dev)
{
    struct ft5x06_ts_data *data = dev_get_drvdata(dev);
    //char txbuf[2], i;
    char i;

    if (data->loading_fw)
    {
        dev_info(dev, "Firmware loading in process...\n");
        return 0;
    }


    if (data->suspended)
    {
        dev_info(dev, "Already in suspend state\n");
        return 0;
    }

#if WT_CTP_GESTURE_SUPPORT
		if(ft_gesture_onoff())
		{

		ft5x0x_write_reg(gesture_client, 0xd0, 0x01);
		dev_err(dev, "suspend fts_updateinfo_curr.CHIP_ID= 0x%x\n",fts_updateinfo_curr.CHIP_ID);
		//if (fts_updateinfo_curr.CHIP_ID==0x54 || fts_updateinfo_curr.CHIP_ID==0x58)
		{
			ft5x0x_write_reg(gesture_client, 0xd1, 0xff);
			ft5x0x_write_reg(gesture_client, 0xd2, 0xff);
			ft5x0x_write_reg(gesture_client, 0xd5, 0xff);
			ft5x0x_write_reg(gesture_client, 0xd6, 0xff);
			ft5x0x_write_reg(gesture_client, 0xd7, 0xff);
			ft5x0x_write_reg(gesture_client, 0xd8, 0xff);
		}
		enable_irq_wake(data->client->irq);
		CTP_DEBUG("in suspend gesture\n");

		return 0;
	}
#endif

    disable_irq(data->client->irq);

    /* release all touches */
    for (i = 0; i < data->pdata->num_max_touches; i++)
    {
        input_mt_slot(data->input_dev, i);
        input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
    }
    input_mt_report_pointer_emulation(data->input_dev, false);
    input_sync(data->input_dev);

    if (gpio_is_valid(data->pdata->reset_gpio))
    {
       // txbuf[0] = FT_REG_PMODE;
       // txbuf[1] = FT_PMODE_HIBERNATE;
        //err = ft5x06_i2c_write(data->client, txbuf, sizeof(txbuf));
	ft5x0x_write_reg(gesture_client, FT_REG_PMODE, FT_PMODE_HIBERNATE);
	dev_err(dev, "FT5346: write 0xA5   03 \n");
	//gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
       msleep(data->pdata->hard_rst_dly);
    }


/*
    if (data->pdata->power_on)
    {
        err = data->pdata->power_on(false);
        if (err)
        {
            dev_err(dev, "power off failed");
            goto pwr_off_fail;
        }
    }
    else
    {
        err = ft5x06_power_on(data, false);
        if (err)
        {
            dev_err(dev, "power off failed");
            goto pwr_off_fail;
        }
    }
*/

    data->suspended = true;

    return 0;


/*
pwr_off_fail:
    if (gpio_is_valid(data->pdata->reset_gpio))
    {
        gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
        msleep(data->pdata->hard_rst_dly);
        gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
    }
    enable_irq(data->client->irq);
    return err;

*/
}

static int ft5x06_ts_resume(struct device *dev)
{
    struct ft5x06_ts_data *data = dev_get_drvdata(dev);
    if (!data->suspended)
    {
        dev_dbg(dev, "Already in awake state\n");
    }

#if WT_CTP_GESTURE_SUPPORT
          printk("Resume Gesture TP.\n");
	if(ft_gesture_onoff())
	{
		ft5x0x_write_reg(gesture_client,0xD0,0x00);
		printk("Resume Gesture TP Done.\n");
	}

#endif

/*
    if (data->pdata->power_on)
    {
        err = data->pdata->power_on(true);
        if (err)
        {
            dev_err(dev, "power on failed");
            return err;
        }
    }
    else
    {
        err = ft5x06_power_on(data, true);
        if (err)
        {
            dev_err(dev, "power on failed");
            return err;
        }
    }

*/

    if (gpio_is_valid(data->pdata->reset_gpio))
    {
        gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
        msleep(data->pdata->hard_rst_dly);
        gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
    }

    msleep(data->pdata->soft_rst_dly);
   
    enable_irq(data->client->irq);

#if CTP_CHARGER_DETECT
		batt_psy = power_supply_get_by_name("usb");
		if (!batt_psy)
			CTP_ERROR("tp resume battery supply not found\n");
		else{
			is_charger_plug = (u8)power_supply_get_battery_charge_state(batt_psy);

			CTP_DEBUG("is_charger_plug %d, prev %d", is_charger_plug, pre_charger_status);
			if(is_charger_plug){
				ft5x0x_write_reg(update_client, 0x8B, 1);
			}else{
				ft5x0x_write_reg(update_client, 0x8B, 0);
			}
		}
		pre_charger_status = is_charger_plug;
#endif


    data->suspended = false;

    return 0;
}

static const struct dev_pm_ops ft5x06_ts_pm_ops =
{
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
    .suspend = ft5x06_ts_suspend,
    .resume = ft5x06_ts_resume,
#endif
};

#else
static int ft5x06_ts_suspend(struct device *dev)
{
    return 0;
}

static int ft5x06_ts_resume(struct device *dev)
{
    return 0;
}

#endif

#if defined(CONFIG_FB)

static void fb_notify_resume_work(struct work_struct *work)
{
       struct ft5x06_ts_data *ft5x06_data =
               container_of(work, struct ft5x06_ts_data, fb_notify_work);
       ft5x06_ts_resume(&ft5x06_data->client->dev);
}
static int fb_notifier_callback(struct notifier_block *self,
                                unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct ft5x06_ts_data *ft5x06_data =
        container_of(self, struct ft5x06_ts_data, fb_notif);

    if (evdata && evdata->data && event == FB_EVENT_BLANK &&
        ft5x06_data && ft5x06_data->client)
    {
        blank = evdata->data;
        if (*blank == FB_BLANK_UNBLANK){
		  schedule_work(&ft5x06_data->fb_notify_work);
        }
         else if (*blank == FB_BLANK_POWERDOWN) {
            flush_work(&ft5x06_data->fb_notify_work);
            ft5x06_ts_suspend(&ft5x06_data->client->dev);
         }
    }

    return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ft5x06_ts_early_suspend(struct early_suspend *handler)
{
    struct ft5x06_ts_data *data = container_of(handler,
                                  struct ft5x06_ts_data,
                                  early_suspend);

    ft5x06_ts_suspend(&data->client->dev);
}

static void ft5x06_ts_late_resume(struct early_suspend *handler)
{
    struct ft5x06_ts_data *data = container_of(handler,
                                  struct ft5x06_ts_data,
                                  early_suspend);

    ft5x06_ts_resume(&data->client->dev);
}
#endif


int hid_to_i2c(struct i2c_client * client)
{
	u8 auc_i2c_write_buf[5] = {0};
	int bRet = 0;
#ifdef HIDTOI2C_DISABLE

	return 2;

#endif
	auc_i2c_write_buf[0] = 0xeb;
	auc_i2c_write_buf[1] = 0xaa;
	auc_i2c_write_buf[2] = 0x09;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 3);
	msleep(10);
	auc_i2c_write_buf[0] = auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = 0;
	ft5x06_i2c_read(client, auc_i2c_write_buf, 0, auc_i2c_write_buf, 3);
	CTP_DEBUG("auc_i2c_write_buf[0]:%x,auc_i2c_write_buf[1]:%x,auc_i2c_write_buf[2]:%x\n",auc_i2c_write_buf[0],auc_i2c_write_buf[1],auc_i2c_write_buf[2]);
	if(0xeb==auc_i2c_write_buf[0] && 0xaa==auc_i2c_write_buf[1] && 0x08==auc_i2c_write_buf[2])
	{
		bRet = 1;		
	}
	else bRet = 0;
	return bRet;
}


/************************************************************************
*   Name: fts_8607_writepram
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
int  fts_8607_writepram(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6]={0};
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret;

	dev_err(&client->dev,"8606 dw_lenth= %d",dw_lenth);
	if(dw_lenth > 0x10000 || dw_lenth ==0)
	{
		return -EIO;
	}

	for (i = 0; i < 20; i++)
	{
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55);
		msleep(100);
		/*********Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		i_ret = ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
		if(i_ret < 0)
		{
			dev_err(&client->dev,"[FTS] failed writing  0x55 ! \n");
			continue;
		}

		/*********Step 3:check READ-ID***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;
		reg_val[0] = reg_val[1] = 0x00;

		ft5x06_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == 0x86
			&& reg_val[1] == 0x06) || (reg_val[0] == 0x86
			&& reg_val[1] == 0x07))
		{
			//msleep(50);
			break;
		}
		else
		{
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);

			continue;
		}
	}

	if (i >= FT_UPGRADE_LOOP )
		return -EIO;

	/*********Step 4:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	dev_err(&client->dev,"Step 5:write firmware(FW) to ctpm flash\n");
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xae;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++)
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++)
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		ft5x06_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++)
		{
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		ft5x06_i2c_write(client, packet_buf, temp + 6);
	}

	/*********Step 5: read out checksum***********************/
	/*send the opration head */
	dev_err(&client->dev,"Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0xcc;
	ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc)
	{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",reg_val[0],bt_ecc);
		return -EIO;
	}
	dev_err(&client->dev,"checksum %X %X \n",reg_val[0],bt_ecc);
	dev_err(&client->dev,"Read flash and compare\n");

	msleep(50);

	/*********Step 6: start app***********************/
	dev_err(&client->dev,"Step 6: start app\n");
	auc_i2c_write_buf[0] = 0x08;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(20);

	return 0;
}


/************************************************************************
*   Name: fts_8607_ctpm_fw_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
int  fts_8607_ctpm_fw_upgrade(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth)
{
	u8 reg_val[4] = {0};
	u8 reg_val_id[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret;
	unsigned char cmd[20];
	unsigned char Checksum = 0;
	u32 uCheckStart =0x0000;
	u32 uCheckOff =0x20;
	u32 uStartAddr =0x00;

	auc_i2c_write_buf[0] = 0x05;
	reg_val_id[0] = 0x00;

	i_ret =ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val_id, 1);
	if(dw_lenth == 0)
	{
		return -EIO;
	}

	if(0x81 == (int)reg_val_id[0])
	{
		if(dw_lenth > 1024*64)
		{
			return -EIO;
		}
	}
	else if(0x80 == (int)reg_val_id[0])
	{
		if(dw_lenth > 1024*68)
		{
			return -EIO;
		}
	}

	for (i = 0; i < FT_UPGRADE_LOOP; i++)
	{
		msleep(10);
		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		auc_i2c_write_buf[1] = FT_UPGRADE_AA;
		i_ret = ft5x06_i2c_write(client, auc_i2c_write_buf, 2);
		if(i_ret < 0)
		{
			dev_err(&client->dev,"failed writing  0x55 and 0xaa ! \n");
			continue;
		}

		/*********Step 3:check READ-ID***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		reg_val[0] = reg_val[1] = 0x00;

		ft5x06_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == fts_updateinfo_curr.upgrade_id_1
			&& reg_val[1] == fts_updateinfo_curr.upgrade_id_2)|| (reg_val[0] == 0x86 && reg_val[1] == 0xA6)) {
				dev_err(&client->dev,"[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
					reg_val[0], reg_val[1]);
				break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);

			continue;
		}
	}

	if (i >= FT_UPGRADE_LOOP )
		return -EIO;

	/*Step 4:erase app and panel paramenter area*/
	dev_err(&client->dev,"Step 4:erase app and panel paramenter area\n");

	{
		cmd[0] = 0x05;
		cmd[1] = reg_val_id[0];//0x80;
		cmd[2] = 0x00;
		ft5x06_i2c_write(client, cmd, 3);
	}

	{
		cmd[0] = 0x09;
		cmd[1] = 0x0A;
		ft5x06_i2c_write(client, cmd, 2);
	}

	for(i=0; i<dw_lenth ; i++)
	{
		Checksum ^= pbt_buf[i];
	}
	msleep(50);

	auc_i2c_write_buf[0] = 0x61;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1350);

	for(i = 0;i < 15;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		if(0xF0==reg_val[0] && 0xAA==reg_val[1])
		{
			break;
		}
		msleep(50);
	}

	bt_ecc = 0;
	dev_err(&client->dev,"Step 5:write firmware(FW) to ctpm flash\n");

	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;

	for (j = 0; j < packet_number; j++) {
		temp = uCheckStart + j * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr = temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;
		uCheckOff = uStartAddr/lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++)
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		ft5x06_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
			msleep(1);
		}

		temp = (reg_val[0] << 8) | reg_val[1] ;
		if(i == 30)
			dev_err(&client->dev,"[FTS]: temp =%d, (uCheckOff + uCheckStart)=%d \n", temp,(uCheckOff + uCheckStart));
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = uCheckStart + packet_number * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr = temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;
		uCheckOff = uStartAddr/lenght;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		ft5x06_i2c_write(client, packet_buf, temp + 6);

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
			msleep(1);
		}

		temp = (reg_val[0] << 8) | reg_val[1] ;
		if(i == 30)
			dev_err(&client->dev,"[FTS]: temp =%d, (uCheckOff + uCheckStart)=%d \n", temp,(uCheckOff + uCheckStart));

	}

	msleep(50);

	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	dev_err(&client->dev,"Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0x64;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);
	temp = 0x0000+0;

	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);

	if (dw_lenth > LEN_FLASH_ECC_MAX)
	{
		temp = LEN_FLASH_ECC_MAX;
	}
	else
	{
		temp = dw_lenth;
		dev_err(&client->dev,"Step 6_1: read out checksum\n");
	}
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ft5x06_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);

	for(i = 0;i < 100;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		if (0xF0==reg_val[0] && 0x55==reg_val[1])
		{
			break;
		}
		msleep(1);

	}
	//----------------------------------------------------------------------
	if (dw_lenth > LEN_FLASH_ECC_MAX)
	{
		temp = LEN_FLASH_ECC_MAX;//??? 0x1000+LEN_FLASH_ECC_MAX
		auc_i2c_write_buf[0] = 0x65;
		auc_i2c_write_buf[1] = (u8)(temp >> 16);
		auc_i2c_write_buf[2] = (u8)(temp >> 8);
		auc_i2c_write_buf[3] = (u8)(temp);
		temp = dw_lenth-LEN_FLASH_ECC_MAX;
		auc_i2c_write_buf[4] = (u8)(temp >> 8);
		auc_i2c_write_buf[5] = (u8)(temp);
		i_ret = ft5x06_i2c_write(client, auc_i2c_write_buf, 6);

		msleep(dw_lenth/256);

		for(i = 0;i < 100;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if (0xF0==reg_val[0] && 0x55==reg_val[1])
			{
				break;
			}
			msleep(1);

		}
	}
	auc_i2c_write_buf[0] = 0x66;
	ft5x06_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc)
	{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
			reg_val[0],
			bt_ecc);

		return -EIO;
	}
	dev_err(&client->dev,"checksum %X %X \n",reg_val[0],bt_ecc);
	/*********Step 7: reset the new FW***********************/
	dev_err(&client->dev,"Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(200);   //make sure CTP startup normally
	return 0;
}

int  fts_8607_read_vendor_id_from_bootloader(struct i2c_client * client)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u8 auc_i2c_write_buf[10];
	int i_ret;
	u8 vid = 0xff;
	for (i= 0; i < FT_UPGRADE_LOOP; i++)
	{
		/*********Step 1:RESET***********************/
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		msleep(fts_updateinfo_curr.delay_aa);
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55);
		if(i <=15)
		{
			msleep(fts_updateinfo_curr.delay_55  + i*3);
		}
		else
		{
			msleep(fts_updateinfo_curr.delay_55 - (i-15)*2);
		}
		msleep(5);

		/*********Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		auc_i2c_write_buf[1] = FT_UPGRADE_AA;
		i_ret = ft5x06_i2c_write(client, auc_i2c_write_buf, 2);
		if(i_ret < 0)
		{
			dev_err(&client->dev,"failed writing  0x55 and 0xaa ! \n");
			continue;
		}

		/*********Step 3:check READ-ID***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		ft5x06_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		if ((reg_val[0] == fts_updateinfo_curr.upgrade_id_1&& reg_val[1] == fts_updateinfo_curr.upgrade_id_2)|| (reg_val[0] == 0x86 && reg_val[1] == 0xA6))
		{
			dev_err(&client->dev,"[FTS] Step 3: READ OK CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);

				continue;
		}
	}
	if (i >= FT_UPGRADE_LOOP )
		return -EIO;
	for(i=0;i<5;i++)
	{
		i_ret = ft5x0x_read_reg(client, 0xA8, &vid);//read vendor id from bootloader
		if( (i_ret < 0)||((vid != VENDOR_TCL) && (vid != VENDOR_TIANMA) && (vid != VENDOR_INX) && (vid != VENDOR_SHARP)) )
		{
			dev_err(&client->dev, "[FTS] Step 4: read vendor ID from flash error, i_ret = %d, vid = 0x%x \n", i_ret, vid);
			msleep(1);
			continue;
		}
		break;
	}

	/*********Step 7: reset the new FW***********************/
	dev_err(&client->dev,"Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	ft5x06_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(200);   //make sure CTP startup normally
	return vid;
}

static int ft5x06_fw_upgrade_start(struct i2c_client *client,
                                   const u8 *data, u32 data_len)
{
    struct ft5x06_ts_data *ts_data = i2c_get_clientdata(client);
    struct fw_upgrade_info info = ts_data->pdata->info;
    //u8 reset_reg;
    u8 reg_addr;
    u8 chip_id = 0x00;
    u8 w_buf[FT_MAX_WR_BUF] = {0}, r_buf[FT_MAX_RD_BUF] = {0};
    u8 pkt_buf[FT_FW_PKT_LEN + FT_FW_PKT_META_LEN];
    int i, j, temp;
    u32 pkt_num, pkt_len;
//    u8 is_5336_new_bootloader = false;
//    u8 is_5336_fwsize_30 = false;
    u8 fw_ecc;
	int i_ret;
//	u8 auc_i2c_write_buf[10];


#if 1//READ IC INFO
    reg_addr = FT_REG_ID;
    temp = ft5x06_i2c_read(client, &reg_addr, 1, &chip_id, 1);
	CTP_DEBUG("Update:Read ic info:%x\n",chip_id);
    if (temp < 0)
    {
        dev_err(&client->dev, "version read failed");
    }

    if (is_ic_update_crash)
    {
         chip_id = CTP_IC_TYPE_0;

    }

    for(i=0; i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info); i++)
    {
        if(chip_id==fts_updateinfo[i].CHIP_ID)
        {
            info.auto_cal = fts_updateinfo[i].AUTO_CLB;
            info.delay_55 = fts_updateinfo[i].delay_55;
            info.delay_aa = fts_updateinfo[i].delay_aa;
            info.delay_erase_flash = fts_updateinfo[i].delay_earse_flash;
            info.delay_readid = fts_updateinfo[i].delay_readid;
            info.upgrade_id_1 = fts_updateinfo[i].upgrade_id_1;
            info.upgrade_id_2 = fts_updateinfo[i].upgrade_id_2;

            break;
        }
    }
	
	ts_data->family_id = chip_id;
			
    if(i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info))
    {
        info.auto_cal = fts_updateinfo[11].AUTO_CLB;
        info.delay_55 = fts_updateinfo[11].delay_55;
        info.delay_aa = fts_updateinfo[11].delay_aa;
        info.delay_erase_flash = fts_updateinfo[11].delay_earse_flash;
        info.delay_readid = fts_updateinfo[11].delay_readid;
        info.upgrade_id_1 = fts_updateinfo[11].upgrade_id_1;
        info.upgrade_id_2 = fts_updateinfo[11].upgrade_id_2;
    }
#endif

    dev_err(&client->dev, "id1 = 0x%x id2 = 0x%x family_id=0x%x,data_len = %d\n",
            info.upgrade_id_1, info.upgrade_id_2, ts_data->family_id,data_len);
    /* determine firmware size */
    i_ret = hid_to_i2c(client);

	if(i_ret == 0)
	{
		CTP_DEBUG("[FTS] hid change to i2c fail ! \n");
	}

    for (i = 0, j = 0; i < FT_UPGRADE_LOOP; i++)
    {
        msleep(FT_EARSE_DLY_MS);
    #if 0
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
	{
		gpio_set_value_cansleep(ts_data->pdata->reset_gpio, 0);
		msleep(ts_data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(ts_data->pdata->reset_gpio, 1);
	}
	#else
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		msleep(info.delay_aa);

		//write 0x55 to register 0xfc 
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55);
		msleep(200);
	#endif
        i_ret = hid_to_i2c(client);
		if(i_ret == 0)
		{
			CTP_DEBUG("[FTS] hid change to i2c fail ! \n");
			//continue;
		}
		
	    msleep(10);
        /* Enter upgrade mode */
        w_buf[0] = FT_UPGRADE_55;
        ft5x06_i2c_write(client, &w_buf[0], 1);
        usleep(FT_55_AA_DLY_NS);
        w_buf[0] = FT_UPGRADE_AA;
        ft5x06_i2c_write(client, &w_buf[0], 1);
		if(i_ret < 0)
		{
			CTP_DEBUG("[FTS] failed writing  0x55 and 0xaa ! \n");
			continue;
		}
		

        /* check READ_ID */
        msleep(info.delay_readid);
        w_buf[0] = FT_READ_ID_REG;
        w_buf[1] = 0x00;
        w_buf[2] = 0x00;
        w_buf[3] = 0x00;

        ft5x06_i2c_read(client, w_buf, 4, r_buf, 2);


        CTP_DEBUG("r_buf[0] :%X,r_buf[1]: %X\n", r_buf[0], r_buf[1]);
        if (r_buf[0] != info.upgrade_id_1
            || r_buf[1] != info.upgrade_id_2)
        {
            dev_err(&client->dev, "Upgrade ID mismatch(%d), IC=0x%x 0x%x, info=0x%x 0x%x\n",
                    i, r_buf[0], r_buf[1],
                    info.upgrade_id_1, info.upgrade_id_2);
        }
        else
            break;
    }
    CTP_DEBUG("Begin to update \n\n");
    if (i >= FT_UPGRADE_LOOP)
    {
        dev_err(&client->dev, "Abort upgrade\n");
        return -EIO;
    }

    /* erase app and panel paramenter area */
	CTP_DEBUG("Step 4:erase app and panel paramenter area\n");
    w_buf[0] = FT_ERASE_APP_REG;
    ft5x06_i2c_write(client, w_buf, 1);
    msleep(1350);

	for(i = 0;i < 15;i++)
		{
			w_buf[0] = 0x6a;
			r_buf[0] = r_buf[1] = 0x00;
			ft5x06_i2c_read(client, w_buf, 1, r_buf, 2);
	        
			if(0xF0==r_buf[0] && 0xAA==r_buf[1])
			{
				break;
			}
			msleep(50);
	
		}
	 
		w_buf[0] = 0xB0;
		w_buf[1] = (u8) ((data_len >> 16) & 0xFF);
		w_buf[2] = (u8) ((data_len >> 8) & 0xFF);
		w_buf[3] = (u8) (data_len & 0xFF);
	
		ft5x06_i2c_write(client, w_buf, 4);

    /* program firmware */
	CTP_DEBUG("Step 5:program firmware\n");
    pkt_num = (data_len) / FT_FW_PKT_LEN;
    pkt_buf[0] = FT_FW_START_REG;
    pkt_buf[1] = 0x00;
    fw_ecc = 0;
	temp = 0;
    
    for (j = 0; j < pkt_num; j++)
    {
        temp = j * FT_FW_PKT_LEN;
        pkt_buf[2] = (u8) (temp >> 8);
        pkt_buf[3] = (u8) temp;
		pkt_len = FT_FW_PKT_LEN;
        pkt_buf[4] = (u8) (pkt_len >> 8);
        pkt_buf[5] = (u8) pkt_len;

        for (i = 0; i < FT_FW_PKT_LEN; i++)
        {
            pkt_buf[6 + i] = data[j * FT_FW_PKT_LEN + i];
            fw_ecc ^= pkt_buf[6 + i];
        }

        ft5x06_i2c_write(client, pkt_buf,FT_FW_PKT_LEN + 6);
		 
		for(i = 0;i < 30;i++)
		{
			w_buf[0] = 0x6a;
			r_buf[0] = r_buf[1] = 0x00;
			ft5x06_i2c_read(client, w_buf, 1, r_buf, 2);

			if ((j + 0x1000) == (((r_buf[0]) << 8) | r_buf[1]))
			{
				break;
			}
			msleep(1);
		}
    }

    /* send remaining bytes */
	CTP_DEBUG("Step 6:send remaining bytes\n");
    if ((data_len) % FT_FW_PKT_LEN > 0)
    {
        temp = pkt_num * FT_FW_PKT_LEN;
        pkt_buf[2] = (u8) (temp >> 8);
        pkt_buf[3] = (u8) temp;
        temp = (data_len) % FT_FW_PKT_LEN;
        pkt_buf[4] = (u8) (temp >> 8);
        pkt_buf[5] = (u8) temp;

        for (i = 0; i < temp; i++)
        {
            pkt_buf[6 + i] = data[pkt_num * FT_FW_PKT_LEN + i];
            fw_ecc ^= pkt_buf[6 + i];
        }

        ft5x06_i2c_write(client, pkt_buf, temp + 6);

		for(i = 0;i < 30;i++)
		{
			w_buf[0] = 0x6a;
			r_buf[0] = r_buf[1] = 0x00;
			ft5x06_i2c_read(client, w_buf, 1, r_buf, 2);

			if ((j + 0x1000) == (((r_buf[0]) << 8) | r_buf[1]))
			{
				break;
			}
			msleep(1);
		}
    }

	msleep(50);
						
		/*********Step 6: read out checksum***********************/
		/*send the opration head */
		CTP_DEBUG("Step 7: read out checksum\n");
		w_buf[0] = 0x64;
		ft5x06_i2c_write(client, w_buf, 1); 
		msleep(300);

		temp = 0;
		w_buf[0] = 0x65;
		w_buf[1] = (u8)(temp >> 16);
		w_buf[2] = (u8)(temp >> 8);
		w_buf[3] = (u8)(temp);
		temp = data_len;
		w_buf[4] = (u8)(temp >> 8);
		w_buf[5] = (u8)(temp);
		i_ret = ft5x06_i2c_write(client, w_buf, 6); 
		msleep(data_len/256);

		for(i = 0;i < 100;i++)
		{
		w_buf[0] = 0x6a;
		r_buf[0] = r_buf[1] = 0x00;
		ft5x06_i2c_read(client, w_buf, 1, r_buf, 2);

		if (0xF0==r_buf[0] && 0x55==r_buf[1])
		{
		break;
		}
		msleep(1);

		}
		w_buf[0] = 0x66;
		ft5x06_i2c_read(client, w_buf, 1, r_buf, 1);
		if (r_buf[0] != fw_ecc) 
		{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
		r_buf[0],
		fw_ecc);

		return -EIO;
		}

    /* reset */
	CTP_DEBUG("Step 8: reset \n");
    w_buf[0] = FT_REG_RESET_FW;
    ft5x06_i2c_write(client, w_buf, 1);
    msleep(ts_data->pdata->soft_rst_dly);

    CTP_DEBUG("Firmware upgrade successful\n");

    return 0;
}

static void fts_get_upgrade_array(struct i2c_client *client)
{

	u8 chip_id;
	u32 i;
	int ret = 0;

	ret = ft5x0x_read_reg(client, FT_REG_ID, &chip_id);
	if (ret<0) 
	{
		CTP_ERROR("[Focal][Touch] read value fail");
	}

	for(i=0;i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info);i++)
	{
		if(chip_id==fts_updateinfo[i].CHIP_ID)
		{
			memcpy(&fts_updateinfo_curr, &fts_updateinfo[i], sizeof(struct Upgrade_Info));
			break;
		}
	}

        CTP_DEBUG("fts_updateinfo:%d\n",i);
	if(i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info))
	{
		memcpy(&fts_updateinfo_curr, &fts_updateinfo[12], sizeof(struct Upgrade_Info));
	}
}

#if TPD_AUTO_UPGRADE

static unsigned char CTPM_FW1[]= //TCL
{
	#include "ft_app_ic_tcl.txt"
};

static unsigned char CTPM_FW2[]= //TIANMA
{
	#include "ft_app_ic_tianma.txt"
};
static unsigned char CTPM_FW3[]= //INX
{
	#include "ft_app_ic_inx.txt"
};
static unsigned char CTPM_FW4[]= //sharp
{
	#include "ft_app_ic_sharp.txt"
};

static unsigned char aucFW_PRAM_BOOT[] = {
	#include "FT8607_Pramboot_V0.1_20160606.txt"
};

int fts_ctpm_get_i_file_ver(u8 vendor_id)
{
	u16 ui_sz;
	if(vendor_id == VENDOR_TCL)
		ui_sz = sizeof(CTPM_FW1);
	else if(vendor_id == VENDOR_TIANMA)
		ui_sz = sizeof(CTPM_FW2);
	else if(vendor_id == VENDOR_INX)
		ui_sz = sizeof(CTPM_FW3);
	else if(vendor_id == VENDOR_SHARP)
		ui_sz = sizeof(CTPM_FW4);
	else
		goto exit;
	if (ui_sz > 2)
	{
		if(vendor_id == VENDOR_TCL)
			return CTPM_FW1[0x110E];//FT8607
		else if(vendor_id == VENDOR_TIANMA)
			return CTPM_FW2[0x110E];//FT8607
		else if(vendor_id == VENDOR_INX)
			return CTPM_FW3[0x110E];//FT8607
		else if(vendor_id == VENDOR_SHARP)
			return CTPM_FW4[0x110E];//FT8607
		else
			goto exit;
	}

exit:
	return 0x00;
}
static int fts_ctpm_fw_upgrade_with_i_file(struct ft5x06_ts_data *data)
{
    struct i2c_client *client = data->client;
    int  flag_TPID=0;
    u8*     pbt_buf = 0x0;
    int rc = 0,fw_len = 0;
    u8 uc_host_fm_ver,uc_tp_fm_ver,vendor_id, ic_type;
    u8 reg_addr;

    reg_addr = 0xA6;
    ft5x06_i2c_read(client, &reg_addr, 1, &uc_tp_fm_ver, 1);
    reg_addr = 0xA8;
    ft5x06_i2c_read(client, &reg_addr, 1, &vendor_id, 1);
    reg_addr = 0xA3;
    ft5x06_i2c_read(client, &reg_addr, 1, &ic_type, 1);

    CTP_DEBUG("Vendor ID:0x%02X, TP FW:0x%02X, IC TYPE:%d", vendor_id,uc_tp_fm_ver,ic_type);
/*
    if((ic_type != CTP_IC_TYPE_0) && (ic_type != CTP_IC_TYPE_1))
    {
        CTP_ERROR("IC type dismatch, please check");
    }
*/	
   // if(vendor_id == 0xA8 || vendor_id == 0x00 || ic_type == 0xA3 || ic_type == 0x00)
    if(((vendor_id ==0x01)&&(uc_tp_fm_ver == 0x01)&&(ic_type == 0x01))|| ( (vendor_id != VENDOR_TCL ) &&  (vendor_id != VENDOR_TIANMA) && (vendor_id != VENDOR_INX ) &&  (vendor_id != VENDOR_SHARP)))
    {
	  if(strstr(panelid, "tcl") != NULL)
	  {
		vendor_id = VENDOR_TCL;
	  }
	  else if(strstr(panelid, "tianma") != NULL)
	  {
		vendor_id = VENDOR_TIANMA;
	  }
	  else if(strstr(panelid, "inx") != NULL)
	  {
		vendor_id = VENDOR_INX;
	  }
	   else if(strstr(panelid, "sharp") != NULL)
	  {
		vendor_id = VENDOR_SHARP;
	  }
	  else
	  {
		CTP_ERROR("read vendor_id fail");
		return -1;
	  }
        CTP_ERROR("vend_id read error,need project");
        //vendor_id = fts_8607_read_vendor_id_from_bootloader(client);
	//fts_ctpm_read_lockdown(client,data);
	uc_tp_fm_ver = 0x00;
	is_ic_update_crash= 1;
        flag_TPID = 1;
    }
	
    if(vendor_id == VENDOR_TCL) //TCL
    {
        pbt_buf = CTPM_FW1;
        fw_len = sizeof(CTPM_FW1);
        CTP_DEBUG("TCL");
    }
    else if(vendor_id == VENDOR_TIANMA) //VENDOR_TIANMA
    {
	 pbt_buf = CTPM_FW2;
	 fw_len = sizeof(CTPM_FW2);
	 CTP_DEBUG("TIANMA");
     }
     else if(vendor_id == VENDOR_INX) //VENDOR_TIANMA
    {
	 pbt_buf = CTPM_FW3;
	 fw_len = sizeof(CTPM_FW3);
	 CTP_DEBUG("INX");
     }
      else if(vendor_id == VENDOR_SHARP) //VENDOR_TIANMA
    {
	 pbt_buf = CTPM_FW4;
	 fw_len = sizeof(CTPM_FW4);
	 CTP_DEBUG("SHARP");
     }
    else
    {
        CTP_ERROR("read vendor_id fail");
        return -1;
    }
	
    CTP_DEBUG("update firmware size:%d", fw_len);	

       uc_host_fm_ver = fts_ctpm_get_i_file_ver(vendor_id);
        CTP_DEBUG("[FTS] uc_tp_fm_ver = %d.\n", uc_tp_fm_ver);
        CTP_DEBUG("[FTS] uc_host_fm_ver = %d.\n", uc_host_fm_ver);

        if((uc_tp_fm_ver < uc_host_fm_ver)||(is_ic_update_crash==1))
        {
		rc = fts_8607_writepram(update_client, aucFW_PRAM_BOOT, sizeof(aucFW_PRAM_BOOT));
		if (rc != 0)
		{
			#ifdef CONFIG_HUAWEI_DSM
				dsm_client_record(touch_dclient,"touch  writepram failed rc = %d\n", rc);
				dsm_client_notify(touch_dclient, DSM_TP_FWUPDATE_ERROR_NO);
			#endif
			dev_err(&client->dev, "%s: [FTS]  writepram failed. err.\n",__func__);
			return -EIO;
		}

	       rc =  fts_8607_ctpm_fw_upgrade(update_client, pbt_buf, fw_len);
	       if (rc != 0)
		{
			#ifdef CONFIG_HUAWEI_DSM
				dsm_client_record(touch_dclient,"touch firmware upgrade failed rc = %d\n", rc);
				dsm_client_notify(touch_dclient, DSM_TP_FWUPDATE_ERROR_NO);
			#endif
			dev_err(&client->dev, "[FTS] upgrade failed. rc=%d.\n", rc);
		}
		else
		{
			#ifdef AUTO_CLB
			fts_ctpm_auto_clb(client);  /*start auto CLB*/
			#endif
		}
	}
    return rc;
}
#endif

#if CTP_SYS_APK_UPDATE
static ssize_t ft5x06_fw_name_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    u8 fw_version = 0x00;
    //struct ft5x06_ts_data *data = dev_get_drvdata(dev);
    ft5x0x_read_reg(update_client, FT5x0x_REG_FW_VER, &fw_version);

    return sprintf(buf, "firmware version %02X\n", fw_version);
}

static ssize_t ft5x06_fw_name_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t size)
{
    struct ft5x06_ts_data *data = dev_get_drvdata(dev);

    if (size > FT_FW_NAME_MAX_LEN - 1)
        return -EINVAL;

    strlcpy(data->fw_name, buf, size);
    if (data->fw_name[size-1] == '\n')
        data->fw_name[size-1] = 0;

    return size;
}

static DEVICE_ATTR(fw_name, 0664, ft5x06_fw_name_show, ft5x06_fw_name_store);

static int ft5x06_auto_cal(struct i2c_client *client)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(client);
	u8 temp = 0, i;

	/* set to factory mode */
	msleep(2 * data->pdata->soft_rst_dly);
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* start calibration */
	ft5x0x_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_START);
	msleep(2 * data->pdata->soft_rst_dly);
	for (i = 0; i < FT_CAL_RETRY; i++) {
		ft5x0x_read_reg(client, FT_REG_CAL, &temp);
		/*return to normal mode, calibration finish */
		if (((temp & FT_CAL_MASK) >> FT_4BIT_SHIFT) == FT_CAL_FIN)
			break;
	}

	/*calibration OK */
	msleep(2 * data->pdata->soft_rst_dly);
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* store calibration data */
	ft5x0x_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_STORE);
	msleep(2 * data->pdata->soft_rst_dly);

	/* set to normal mode */
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_WORKMODE_VALUE);
	msleep(2 * data->pdata->soft_rst_dly);

	return 0;
}


static int ft5x06_fw_upgrade(struct device *dev, bool force)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	int rc;
	u8 fw_file_maj, fw_file_min, fw_file_sub_min, fw_file_vendor_id;
	bool fw_upgrade = false;

	if (data->suspended) {
		dev_err(dev, "Device is in suspend state: Exit FW upgrade\n");
		return -EBUSY;
	}

	rc = request_firmware(&fw, data->fw_name, dev);
	if (rc < 0) {
		dev_err(dev, "Request firmware failed - %s (%d)\n",
						data->fw_name, rc);
		return rc;
	}

	if (fw->size < FT_FW_MIN_SIZE || fw->size > FT_FW_MAX_SIZE) {
		dev_err(dev, "Invalid firmware size (%zu)\n", fw->size);
		rc = -EIO;
		goto rel_fw;
	}

	if (data->family_id == FT6X36_ID) {
		fw_file_maj = FT_FW_FILE_MAJ_VER_FT6X36(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID_FT6X36(fw);
	} else {
		fw_file_maj = FT_FW_FILE_MAJ_VER(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID(fw);
	}
	fw_file_min = FT_FW_FILE_MIN_VER(fw);
	fw_file_sub_min = FT_FW_FILE_SUB_MIN_VER(fw);

	dev_info(dev, "Current firmware: %d.%d.%d", data->fw_ver[0],
				data->fw_ver[1], data->fw_ver[2]);
	dev_info(dev, "New firmware: %d.%d.%d", fw_file_maj,
				fw_file_min, fw_file_sub_min);

	if (force)
		fw_upgrade = true;
	else if ((data->fw_ver[0] < fw_file_maj) &&
		data->fw_vendor_id == fw_file_vendor_id)
		fw_upgrade = true;

	if (!fw_upgrade) {
		dev_info(dev, "Exiting fw upgrade...\n");
		rc = -EFAULT;
		goto rel_fw;
	}

	/* start firmware upgrade */
	if (FT_FW_CHECK(fw, data)) {
		rc = ft5x06_fw_upgrade_start(data->client, fw->data, fw->size);
		if (rc < 0)
			dev_err(dev, "update failed (%d). try later...\n", rc);
		else if (data->pdata->info.auto_cal)
			ft5x06_auto_cal(data->client);
	} else {
		dev_err(dev, "FW format error\n");
		rc = -EIO;
	}

	ft5x06_update_fw_ver(data);

	FT_STORE_TS_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);
rel_fw:
	release_firmware(fw);
	return rc;
}


static ssize_t ft5x06_update_fw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	return snprintf(buf, 2, "%d\n", data->loading_fw);
}

static ssize_t ft5x06_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	if (data->suspended) {
		dev_info(dev, "In suspend state, try again later...\n");
		return size;
	}

	mutex_lock(&data->input_dev->mutex);
	if (!data->loading_fw  && val) {
		data->loading_fw = true;
		ft5x06_fw_upgrade(dev, false);
		data->loading_fw = false;
	}
	mutex_unlock(&data->input_dev->mutex);

	return size;
}

static DEVICE_ATTR(update_fw, 0664, ft5x06_update_fw_show,
				ft5x06_update_fw_store);

static ssize_t ft5x06_force_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	mutex_lock(&data->input_dev->mutex);
	if (!data->loading_fw  && val) {
		data->loading_fw = true;
		ft5x06_fw_upgrade(dev, true);
		data->loading_fw = false;
	}
	mutex_unlock(&data->input_dev->mutex);

	return size;
}

static DEVICE_ATTR(force_update_fw, 0664, ft5x06_update_fw_show,
				ft5x06_force_update_fw_store);


#define FT_DEBUG_DIR_NAME	"ts_debug"

static bool ft5x06_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0xFF) {
		pr_err("FT reg address is invalid: 0x%x\n", addr);
		return false;
	}

	return true;
}

static int ft5x06_debug_data_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr))
		dev_info(&data->client->dev,
			"Writing into FT registers not supported\n");

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5x06_debug_data_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;
	int rc;
	u8 reg;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr)) {
		rc = ft5x0x_read_reg(data->client, data->addr, &reg);
		if (rc < 0)
			dev_err(&data->client->dev,
				"FT read register 0x%x failed (%d)\n",
				data->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, ft5x06_debug_data_get,
			ft5x06_debug_data_set, "0x%02llX\n");

static int ft5x06_debug_addr_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	if (ft5x06_debug_addr_is_valid(val)) {
		mutex_lock(&data->input_dev->mutex);
		data->addr = val;
		mutex_unlock(&data->input_dev->mutex);
	}

	return 0;
}

static int ft5x06_debug_addr_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr))
		*val = data->addr;

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, ft5x06_debug_addr_get,
			ft5x06_debug_addr_set, "0x%02llX\n");

static int ft5x06_debug_suspend_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		ft5x06_ts_suspend(&data->client->dev);
	else
		ft5x06_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5x06_debug_suspend_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, ft5x06_debug_suspend_get,
			ft5x06_debug_suspend_set, "%lld\n");

static int ft5x06_debug_dump_info(struct seq_file *m, void *v)
{
	struct ft5x06_ts_data *data = m->private;

	seq_printf(m, "%s\n", data->ts_info);

	return 0;
}

static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ft5x06_debug_dump_info, inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};

#endif

static int ft5x0x_GetFirmwareSize(char * firmware_name)
{
    struct file* pfile = NULL;
    struct inode *inode;
    unsigned long magic;
    off_t fsize = 0;
    char filepath[128];
    memset(filepath, 0, sizeof(filepath));

    sprintf(filepath, "%s", firmware_name);
    CTP_ERROR("filepath=%s\n", filepath);
    if(NULL == pfile)
    {
        pfile = filp_open(filepath, O_RDONLY, 0);
    }
    if(IS_ERR(pfile))
    {
        CTP_ERROR("error occured while opening file %s.\n", filepath);
        return -1;
    }
    inode=pfile->f_dentry->d_inode;
    magic=inode->i_sb->s_magic;
    fsize=inode->i_size;
    filp_close(pfile, NULL);
    return fsize;
}

static int ft5x0x_ReadFirmware(char * firmware_name, unsigned char * firmware_buf)
{
    struct file* pfile = NULL;
    struct inode *inode;
    unsigned long magic;
    off_t fsize;
    char filepath[128];
    loff_t pos;
    mm_segment_t old_fs;

    memset(filepath, 0, sizeof(filepath));
    sprintf(filepath, "%s", firmware_name);
    CTP_INFO("filepath=%s\n", filepath);
    if(NULL == pfile)
    {
        pfile = filp_open(filepath, O_RDONLY, 0);
    }
    if(IS_ERR(pfile))
    {
        CTP_ERROR("error occured while opening file %s.\n", filepath);
        return -1;
    }
    inode=pfile->f_dentry->d_inode;
    magic=inode->i_sb->s_magic;
    fsize=inode->i_size;
    //char * buf;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;

    vfs_read(pfile, firmware_buf, fsize, &pos);

    filp_close(pfile, NULL);
    set_fs(old_fs);
    return 0;
}

static void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}

static int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(update_client,0, 0x40);
    delay_qt_ms(100);                       //make sure already enter factory mode
    ft5x0x_write_reg(update_client,2, 0x4);               //write command to start calibration
    delay_qt_ms(300);
    for(i=0; i<100; i++)
    {
        ft5x0x_read_reg(update_client,0,&uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0)    //return to normal mode, calibration finish
        {
            break;
        }
        delay_qt_ms(20);
        printk("[FTS] waiting calibration %d\n",i);
    }

    printk("[FTS] calibration OK.\n");

    ft5x0x_write_reg(update_client,0, 0x40);              //goto factory mode
    delay_qt_ms(200);                       //make sure already enter factory mode
    ft5x0x_write_reg(update_client,2, 0x5);               //store CLB result
    delay_qt_ms(300);
    ft5x0x_write_reg(update_client,0, 0x0);               //return to normal mode
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}


static int fts_ctpm_fw_upgrade_with_app_file(char * firmware_name)
{
    unsigned char* pbt_buf = NULL;
    int i_ret;
    u8 fwver;
    int fwsize = ft5x0x_GetFirmwareSize(firmware_name);

    CTP_DEBUG("enter fw_upgrade_with_app_file");
    if(fwsize <= 0)
    {
        CTP_ERROR("%s ERROR:Get firmware size failed\n", __FUNCTION__);
        return -1;
    }
    //=========FW upgrade========================*/
    pbt_buf = (unsigned char *) kmalloc(fwsize+1,GFP_ATOMIC);
    if(ft5x0x_ReadFirmware(firmware_name, pbt_buf))
    {
        CTP_ERROR("%s() - ERROR: request_firmware failed\n", __FUNCTION__);
        kfree(pbt_buf);
        return -1;
    }


	//check if the firmware is correct
    if ((pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]) != 0xFF
	    || (pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]) != 0xFF
	    || (pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]) != 0xFF)
    {
    	CTP_ERROR("the update file is not correct, please check\n");
	CTP_ERROR("checksum is %2x,%2x,%2x,%2x,%2x,%2x", pbt_buf[fwsize - 8],pbt_buf[fwsize - 6],pbt_buf[fwsize - 7],pbt_buf[fwsize - 5],pbt_buf[fwsize - 3],pbt_buf[fwsize - 4]);
	return -1;
    }
    /*call the upgrade function*/
    i_ret =  ft5x06_fw_upgrade_start(update_client,pbt_buf, fwsize);
    if (i_ret != 0)
    {
        CTP_ERROR("%s() - ERROR:[FTS] upgrade failed i_ret = %d.\n",__FUNCTION__,  i_ret);
        //error handling ...
        //TBD
    }
    else
    {
        CTP_INFO("[FTS] upgrade successfully.\n");
        if(ft5x0x_read_reg(update_client, FT5x0x_REG_FW_VER, &fwver)>=0)
            CTP_INFO("the new fw ver is 0x%02x\n", fwver);
        //fts_ctpm_auto_clb();  //start auto CLB
    }
    kfree(pbt_buf);
    return i_ret;
}

#if CTP_SYS_APK_UPDATE

static ssize_t ft5x0x_fwupgradeapp_show(struct device *dev,
                                        struct device_attribute *attr, char *buf)
{
    /* place holder for future use */
    return -EPERM;
}
#endif

#if  CTP_SYS_APK_UPDATE
//upgrade from app.bin
static ssize_t ft5x0x_fwupgradeapp_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    //struct ft5x06_ts_data *data = dev_get_drvdata(dev);

    struct i2c_client *client = container_of(dev, struct i2c_client, dev);

    char fwname[128];
    memset(fwname, 0, sizeof(fwname));
    sprintf(fwname, "%s", buf);
    fwname[count-1] = '\0';

    disable_irq(client->irq);

    fts_ctpm_fw_upgrade_with_app_file(fwname);

    enable_irq(client->irq);

    return count;
}

#endif

#if CTP_SYS_APK_UPDATE

static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO|S_IWUSR, ft5x0x_fwupgradeapp_show, ft5x0x_fwupgradeapp_store);
#endif

#ifdef CONFIG_OF
static int ft5x06_get_dt_coords(struct device *dev, char *name,
                                struct ft5x06_ts_platform_data *pdata)
{
    u32 coords[FT_COORDS_ARR_SIZE];
    struct property *prop;
    struct device_node *np = dev->of_node;
    int coords_size, rc;

    prop = of_find_property(np, name, NULL);
    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENODATA;

    coords_size = prop->length / sizeof(u32);
    if (coords_size != FT_COORDS_ARR_SIZE)
    {
        dev_err(dev, "invalid %s\n", name);
        return -EINVAL;
    }

    rc = of_property_read_u32_array(np, name, coords, coords_size);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read %s\n", name);
        return rc;
    }

    if (!strcmp(name, "ftech,panel-coords"))
    {
        pdata->panel_minx = coords[0];
        pdata->panel_miny = coords[1];
        pdata->panel_maxx = coords[2];
        pdata->panel_maxy = coords[3];
    }
    else if (!strcmp(name, "ftech,display-coords"))
    {
        pdata->x_min = coords[0];
        pdata->y_min = coords[1];
        pdata->x_max = coords[2];
        pdata->y_max = coords[3];
    }
    else
    {
        dev_err(dev, "unsupported property %s\n", name);
        return -EINVAL;
    }

    return 0;
}

static int ft5x06_parse_dt(struct device *dev,
                           struct ft5x06_ts_platform_data *pdata)
{
    int rc;
    struct device_node *np = dev->of_node;
    struct property *prop;
    u32 temp_val, num_buttons;
    u32 button_map[MAX_BUTTONS];

    pdata->name = "ftech";
    rc = of_property_read_string(np, "ftech,name", &pdata->name);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read name\n");
        return rc;
    }

    rc = ft5x06_get_dt_coords(dev, "ftech,panel-coords", pdata);
    if (rc && (rc != -EINVAL))
        return rc;

    rc = ft5x06_get_dt_coords(dev, "ftech,display-coords", pdata);
    if (rc)
        return rc;

	
    pdata->i2c_pull_up = of_property_read_bool(np,
                         "ftech,i2c-pull-up");

    pdata->no_force_update = of_property_read_bool(np,
                             "ftech,no-force-update");
    /* reset, irq gpio info */
    pdata->reset_gpio = of_get_named_gpio_flags(np, "ftech,reset-gpio",
                        0, &pdata->reset_gpio_flags);
    if (pdata->reset_gpio < 0)
        return pdata->reset_gpio;

    pdata->irq_gpio = of_get_named_gpio_flags(np, "ftech,irq-gpio",
                      0, &pdata->irq_gpio_flags);
    if (pdata->irq_gpio < 0)
        return pdata->irq_gpio;

    pdata->fw_name = "ft_fw.bin";
    rc = of_property_read_string(np, "ftech,fw-name", &pdata->fw_name);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw name\n");
        return rc;
    }

    rc = of_property_read_u32(np, "ftech,group-id", &temp_val);
    if (!rc)
        pdata->group_id = temp_val;
    else
        return rc;

    rc = of_property_read_u32(np, "ftech,hard-reset-delay-ms",
                              &temp_val);
    if (!rc)
        pdata->hard_rst_dly = temp_val;
    else
        return rc;

    rc = of_property_read_u32(np, "ftech,soft-reset-delay-ms",
                              &temp_val);
    if (!rc)
        pdata->soft_rst_dly = temp_val;
    else
        return rc;

    rc = of_property_read_u32(np, "ftech,num-max-touches", &temp_val);
    if (!rc)
	pdata->num_max_touches = temp_val;
    else
        return rc;

    rc = of_property_read_u32(np, "ftech,fw-delay-aa-ms", &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw delay aa\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.delay_aa =  temp_val;

    rc = of_property_read_u32(np, "ftech,fw-delay-55-ms", &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw delay 55\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.delay_55 =  temp_val;

    rc = of_property_read_u32(np, "ftech,fw-upgrade-id1", &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw upgrade id1\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.upgrade_id_1 =  temp_val;

    rc = of_property_read_u32(np, "ftech,fw-upgrade-id2", &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw upgrade id2\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.upgrade_id_2 =  temp_val;

    rc = of_property_read_u32(np, "ftech,fw-delay-readid-ms",
                              &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw delay read id\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.delay_readid =  temp_val;

    rc = of_property_read_u32(np, "ftech,fw-delay-era-flsh-ms",
                              &temp_val);
    if (rc && (rc != -EINVAL))
    {
        dev_err(dev, "Unable to read fw delay erase flash\n");
        return rc;
    }
    else if (rc != -EINVAL)
        pdata->info.delay_erase_flash =  temp_val;

    pdata->info.auto_cal = of_property_read_bool(np,
                           "ftech,fw-auto-cal");

    pdata->fw_vkey_support = of_property_read_bool(np,
                             "ftech,fw-vkey-support");

    pdata->ignore_id_check = of_property_read_bool(np,
                             "ftech,ignore-id-check");

    rc = of_property_read_u32(np, "ftech,family-id", &temp_val);
    if (!rc)
        pdata->family_id = temp_val;
    else
        return rc;

    prop = of_find_property(np, "ftech,button-map", NULL);
    if (prop)
    {
        num_buttons = prop->length / sizeof(temp_val);
        if (num_buttons > MAX_BUTTONS)
            return -EINVAL;

        rc = of_property_read_u32_array(np,
                                        "ftech,button-map", button_map,
                                        num_buttons);
        if (rc)
        {
            dev_err(dev, "Unable to read key codes\n");
            return rc;
        }
    }

    return 0;
}
#else
static int ft5x06_parse_dt(struct device *dev,
                           struct ft5x06_ts_platform_data *pdata)
{
    return -ENODEV;
}
#endif

#if CTP_PROC_INTERFACE
static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
    char *ptr = buf;
    u8 result= 0;
    int nret = -1;
    struct i2c_client *client = fts_i2c_client;
    ssize_t num_read_chars = 0;

	if(*ppos)
	{
	    CTP_INFO("tp test again return\n");
	    return 0;
	}
	*ppos += count;

	mutex_lock(&fts_mutex);
	disable_irq(client->irq);

	nret = fts_open_short_test( FTS_INI_FILE_NAME, NULL, &num_read_chars);
	if(0 != nret)
	{
		result = 0;
		dev_err(&client->dev, "%s: fts open short test fail \n", __func__);
	}
	else
	{
		result = 1;
		dev_err(&client->dev, "%s: fts open short test success \n", __func__);
	}

	enable_irq(client->irq);
	mutex_unlock(&fts_mutex);

	dev_err(&client->dev, "%s: End Open-Short Test \n", __func__);
    return  sprintf(ptr, "result=%d\n",result);
}

/*
static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
    char *ptr = buf;
    u8 result= 0;
    int nret = -1;
    struct i2c_client *client = fts_i2c_client;
    ssize_t num_read_chars = 0;

	if(*ppos)
	{
	    CTP_INFO("tp test again return\n");
	    return 0;
	}
	*ppos += count;

	mutex_lock(&fts_mutex);
	disable_irq(client->irq);
	nret = fts_open_short_test( FTS_INI_FILE_NAME, NULL, &num_read_chars);
	if(0 != nret)
	{
		result = 0;
		dev_err(&client->dev, "%s: fts open short test fail \n", __func__);
	}
	else
	{
		result = 1;
		dev_err(&client->dev, "%s: fts open short test success \n", __func__);
	}

	enable_irq(client->irq);
	mutex_unlock(&fts_mutex);
	dev_err(&client->dev, "%s: End Open-Short Test \n", __func__);

    return  sprintf(ptr, "result=%d\n",result);
}*/

static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
{
	return -1;
}

static void create_ctp_proc(void)
{
    //----------------------------------------
    //create read/write interface for tp information
    //the path is :proc/touchscreen
    //child node is :version
    //----------------------------------------
    struct proc_dir_entry *ctp_device_proc = NULL;
    struct proc_dir_entry *ctp_open_proc = NULL;
   
    ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
    if(ctp_device_proc == NULL)
    {
        CTP_ERROR("ft5x06: create parent_proc fail\n");
        return;
    }

    ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0644, ctp_device_proc, &ctp_open_procs_fops);
    if (ctp_open_proc == NULL)
    {
        CTP_ERROR("ft5x06: create open_proc fail\n");
    }
}
#endif

#if  WT_ADD_CTP_INFO
static int hardwareinfo_set(struct ft5x06_ts_data *data,u8 value_name)
{
	char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
	char vendor_for_id[HARDWARE_MAX_ITEM_LONGTH];
	char ic_name[HARDWARE_MAX_ITEM_LONGTH];
	int err;

	if (data->fw_vendor_id == VENDOR_O_FILM)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"OUFEI");
	else if (data->fw_vendor_id == VENDOR_Lens)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"LENS");
	else if (data->fw_vendor_id == VENDOR_TCL)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"TCL");
	else 
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"Other vendor");

        snprintf(ic_name,HARDWARE_MAX_ITEM_LONGTH,"FT8607");
	snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"%s,%s,FW:0x%x",vendor_for_id,ic_name,data->fw_ver[0]);
        err = hardwareinfo_set_prop(HARDWARE_TP,firmware_ver);
        if (err < 0)
		return -1;

	return 0;
}
#endif

#define HARDWARE_MAX_ITEM_LONGTH 32
static int ctp_app_info(struct ft5x06_ts_data *data,u8 value_name)
{
	char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
	char vendor_for_id[HARDWARE_MAX_ITEM_LONGTH];
	char ic_name[HARDWARE_MAX_ITEM_LONGTH];
	int err;

	if (data->fw_vendor_id == VENDOR_O_FILM)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"OUFEI");
	else if (data->fw_vendor_id == VENDOR_Lens)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"LENS");
	else if (data->fw_vendor_id == VENDOR_TCL)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"TCL");
	else if (data->fw_vendor_id == VENDOR_TIANMA)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"TIANMA");
	else if (data->fw_vendor_id == VENDOR_INX)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"INX");
	else if (data->fw_vendor_id == VENDOR_SHARP)
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"SHARP");
	else
		snprintf(vendor_for_id,HARDWARE_MAX_ITEM_LONGTH,"Other vendor");

        snprintf(ic_name,HARDWARE_MAX_ITEM_LONGTH,"FT8607");
	snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"%s,%s,FW:0x%x",vendor_for_id,ic_name,data->fw_ver[0]);
       err = app_info_set("touch_panel", firmware_ver);
        if (err < 0)
		return -1;

	return 0;
}

#if FTS_PROC_APK_DEBUG
static int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
static int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}

static ssize_t ft5x0x_debug_write(struct file *filp, const char __user *buff,size_t len, loff_t *ppos)
{
	struct i2c_client *client = update_client;
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = len;
	int writelen = 0;
	int ret = 0;

	if(*ppos)
		return -1;
	
	if (copy_from_user(&writebuf, buff, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];
	CTP_INFO("write mode %x", proc_operate_mode);
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			
			disable_irq(client->irq);

			ret = fts_ctpm_fw_upgrade_with_app_file(upgrade_file_path);

			enable_irq(client->irq);
			if (ret < 0) {
				dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
				return ret;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_RAWDATA:
		break;
	case PROC_AUTOCLB:
		fts_ctpm_auto_clb();
		break;
	default:
		break;
	}
	
	*ppos += len;
	return len;
}

static unsigned char debug_read_buf[PAGE_SIZE];

/*interface of read proc*/
static ssize_t ft5x0x_debug_read(struct file *file, char __user *user_buf,size_t count, loff_t *ppos)
{
	struct i2c_client *client = update_client;
	int ret = 0;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;

	if(*ppos)
		return -1;
	
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling ft5x0x_debug_write to upgrade*/
		regaddr = 0xA6;
		ret = ft5x0x_read_reg(client, regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(debug_read_buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(debug_read_buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = ft5x0x_i2c_Read(client, NULL, 0, debug_read_buf, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		} else
//			CTP_ERROR("%s:value=0x%02x,count %d\n", __func__, debug_read_buf[0], count);
		num_read_chars = 1;
		break;
	case PROC_RAWDATA:
		break;
	default:
		break;
	}
	
	memcpy(user_buf, debug_read_buf, num_read_chars);
	*ppos += num_read_chars;
	return num_read_chars;
}

static const struct file_operations ctp_apk_proc_fops =
{
    .write = ft5x0x_debug_write,
    .read = ft5x0x_debug_read,
    .open = simple_open,
    .owner = THIS_MODULE,
};

static int ft5x0x_create_apk_debug_channel(struct i2c_client * client)
{
	ft5x0x_proc_entry = proc_create(PROC_NAME, 0666, NULL, &ctp_apk_proc_fops);
	if (NULL == ft5x0x_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	} 
	return 0;
}

static void ft5x0x_release_apk_debug_channel(void)
{
	if (ft5x0x_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
}

#endif

static int get_boot_mode(struct i2c_client *client)
{
        int ret;
        char *cmdline_tp=NULL;
        char *temp;
	char cmd_line[15]={'\0'};

        cmdline_tp = strstr(saved_command_line, "androidboot.mode=");
        if(cmdline_tp != NULL)
        {
                temp = cmdline_tp + strlen("androidboot.mode=");
                ret = strncmp(temp, "ffbm", strlen("ffbm"));
		memcpy(cmd_line, temp, 10);
                dev_err(&client->dev, "cmd_line =%s \n", cmd_line);
                if(ret==0)
                {
                        dev_err(&client->dev, "mode: ffbm\n");
                        return 1;/* factory mode*/
                }
		else
		{
		        dev_err(&client->dev, "mode: no ffbm\n");
                        return 2;/* no factory mode*/
		}
        }
       dev_err(&client->dev, "has no androidboot.mode \n");
       return 0;
}

#ifdef WT_CTP_GESTURE_SUPPORT

#define EASYWAKUP_PATH  	"touchscreen"
#define EASY_WAKEUP_GEASTURE_NAME 	"easy_wakeup_gesture"
#define EASY_WAKEUP_POSITION_NAME	"easy_wakeup_position"
static struct kobject *touch_screen_kobject_ts = NULL;
#define GESTURE_FROM_APP(_x) (_x)

 static ssize_t easy_wakeup_gesture_show(struct kobject *dev,
		struct kobj_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(gesture_client);
	ssize_t ret;

	mutex_lock(&data->sysfs_mutex);

	printk("%s: ft_easy_wakeup_gesture=0x%04x \n", __func__, ft_easy_wakeup_gesture);
	ret = snprintf(buf, PAGE_SIZE, "0x%04x\n",ft_easy_wakeup_gesture);

	mutex_unlock(&data->sysfs_mutex);
	return ret;
}

static ssize_t easy_wakeup_gesture_store(struct kobject *dev,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(gesture_client);
	unsigned long value = 0;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0){
		dev_err(&gesture_client->dev,"%s: kstrtoul error,ret=%d \n", __func__,ret);
		return ret;
	}

	printk("%s: value=0x%04x \n", __func__, (unsigned int)value);
	if (value > 0xFFFF) {
		dev_err(&gesture_client->dev,"%s: Invalid value:%ld \n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&data->sysfs_mutex);

	if (ft_easy_wakeup_gesture != ((u32)GESTURE_FROM_APP(value)))
		ft_easy_wakeup_gesture = (u32)GESTURE_FROM_APP(value);
	dev_err(&gesture_client->dev,"%s: ft_easy_wakeup_gesture=%d \n", __func__, ft_easy_wakeup_gesture);

	mutex_unlock(&data->sysfs_mutex);

	return size;
}
static ssize_t easy_wakeup_position_show(struct kobject *dev,
		struct kobj_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(gesture_client);
	ssize_t ret;
	char temp[300] = {0};
	short pointnum = 0;
	int i = 0;
	buf[0] = 0xd3;
	ft5x06_i2c_read(gesture_client, buf, 1, buf, FTS_GESTRUE_POINTS_HEADER);
	pointnum = (short)(buf[1]) & 0xff;
	mutex_lock(&data->sysfs_mutex);
	if(ft_gesture_onoff()){
		for(i = 0;i < 6;i ++)
		{
			ret = sprintf(temp, "%s%04x%04x", temp, coordinate_x[i], coordinate_y[i]);
		}
	}
	sprintf(buf, temp);
	printk("easy_wakeup_position_show: buf = %s\n", buf);
	mutex_unlock(&data->sysfs_mutex);
	return ret;
}
static ssize_t easy_wakeup_position_store(struct kobject *dev,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(gesture_client);
	if (!data){
		CTP_ERROR("device is null \n");
		return -EINVAL;
	}
	return 0;
}

static struct kobj_attribute easy_wakeup_gesture = {
	.attr = {.name = EASY_WAKEUP_GEASTURE_NAME, .mode = (S_IRUSR | S_IWUSR | S_IRGRP |S_IWGRP |S_IROTH | S_IWOTH)},
	.show = easy_wakeup_gesture_show,
	.store = easy_wakeup_gesture_store,
};
static struct kobj_attribute easy_wakeup_position = {
	.attr = {.name = EASY_WAKEUP_POSITION_NAME, .mode = (S_IRUSR | S_IWUSR | S_IRGRP |S_IWGRP |S_IROTH)},
	.show = easy_wakeup_position_show,
	.store = easy_wakeup_position_store,
};

static struct kobject* tp_get_touch_screen_obj(void)
{
	if( NULL == touch_screen_kobject_ts )
	{
		touch_screen_kobject_ts = kobject_create_and_add(EASYWAKUP_PATH, NULL);
		if (!touch_screen_kobject_ts)
		{
			dev_err(&gesture_client->dev,"%s: create sys/touchscreen kobjetct error!\n", __func__);
			return NULL;
		}
		else
		{
			dev_err(&gesture_client->dev,"%s: create sys/touchscreen/ successful!\n", __func__);
		}
	}
	else
	{
		dev_err(&gesture_client->dev,"%s: sys/touchscreen/ already exist!\n", __func__);
	}

	return touch_screen_kobject_ts;
}


static int add_easy_wakeup_interfaces(struct device *dev)
{
	int error = 0;
	struct kobject *properties_kobj;

	properties_kobj = tp_get_touch_screen_obj();
	if( NULL == properties_kobj )
	{
		dev_err(&gesture_client->dev,"%s: Error, get kobj failed!\n", __func__);
		return -1;
	}

	/*add the node easy_wakeup_gesture apk to write*/
	error = sysfs_create_file(properties_kobj, &easy_wakeup_gesture.attr);
	if (error)
	{
		kobject_put(properties_kobj);
		dev_err(&gesture_client->dev,"%s: easy_wakeup_gesture create file error\n", __func__);
		return -ENODEV;
	}
	/*add the node easy_wakeup_position for apk to write*/
	error = sysfs_create_file(properties_kobj, &easy_wakeup_position.attr);
	if (error)
	{
		kobject_put(properties_kobj);
		dev_err(&gesture_client->dev,"%s: easy_wakeup_position create file error\n", __func__);
		return -ENODEV;
	}

	return 0;
}
#endif

static int __init early_parse_panelid(char *cmdline)
{
	if (cmdline) {
		strlcpy(panelid, cmdline, sizeof(panelid));
	}
	return 0;
}
early_param("mdss_mdp.panel", early_parse_panelid);
static int ft5x06_ts_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct ft5x06_ts_platform_data *pdata;
    struct ft5x06_ts_data *data;
    struct input_dev *input_dev;
	struct dentry *temp;
    u8 reg_value;
    u8 reg_addr;
	int err;
	int len;
    u8 ic_name;
#if TPD_AUTO_UPGRADE
    int ret_auto_upgrade = 0;
    int i;
#endif

if(strstr(panelid, "ft8607") == NULL)
{
	dev_err(&client->dev, "panel id not matched\n");
	return -1;
}
	temp = NULL;
    update_client = client;

#if WT_CTP_GESTURE_SUPPORT
    gesture_client = client;
#endif

#if CTP_PROC_INTERFACE
	fts_i2c_client = client;
#endif

#if defined (CONFIG_HUAWEI_DSM)
    if(!touch_dclient)
    {
        touch_dclient = dsm_register_client(&dsm_touch_ft5346);
    }
#endif

    if (client->dev.of_node)
    {
        pdata = devm_kzalloc(&client->dev,
                             sizeof(struct ft5x06_ts_platform_data), GFP_KERNEL);
        if (!pdata)
        {
            dev_err(&client->dev, "Failed to allocate memory\n");
            return -ENOMEM;
        }

        err = ft5x06_parse_dt(&client->dev, pdata);
        if (err)
        {
            dev_err(&client->dev, "DT parsing failed\n");
            return err;
        }
    }
    else
        pdata = client->dev.platform_data;

    if (!pdata)
    {
        dev_err(&client->dev, "Invalid pdata\n");
        return -EINVAL;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        dev_err(&client->dev, "I2C not supported\n");
        return -ENODEV;
    }

    data = devm_kzalloc(&client->dev,
                        sizeof(struct ft5x06_ts_data), GFP_KERNEL);
    if (!data)
    {
        dev_err(&client->dev, "Not enough memory\n");
        return -ENOMEM;
    }

    if (pdata->fw_name)
    {
        len = strlen(pdata->fw_name);
        if (len > FT_FW_NAME_MAX_LEN - 1)
        {
            dev_err(&client->dev, "Invalid firmware name\n");
            return -EINVAL;
        }

        strlcpy(data->fw_name, pdata->fw_name, len + 1);
    }

    data->tch_data_len = FT_TCH_LEN(pdata->num_max_touches);
    data->tch_data = devm_kzalloc(&client->dev,
                                  data->tch_data_len, GFP_KERNEL);
    if (!data)
    {
        return -ENOMEM;
    }

    input_dev = input_allocate_device();
    if (!input_dev)
    {
        return -ENOMEM;
    }

    data->input_dev = input_dev;
    data->client = client;
    data->pdata = pdata;

    input_dev->name = "ft5346";
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &client->dev;

    input_set_drvdata(input_dev, data);
    i2c_set_clientdata(client, data);

    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if WT_CTP_GESTURE_SUPPORT
	mutex_init(&data->sysfs_mutex);
	input_set_capability(input_dev, EV_KEY, KEY_F1);
	input_set_capability(input_dev, EV_KEY, KEY_F8);
	input_set_capability(input_dev, EV_KEY, KEY_F9);
	input_set_capability(input_dev, EV_KEY, KEY_F10);
	input_set_capability(input_dev, EV_KEY, KEY_F11);
	input_set_capability(input_dev, EV_KEY, KEY_POWER);

	__set_bit(KEY_F1, input_dev->keybit);
	__set_bit(KEY_F8, input_dev->keybit);
	__set_bit(KEY_F9, input_dev->keybit);
	__set_bit(KEY_F10, input_dev->keybit);
	__set_bit(KEY_F11, input_dev->keybit);
	__set_bit(KEY_POWER, input_dev->keybit);

#endif

    input_mt_init_slots(input_dev, pdata->num_max_touches, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min,
                         pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min,
                         pdata->y_max, 0, 0);
	
    err = input_register_device(input_dev);
    if (err)
    {
        dev_err(&client->dev, "Input device registration failed\n");
        goto free_inputdev;
    }

    if (pdata->power_init)
    {
        err = pdata->power_init(true);
        if (err)
        {
            dev_err(&client->dev, "power init failed");
            goto unreg_inputdev;
        }
    }
    else
    {
        err = ft5x06_power_init(data, true);
        if (err)
        {
            dev_err(&client->dev, "power init failed");
            goto unreg_inputdev;
        }
    }

    if (pdata->power_on)
    {
        err = pdata->power_on(true);
        if (err)
        {
            dev_err(&client->dev, "power on failed");
            goto pwr_deinit;
        }
    }
    else
    {
        err = ft5x06_power_on(data, true);
        if (err)
        {
            dev_err(&client->dev, "power on failed");
            goto pwr_deinit;
        }
    }
/*
    err = ft5x06_ts_pinctrl_init(data);
    if (!err && data->ts_pinctrl)
    {
        err = ft5x06_ts_pinctrl_select(data, true);
        if (err < 0)
            goto pwr_off;
    }
*/
    if (gpio_is_valid(pdata->irq_gpio))
    {
        err = gpio_request(pdata->irq_gpio, "ft5x06_irq_gpio");
        if (err)
        {
            dev_err(&client->dev, "irq gpio request failed");
            goto pwr_off;
        }
        err = gpio_direction_input(pdata->irq_gpio);
        if (err)
        {
            dev_err(&client->dev,
                    "set_direction for irq gpio failed\n");
            goto free_irq_gpio;
        }
    }

    if (gpio_is_valid(pdata->reset_gpio))
    {
        err = gpio_request(pdata->reset_gpio, "ft5x06_reset_gpio");
        if (err)
        {
            goto free_irq_gpio;
        }

        err = gpio_direction_output(pdata->reset_gpio, 0);
        if (err)
        {
            dev_err(&client->dev,
                    "set_direction for reset gpio failed\n");
            goto free_reset_gpio;
        }
        msleep(data->pdata->hard_rst_dly);
        gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
    }

    /* make sure CTP already finish startup process */
    msleep(data->pdata->soft_rst_dly);

    /* check the controller id */
    reg_addr = FT_REG_ID;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
    if (err < 0)
    {
        dev_err(&client->dev, "version read failed");
        goto free_reset_gpio;
    }
    ic_name = reg_value;

    dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);

    if ((pdata->family_id != reg_value) && (!pdata->ignore_id_check))
    {
        dev_err(&client->dev, "%s:Unsupported controller\n", __func__);
        goto free_reset_gpio;
    }

    data->family_id = pdata->family_id;

    fts_get_upgrade_array(client);

    err = request_threaded_irq(client->irq, NULL,
                               ft5x06_ts_interrupt,
                               /*pdata->irq_gpio_flags */IRQF_TRIGGER_FALLING | IRQF_ONESHOT ,
                               client->dev.driver->name, data);
    if (err)
    {
        dev_err(&client->dev, "request irq failed\n");
        goto free_reset_gpio;
    }

    disable_irq(data->client->irq);
	
#if CTP_SYS_APK_UPDATE
    err = device_create_file(&client->dev, &dev_attr_fw_name);
    if (err)
    {
        dev_err(&client->dev, "sys file creation failed\n");
    }

    err = device_create_file(&client->dev, &dev_attr_ftsfwupgradeapp);
    if (err)
    {
        dev_err(&client->dev, "upgradeapp sys file creation failed\n");
    }

	err = device_create_file(&client->dev, &dev_attr_ftsmcaptest);
    if (err)
    {
        dev_err(&client->dev, "ftsmcaptest sys file creation failed\n");
    }
	err = device_create_file(&client->dev, &dev_attr_update_fw);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
	}

	err = device_create_file(&client->dev, &dev_attr_force_update_fw);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
	}

	data->dir = debugfs_create_dir(FT_DEBUG_DIR_NAME, NULL);
	if (data->dir == NULL || IS_ERR(data->dir)) {
		pr_err("debugfs_create_dir failed(%ld)\n", PTR_ERR(data->dir));
		err = PTR_ERR(data->dir);
	}
	else
	{
		temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, data->dir, data,
					   &debug_addr_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
			err = PTR_ERR(temp);
		}

		temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, data->dir, data,
					   &debug_data_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
			err = PTR_ERR(temp);
		}

		temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, data->dir,
						data, &debug_suspend_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
			err = PTR_ERR(temp);
		}

		temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR, data->dir,
						data, &debug_dump_info_fops);
		if (temp == NULL || IS_ERR(temp)) {
			pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
			err = PTR_ERR(temp);
		}
	}
#endif
    
    data->ts_info = devm_kzalloc(&client->dev,
                                 FT_INFO_MAX_LEN, GFP_KERNEL);
    if (!data->ts_info)
    {
        dev_err(&client->dev, "Not enough memory\n");
        goto free_irq_gpio;
    }

    /*get some register information */
    reg_addr = FT_REG_POINT_RATE;
    ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
    if (err < 0)
        dev_err(&client->dev, "report rate read failed");

    dev_info(&client->dev, "report rate = %dHz\n", reg_value * 10);

    reg_addr = FT_REG_THGROUP;
    err = ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
    if (err < 0)
        dev_err(&client->dev, "threshold read failed");

    dev_dbg(&client->dev, "touch threshold = %d\n", reg_value * 4);

    ft5x06_update_fw_ver(data);
    ft5x06_update_fw_vendor_id(data);

    FT_STORE_TS_INFO(data->ts_info, data->family_id, data->pdata->name,
                     data->pdata->num_max_touches, data->pdata->group_id,
                     data->pdata->fw_vkey_support ? "yes" : "no",
                     data->pdata->fw_name, data->fw_ver[0],
                     data->fw_ver[1], data->fw_ver[2]);

#if  WT_ADD_CTP_INFO
  err = hardwareinfo_set(data,ic_name);
  if (err < 0)
        dev_err(&client->dev, "hardwareinfo set failed");
  #endif 

  ctp_app_info(data,ic_name);

#if defined(CONFIG_FB)
	INIT_WORK(&data->fb_notify_work, fb_notify_resume_work);
    data->fb_notif.notifier_call = fb_notifier_callback;

    err = fb_register_client(&data->fb_notif);

    if (err)
        dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
                err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
                                FT_SUSPEND_LEVEL;
    data->early_suspend.suspend = ft5x06_ts_early_suspend;
    data->early_suspend.resume = ft5x06_ts_late_resume;
    register_early_suspend(&data->early_suspend);
#endif

#if WT_CTP_GESTURE_SUPPORT
   add_easy_wakeup_interfaces(&input_dev->dev);
#endif

#if CTP_PROC_INTERFACE
    create_ctp_proc();
#endif

#if FTS_PROC_APK_DEBUG
	ft5x0x_create_apk_debug_channel(update_client);
#endif


#if TPD_AUTO_UPGRADE
    err = get_boot_mode(client);
    if(err == 0)
    {
        CTP_DEBUG("********************Enter CTP Auto Upgrade********************\n");
        i = 0;
        do
        {
            ret_auto_upgrade = fts_ctpm_fw_upgrade_with_i_file(data);
            i++;
            if(ret_auto_upgrade < 0)
            {
                CTP_DEBUG(" ctp upgrade fail err = %d \n",ret_auto_upgrade);
            }
        }
        while((ret_auto_upgrade < 0)&&(i<3));
    }
    else//factory mode
    {
        dev_err(&client->dev, "no upgrade\n");
    }
#endif

#if CTP_CHARGER_DETECT
	batt_psy = power_supply_get_by_name("usb");
	if (!batt_psy)
		CTP_DEBUG("tp battery supply not found\n");
#endif

	enable_irq(data->client->irq);

    return 0;


free_reset_gpio:
   if (gpio_is_valid(pdata->reset_gpio))
        gpio_free(pdata->reset_gpio);
    if (data->ts_pinctrl)
    {
        err = ft5x06_ts_pinctrl_select(data, false);
        if (err < 0)
            CTP_ERROR("Cannot get idle pinctrl state\n");
    }
free_irq_gpio:
    if (gpio_is_valid(pdata->irq_gpio))
        gpio_free(pdata->irq_gpio);
    if (data->ts_pinctrl)
    {
        err = ft5x06_ts_pinctrl_select(data, false);
        if (err < 0)
            CTP_ERROR("Cannot get idle pinctrl state\n");
    }
pwr_off:
   /* if (pdata->power_on)
        pdata->power_on(false);
    else
        ft5x06_power_on(data, false);*/
pwr_deinit:
   /* if (pdata->power_init)
        pdata->power_init(false);
    else
        ft5x06_power_init(data, false);*/
unreg_inputdev:
    input_unregister_device(input_dev);
    input_dev = NULL;
free_inputdev:
    input_free_device(input_dev);
    return err;
}

static int ft5x06_ts_remove(struct i2c_client *client)
{
    struct ft5x06_ts_data *data = i2c_get_clientdata(client);
    int retval;

#if CTP_SYS_APK_UPDATE
    device_remove_file(&client->dev, &dev_attr_fw_name);
#endif

#if FTS_PROC_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
#endif

#ifdef  CONFIG_HUAWEI_DSM
	dsm_unregister_client(touch_dclient,&dsm_touch_ft5346);
#endif

#if defined(CONFIG_FB)
    if (fb_unregister_client(&data->fb_notif))
        dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&data->early_suspend);
#endif
    free_irq(client->irq, data);

    if (gpio_is_valid(data->pdata->reset_gpio))
        gpio_free(data->pdata->reset_gpio);

    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);

    if (data->ts_pinctrl)
    {
        retval = ft5x06_ts_pinctrl_select(data, false);
        if (retval < 0)
            CTP_ERROR("Cannot get idle pinctrl state\n");
    }
/*
    if (data->pdata->power_on)
        data->pdata->power_on(false);
    else
        ft5x06_power_on(data, false);

    if (data->pdata->power_init)
        data->pdata->power_init(false);
    else
        ft5x06_power_init(data, false);
*/
    input_unregister_device(data->input_dev);

    return 0;
}

static const struct i2c_device_id ft5x06_ts_id[] =
{
    {"ft5x06_720p", 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

#ifdef CONFIG_OF
static struct of_device_id ft5x06_match_table[] =
{
    { .compatible = "focaltech,5336",},
    { },
};
#else
#define ft5x06_match_table NULL
#endif

static struct i2c_driver ft5x06_ts_driver =
{
    .probe = ft5x06_ts_probe,
    .remove = ft5x06_ts_remove,
    .driver = {
        .name = "ft5x06_720p",
        .owner = THIS_MODULE,
        .of_match_table = ft5x06_match_table,
#ifdef CONFIG_PM
        .pm = &ft5x06_ts_pm_ops,
#endif
    },
    .id_table = ft5x06_ts_id,
};

static int __init ft5x06_ts_init(void)
{
    return i2c_add_driver(&ft5x06_ts_driver);
}
module_init(ft5x06_ts_init);

static void __exit ft5x06_ts_exit(void)
{
    i2c_del_driver(&ft5x06_ts_driver);
}
module_exit(ft5x06_ts_exit);

MODULE_DESCRIPTION("FocalTech ft5x06 TouchScreen driver");
MODULE_LICENSE("GPL v2");

