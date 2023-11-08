/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/alarmtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include "upmu_common.h"
#include "mtk_charger_intf.h"

#define GETARRAYNUM(array) (ARRAY_SIZE(array))

const unsigned int VBAT_CV_VTH[] = {
	8064000,8096000,8128000,8160000,8192000,8224000,8256000,
	8288000,8320000,8352000,8384000,8416000,8448000,8480000,
	8512000,8544000,8576000,8608000,8640000,8672000,8704000,
	8736000,8768000,8800000,8832000,8864000,8896000,8928000,
	8960000,8992000,9024000,9056000,9088000,9120000,9152000,
	9184000,9216000,9248000,9280000,9312000,9344000,9376000,
	9408000,9440000,9472000,9504000,9536000,9568000,9600000,
	9632000,9664000,9696000,9728000
};

const unsigned int VBAT_CV_VTH_TO_REG[] = {
	0x1F80,0x1FA0,0x1FC0,0x1FE0,0x2000,0x2020,
	0x2040,0x2060,0x2080,0x20A0,0x20C0,0x20E0,
	0x2100,0x2120,0x2140,0x2160,0x2180,0x21A0,
	0x21C0,0x21E0,0x2200,0x2220,0x2240,0x2260,
	0x2280,0x22A0,0x22C0,0x22E0,0x2300,0x2320,
	0x2340,0x2360,0x2380,0x23A0,0x23C0,0x23E0,
	0x2400,0x2420,0x2440,0x2460,0x2480,0x24A0,
	0x24C0,0x24E0,0x2500,0x2520,0x2540,0x2560,
	0x2580,0x25A0,0x25C0,0x25E0,0x2600
};

/*bq24715 REG04 ICHG[6:0]*/
const unsigned int CS_VTH[] = {
	0, 6400, 12800, 25600, 51200,
	76800, 102400, 128000, 153600,
	179200 ,198400, 204800, 230400,
	249600, 256000, 281600, 300800
};

/*bq24715 REG00 IINLIM[5:0]*/
const unsigned int INPUT_CS_VTH[] = {
	0, 6400, 12800, 25600, 51200,
	76800, 102400, 128000, 153600,
	179200, 198400, 204800, 230400,
	249600, 256000, 281600, 300800
};

const unsigned int CS_VTH_TO_REG[] = {
	0x0,0x40,0x80,0x100,
	0x200,0x300,0x400,0x500,
	0x600,0x700,0x7C0,0x800,
	0x900,0x9C0,0xA00,0xB00,
	0xBC0
};

#if 0
const unsigned int VCDT_HV_VTH[] = {
	4200000, 4250000, 4300000, 4350000,
	4400000, 4450000, 4500000, 4550000,
	4600000, 6000000, 6500000, 7000000,
	7500000, 8500000, 9500000, 10500000
};


const unsigned int VINDPM_REG[] = {
	3900, 4000, 4100, 4200, 4300, 4400,
	4500, 4600, 4700, 4800, 4900, 5000,
	5100, 5200, 5300, 5400, 5500, 5600,
	5700, 5800, 5900, 6000, 6100, 6200,
	6300, 6400
};

/* bq24715 REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 1200
};
#endif

unsigned int charging_value_to_parameter(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_info("Can't find the parameter\n");
	return parameter[0];

}

unsigned int charging_parameter_to_value(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
		unsigned int number,
		unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0;i--) 
		{/* max value in the last element */
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzl_%d<=%d, i=%d\n",
					pList[i], level, i);
				return pList[i];
			}
		}

		pr_info("Can't find closest level\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}

struct bq24715_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	const char *chg_dev_name;
	const char *eint_name;
	enum charger_type chg_type;
	int irq;
	uint32_t charge_option;
	uint32_t charge_current;
    uint32_t charge_voltage;
    uint32_t input_current;
	uint32_t min_system_volt;
	uint32_t gpio_charger_en_pin;
};

static struct bq24715_info *g_info;
static struct i2c_client *new_client;
static const struct i2c_device_id bq24715_i2c_id[] = { {"bq24715", 0}, {} };

static DEFINE_MUTEX(bq24715_i2c_access);
static DEFINE_MUTEX(bq24715_access_lock);

#define BQ24715_CHG_OPT			0x12
#define BQ24715_CHG_OPT_MASK	0xffff
#define BQ24715_CHG_OPT_CHARGE_DISABLE	(1 << 0)
#define BQ24715_CHG_OPT_AC_PRESENT	(1 << 4)
#define BQ24715_CHARGE_CURRENT		0x14
#define BQ24715_CHARGE_CURRENT_MASK	0x1fc0
#define BQ24715_CHARGE_VOLTAGE		0x15
#define BQ24715_CHARGE_VOLTAGE_MASK	0x7ff0
#define BQ24715_INPUT_CURRENT		0x3f
#define BQ24715_INPUT_CURRENT_MASK	0x1fc0
#define BQ24715_MIN_SYSTEM_VOLTAGE  0x3e
#define BQ24715_MIN_SYSTEM_VOLTAGE_MASK 0x3f00
#define BQ24715_MANUFACTURER_ID		0xfe
#define BQ24715_DEVICE_ID		0xff

static inline int bq24715_write_word(struct i2c_client *client, u8 reg,
				     u16 value)
{
	return i2c_smbus_write_word_data(client, reg, value);
}

static inline int bq24715_read_word(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_word_data(client, reg);
}

static int bq24715_update_word(struct i2c_client *client, u8 reg,
			       u16 mask, u16 value)
{
	unsigned int tmp;
	int ret;

	ret = bq24715_read_word(client, reg);
	if (ret < 0)
		return ret;

	tmp = ret & ~mask;
	tmp |= value & mask;

	return bq24715_write_word(client, reg, tmp);
}

static int bq24715_dump_register(struct charger_device *chg_dev)
{
	int value = 0,ret = 0;
	int manu_id = 0, device_id = 0, val_12 = 0,
		val_14 = 0, val_15 =0,val_3f = 0,val_3e = 0;

	if (g_info->charge_option) {
		value = g_info->charge_option & BQ24715_CHG_OPT_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_CHG_OPT, value);
		dev_err(&new_client->dev,
				"Go to write charger option : 0x%x\n",value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write charger option : 0x%x\n",
				ret);
			return ret;
		}
	}

	device_id = bq24715_read_word(new_client, BQ24715_DEVICE_ID);
	manu_id   = bq24715_read_word(new_client, BQ24715_MANUFACTURER_ID);
	val_12    = bq24715_read_word(new_client, BQ24715_CHG_OPT);
	val_14    = bq24715_read_word(new_client, BQ24715_CHARGE_CURRENT);
	val_15    = bq24715_read_word(new_client, BQ24715_CHARGE_VOLTAGE);
	val_3e    = bq24715_read_word(new_client, BQ24715_MIN_SYSTEM_VOLTAGE);
	val_3f    = bq24715_read_word(new_client, BQ24715_INPUT_CURRENT);

    pr_err("bq24715: manuid=0x%x deviceid=0x%x reg12=0x%x reg14=0x%x \
			reg15=0x%x reg3e=0x%x reg3f=0x%x\n", 
			manu_id,device_id,val_12,val_14,val_15,val_3e,val_3f);

	return 0;
}

static int bq24715_parse_dt(struct bq24715_info *info, struct device *dev)
{
	u32 val,ret;
	struct device_node *np = dev->of_node;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}

	info->gpio_charger_en_pin = of_get_named_gpio(np, "gpio_charger_en_pin", 0);
    if ((gpio_is_valid(info->gpio_charger_en_pin)))
    {
        pr_warn("%s get info->gpio_charger_en_pin = %d success\n",info->gpio_charger_en_pin - 332);
        ret = gpio_request_one(info->gpio_charger_en_pin, GPIOF_OUT_INIT_HIGH ,"charger_en_pin");
        if (ret < 0) {
            pr_warn("%s Unable to request gpio gpio_charger_en_pin\n");
            gpio_free(info->gpio_charger_en_pin);
        }
    }
    else
    {
        gpio_free(info->gpio_charger_en_pin);
        pr_warn("%s of get gpio_charger_en_pin failed\n");
    }

	if (of_property_read_string(np, "charger_name",
	   &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_warn("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name",
	   &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "bq24715";
		pr_warn("%s: no alias name\n", __func__);
	}

	ret = of_property_read_u32(np, "ti,charge-option", &val);
	if (!ret)
		info->charge_option = val;

	ret = of_property_read_u32(np, "ti,charge-current", &val);
	if (!ret)
		info->charge_current = val;

	ret = of_property_read_u32(np, "ti,charge-voltage", &val);
	if (!ret)
		info->charge_voltage = val;

	ret = of_property_read_u32(np, "ti,input-current", &val);
	if (!ret)
		info->input_current = val;

	ret = of_property_read_u32(np, "ti,min-system-volt", &val);
	if (!ret)
		info->min_system_volt = val;

	pr_err("%s charge-option=0x%x charge-current=0x%x charge-voltage=0x%d \
			input-currnt=0x%x min-system-volt=0x%x\n",
			__func__,info->charge_option,info->charge_current,info->charge_voltage,
			info->input_current,info->min_system_volt);

	return 0;
}

static int bq24715_config_charger(struct bq24715_info *charger)
{
	int ret;
	u16 value;

	if (g_info->charge_option) {
		value = g_info->charge_option & BQ24715_CHG_OPT_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_CHG_OPT, value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write charger option : 0x%x\n",
				ret);
			return ret;
		}
	}

	if (g_info->charge_current) {
		value = g_info->charge_current & BQ24715_CHARGE_CURRENT_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_CHARGE_CURRENT, value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write charger current : 0x%x\n",
				ret);
			return ret;
		}
	}

	if (g_info->charge_voltage) {
		value = g_info->charge_voltage & BQ24715_CHARGE_VOLTAGE_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_CHARGE_VOLTAGE, value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write charger voltage : 0x%x\n",
				ret);
			return ret;
		}
	}

	if (g_info->input_current) {
		value = g_info->input_current & BQ24715_INPUT_CURRENT_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_INPUT_CURRENT, value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write input current : 0x%x\n",
				ret);
			return ret;
		}
	}

	if (g_info->min_system_volt) {
		value = g_info->min_system_volt & BQ24715_MIN_SYSTEM_VOLTAGE_MASK;

		ret = bq24715_write_word(new_client,
					 BQ24715_MIN_SYSTEM_VOLTAGE, value);
		if (ret < 0) {
			dev_err(&new_client->dev,
				"Failed to write input current : 0x%x\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static int bq24715_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	int ret = 0;
	int value = 0;
	/*Set maximum delay between consecutive SMBus Write charge voltage 
	 * or charge current command. The charge will be suspended if IC does
	 * not receive write charge voltage or write charge current command
	 * within the watchdog time period and watchdog timer is enabled*/
	//ret = bq24715_config_charger(g_info);
	value = bq24715_read_word(new_client, BQ24715_INPUT_CURRENT);
	ret = bq24715_write_word(new_client,
					BQ24715_INPUT_CURRENT, value);
	if (ret < 0) {
		dev_err(&new_client->dev,
			"Failed to write charger current : 0x%x\n",
			ret);
		return ret;
	}

	return 0;
}

static inline int bq24715_enable_charging(struct bq24715_info *charger)
{
	//int ret;

	//ret = bq24715_config_charger(charger);
	//if (ret)
	//	return ret;
	/*reg 0x12 bit0 need to be set 0 if enable charging*/
	return bq24715_update_word(new_client, BQ24715_CHG_OPT,
				   BQ24715_CHG_OPT_CHARGE_DISABLE, 0); 
}

static inline int bq24715_disable_charging(struct bq24715_info *charger)
{
	/*Sending InputCurrent() below 128 mA or above 8.064 A are
	ignored. Upon POR, default input current limit is 3.2 A.*/
	bq24715_write_word(new_client,
					BQ24715_INPUT_CURRENT, 0x80/*128mA*/);
	bq24715_write_word(new_client,
					BQ24715_CHARGE_CURRENT, 0);
	/*reg 0x12 bit0 need to be set 1 if disable charging*/
	return bq24715_update_word(new_client, BQ24715_CHG_OPT,
				   BQ24715_CHG_OPT_CHARGE_DISABLE,
				   BQ24715_CHG_OPT_CHARGE_DISABLE);
}

static int bq24715_enable_charging2(struct charger_device *chg_dev, bool en)
{
	mutex_lock(&bq24715_access_lock);

	if (en)
		bq24715_enable_charging(g_info);
	else
		bq24715_disable_charging(g_info);

	mutex_unlock(&bq24715_access_lock);

	return 0;
}
////////////////////////////////////////////////////////////////////////////

static int bq24715_get_current(struct charger_device *chg_dev,
			       u32 *ichg)
{
	unsigned int ret_val = 0;
#if 0 //todo
	unsigned char ret_force_20pct = 0;

	/* Get current level */
	bq24715_read_interface(bq24715_CON2, &ret_val, CON2_ICHG_MASK,
			       CON2_ICHG_SHIFT);

	/* Get Force 20% option */
	bq24715_read_interface(bq24715_CON2, &ret_force_20pct,
			       CON2_FORCE_20PCT_MASK,
			       CON2_FORCE_20PCT_SHIFT);

	/* Parsing */
	ret_val = (ret_val * 64) + 512;

#endif
	return ret_val;
}

static int bq24715_set_current(struct charger_device *chg_dev,
			       u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int ret = 0;
	unsigned int value = 0,val_14 = 0;

	dump_stack();

	pr_info("&&&& charge_current_value = %d\n", current_value);
	current_value /= 10;
	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
			 set_chr_current);
	//pr_info("&&&& charge_register_value = %d\n",register_value);
	value = CS_VTH_TO_REG[register_value] & BQ24715_CHARGE_CURRENT_MASK;

	pr_info("&&&& value = %d\n",value);
	if (value >= (g_info->charge_current & BQ24715_CHARGE_CURRENT_MASK))
		value = (g_info->charge_current & BQ24715_CHARGE_CURRENT_MASK);

	pr_info("&&&& value = %d\n",value);
	ret = bq24715_write_word(new_client,
					BQ24715_CHARGE_CURRENT, value);
	if (ret < 0) {
		dev_err(&new_client->dev,
			"Failed to write charger current : 0x%x\n",
			ret);
		return ret;
	}

	val_14    = bq24715_read_word(new_client, BQ24715_CHARGE_CURRENT);

	pr_info("%s current_value = %d set_chr_current = %d CS_VTH_TO_REG[register_value] = 0x%x value = 0x%x val_14 = 0x%x\n",
			__func__, current_value, set_chr_current, CS_VTH_TO_REG[register_value], value,val_14);

	return status;
}

static int bq24715_get_input_current(struct charger_device *chg_dev,
				     u32 *aicr)
{
	int ret = 0;
#if 0
	unsigned char val = 0;

	bq24715_read_interface(bq24715_CON0, &val, CON0_IINLIM_MASK,
			       CON0_IINLIM_SHIFT);
	ret = (int)val;
	*aicr = INPUT_CS_VTH[val];
#endif
	return ret;
}

static int bq24715_set_input_current(struct charger_device *chg_dev,
				     u32 current_value)
{
	unsigned int status = true;
	unsigned int set_input_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int ret = 0;
	unsigned int value = 0,val_3f = 0;

	dump_stack();
	current_value /= 10;
	pr_info("&&&& current_value = %d\n", current_value);
	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_input_current = bmt_find_closest_level(INPUT_CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size,
			 set_input_current);

	value = CS_VTH_TO_REG[register_value] & BQ24715_INPUT_CURRENT_MASK;

	pr_info("&&&& value = %d\n",value);
	if (value >= (g_info->input_current & BQ24715_INPUT_CURRENT_MASK))
		value = g_info->input_current & BQ24715_INPUT_CURRENT_MASK;

	pr_info("&&&& value = %d\n",value);
	ret = bq24715_write_word(new_client,
					BQ24715_INPUT_CURRENT, value);
	if (ret < 0) {
		dev_err(&new_client->dev,
			"Failed to write charger current : 0x%x\n",
			ret);
		return ret;
	}
	val_3f = bq24715_read_word(new_client, BQ24715_INPUT_CURRENT);

	pr_info("%s current_value = %d set_input_current = %d CS_VTH_TO_REG[register_value] = 0x%x value = 0x%x value_3f =0x%x\n",
			__func__, current_value, set_input_current, CS_VTH_TO_REG[register_value],value,val_3f);

	return status;
}

static int bq24715_set_cv_voltage(struct charger_device *chg_dev,
				  u32 cv)
{
	unsigned int status = true;
	unsigned int array_size;
	unsigned int set_cv_voltage;
	unsigned short register_value;
	unsigned int ret = 0;
	unsigned int value = 0,val_15 = 0;

	dump_stack();
	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv);
	register_value = charging_parameter_to_value(VBAT_CV_VTH, array_size,
			 set_cv_voltage);

	value = VBAT_CV_VTH_TO_REG[register_value] & BQ24715_CHARGE_VOLTAGE_MASK;

	if (value >= (g_info->charge_voltage & BQ24715_CHARGE_VOLTAGE_MASK))
		value = g_info->charge_voltage & BQ24715_CHARGE_VOLTAGE_MASK;

	ret = bq24715_write_word(new_client,
					BQ24715_CHARGE_VOLTAGE, value);
	if (ret < 0) {
		dev_err(&new_client->dev,
			"Failed to write charger voltage : 0x%x\n",
			ret);
		return ret;
	}
	val_15    = bq24715_read_word(new_client, BQ24715_CHARGE_VOLTAGE);
	pr_info("%s cv = %d set_cv_voltage = %d VBAT_CV_VTH_TO_REG[register_value] = 0x%x value = 0x%x val_15=0x%x\n",
			__func__, cv, set_cv_voltage, VBAT_CV_VTH_TO_REG[register_value], value,val_15);

	return status;
}
#if 0
static int bq24715_set_vindpm_voltage(struct charger_device *chg_dev,
				      u32 vindpm)
{
	int status = 0;
	unsigned int array_size;

	vindpm /= 1000;
	array_size = ARRAY_SIZE(VINDPM_REG);
	vindpm = bmt_find_closest_level(VINDPM_REG, array_size, vindpm);
	vindpm = charging_parameter_to_value(VINDPM_REG, array_size, vindpm);

	pr_info("%s vindpm =%d\r\n", __func__, vindpm);

	//	charging_set_vindpm(vindpm);
	/*bq24715_set_en_hiz(en);*/

	return status;
}

static int bq24715_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	unsigned int status = true;
	unsigned int ret_val;

	ret_val = bq24715_get_chrg_stat();

	if (ret_val == 0x3)
		*is_done = true;
	else
		*is_done = false;

	return status;
}
#endif

static int bq24715_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}
////////////////////////////////////////////////////////////////
static struct charger_ops bq24715_chg_ops = {
	.dump_registers = bq24715_dump_register,
	.enable = bq24715_enable_charging2,
	.kick_wdt = bq24715_reset_watch_dog_timer,
	.get_charging_current = bq24715_get_current,
	.set_charging_current = bq24715_set_current,
	.get_input_current = bq24715_get_input_current,
	.set_input_current = bq24715_set_input_current,
	/*.get_constant_voltage = bq24715_get_battery_voreg,*/
	.set_constant_voltage = bq24715_set_cv_voltage,
	//.set_mivr = bq24715_set_vindpm_voltage,
	//.is_charging_done = bq24715_get_charging_status,

	.event = bq24715_do_event,
};

static ssize_t show_bq24715_access(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	int manu_id = 0, device_id = 0, val_12 = 0,
		val_14 = 0, val_15 =0,val_3f = 0,val_3e = 0;

	device_id = bq24715_read_word(new_client, BQ24715_DEVICE_ID);
	manu_id   = bq24715_read_word(new_client, BQ24715_MANUFACTURER_ID);
	val_12    = bq24715_read_word(new_client, BQ24715_CHG_OPT);
	val_14    = bq24715_read_word(new_client, BQ24715_CHARGE_CURRENT);
	val_15    = bq24715_read_word(new_client, BQ24715_CHARGE_VOLTAGE);
	val_3e    = bq24715_read_word(new_client, BQ24715_MIN_SYSTEM_VOLTAGE);
	val_3f    = bq24715_read_word(new_client, BQ24715_INPUT_CURRENT);

    return sprintf(buf, "manuid=0x%x deviceid=0x%x reg12=0x%x reg14=0x%x \
			reg15=0x%x reg3e=0x%x reg3f=0x%x\n", 
			manu_id,device_id,val_12,val_14,val_15,val_3e,val_3f);
}

static ssize_t write_bq24715_store(struct device *dev,struct device_attribute *attr,
		const char *buf, size_t size)
{
       int ret = 0;
       char *pvalue = NULL, *addr, *val;
       unsigned int reg_value = 0;
       unsigned int reg_address = 0;

       pr_info("[%s]\n", __func__);

       if (buf != NULL && size != 0) {
               pr_info("[%s] buf is %s and size is %zu\n", __func__, buf,
                       size);

               pvalue = (char *)buf;
               addr = strsep(&pvalue, " ");
               ret = kstrtou32(addr, 16,
                       (unsigned int *)&reg_address);

               val = strsep(&pvalue, " ");
               ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
               pr_info(
               "[%s] write bq24715 reg 0x%x with value 0x%x !\n",
               __func__,
               (unsigned int) reg_address, reg_value);
               ret = bq24715_write_word(new_client, reg_address, reg_value);
       }
       return size;
}

static ssize_t bq24715_enable(struct device *dev,struct device_attribute *attr,
		const char *buf, size_t size)
{
       int ret = 0;
       char *pvalue = NULL;
       unsigned int reg_address = 0;

       pr_info("[%s]\n", __func__);

       if (buf != NULL && size != 0) {
           pr_info("[%s] buf is %s and size is %zu\n", __func__, buf,
                   size);

           pvalue = (char *)buf;
           ret = kstrtou32(pvalue, 16,
                       (unsigned int *)&reg_address);
		if (reg_address == 1)
			bq24715_enable_charging(g_info);
		else
			bq24715_disable_charging(g_info);
       }
       return size;
}

static int get_hw_flag(void)
{
        int version = 0;
        char *br_ptr;

        br_ptr = strstr(saved_command_line, "androidboot.dram_mdl_index=");
        if (br_ptr != 0) {
                /* get hardware flag */
                version = br_ptr[27] - '0';
                pr_err("androidboot.dram_mdl_index=%d\n", version);
                if (version == 0)
                    return 0;
                else
                    return 1;
        } else {
                return 0;
        }

}

static DEVICE_ATTR(bq24715_access, 0664, show_bq24715_access, NULL);/* 664 */
static DEVICE_ATTR(bq24715_conf, 0664, NULL, write_bq24715_store);
static DEVICE_ATTR(bq24715_enable, 0664, NULL, bq24715_enable);

static int bq24715_driver_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret = 0;
	int manu_id = 0, device_id = 0;
	struct bq24715_info *info = NULL;

	dev_err(&client->dev,"client->addr=0x%x\n", client->addr);

	info = devm_kzalloc(&client->dev, sizeof(struct bq24715_info),GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "%s: devm_kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	new_client = client;
	info->dev = &client->dev;
	ret = bq24715_parse_dt(info, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "%s: parse dts info failed\n", __func__);
		return ret;
	}
	
	/*Enable 3v3 ldo to ensure i2c communicate successfully*/
	gpio_direction_output(info->gpio_charger_en_pin, 1);
	gpio_set_value(info->gpio_charger_en_pin, 1);

	/*Create debug interface*/
	device_create_file(&(client->dev),&dev_attr_bq24715_access);
	device_create_file(&(client->dev),&dev_attr_bq24715_conf);
	device_create_file(&(client->dev),&dev_attr_bq24715_enable);

	manu_id = bq24715_read_word(client, BQ24715_MANUFACTURER_ID);
	dev_err(&client->dev, "manufacturer id : 0x%x\n",manu_id);

	device_id = bq24715_read_word(client, BQ24715_DEVICE_ID);
	dev_err(&client->dev, "device id : 0x%x\n",device_id);

	if (manu_id != 0x40 && device_id != 0x10) {
		dev_err(&client->dev, "%s: No device Found!\n", __func__);
		return -ENODEV;
	}

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&client->dev, info, &bq24715_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		dev_err(&client->dev, "%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	g_info = info;

	ret = bq24715_config_charger(g_info);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id bq24715_of_match[] = {
	{.compatible = "ti,bq24715"},
	{},
};
#else
static struct i2c_board_info i2c_bq24715 __initdata = {
	I2C_BOARD_INFO("bq24715", (BQ24715_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver bq24715_driver = {
	.driver = {
		.name = "bq24715",
#ifdef CONFIG_OF
		.of_match_table = bq24715_of_match,
#endif
		},
	.probe = bq24715_driver_probe,
	.id_table = bq24715_i2c_id,
};

static int __init bq24715_init(void)
{
	if (get_hw_flag() != 0/*flag == 0, for v1.3, flag != 0,for hardware v1.2  or less*/) {
		pr_err("here is not the hardware v1.3, bypass%s\n", __func__);
		return -ENODEV;
	}

	if (i2c_add_driver(&bq24715_driver) != 0)
		pr_info("Failed to register bq24715 i2c driver.\n");
	else
		pr_info("Success to register bq24715 i2c driver.\n");

	return 0;
}

static void __exit bq24715_exit(void)
{
	i2c_del_driver(&bq24715_driver);
}

module_init(bq24715_init);
module_exit(bq24715_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24715 Driver");
MODULE_AUTHOR("Zhenlin Zhang<zhenlin.zhang@emdoor.com>");
