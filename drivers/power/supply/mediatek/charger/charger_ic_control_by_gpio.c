/*
 * Copyright (C) 2019 MediaTek Inc.
 *
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
#include <linux/types.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
//#include "upmu_common.h"
#include "charger_class.h"

#if 1
#define PRINT_CHARGER(fmt, args...)   
#else
#define PRINT_CHARGER(fmt, args...)   printk("[charger_gpio] " fmt,##args)
#endif

struct charger_ic_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct alarm otg_kthread_gtimer;
	struct workqueue_struct *otg_boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;

	const char *chg_dev_name;
	const char *eint_name;
	//enum charger_type chg_type;
	int irq;
	unsigned int gpio_charger_en_pin;
};
struct charger_ic_info *info = NULL;

#if 0
static int charger_gpio_get_charging_status(struct charger_device *chg_dev, bool *is_done)
{
	return 1;
}
#endif

static int charger_gpio_enable_charging(struct charger_device *chg_dev, bool en)
{
	
	gpio_direction_output(info->gpio_charger_en_pin, 1);
	
	if (en) 
		gpio_set_value(info->gpio_charger_en_pin, 0);
	else 
		gpio_set_value(info->gpio_charger_en_pin, 1);

	return 1;
}

static struct charger_ops charger_gpio_chg_ops = {

	/* Normal charging */
	.enable = charger_gpio_enable_charging,
	//.is_charging_done = charger_gpio_get_charging_status,
};

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

static int charger_gpio_driver_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	PRINT_CHARGER("%s : start\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct charger_ic_info),
			   GFP_KERNEL);

	if (!info)
		return -ENOMEM;

	if (!np) {
		PRINT_CHARGER("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",
	   &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_warn("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name",
	   &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "mp2615";
		pr_warn("%s: no alias name\n", __func__);
	}
	
	info->gpio_charger_en_pin = of_get_named_gpio(np, "gpio_charger_en_pin", 0);
	if ((gpio_is_valid(info->gpio_charger_en_pin)))
	{
		PRINT_CHARGER("get info->gpio_charger_en_pin = %d success\n",info->gpio_charger_en_pin - 332);
		ret = gpio_request_one(info->gpio_charger_en_pin, GPIOF_OUT_INIT_LOW ,"charger_en_pin");
		if (ret < 0) {
			PRINT_CHARGER("Unable to request gpio gpio_charger_en_pin\n");
			gpio_free(info->gpio_charger_en_pin);
		}
	}
	else
	{
		gpio_free(info->gpio_charger_en_pin);
		PRINT_CHARGER( "of get gpio_charger_en_pin failed\n");
	}

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&pdev->dev, info, &charger_gpio_chg_ops, &info->chg_props);

	if (IS_ERR_OR_NULL(info->chg_dev)) {
		PRINT_CHARGER("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}
	
	PRINT_CHARGER("%s : end\n");
	
	return 0;
}

static int charger_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id charger_gpio_of_match[] = {
	{.compatible = "mediatek,gpio-charger"},
	{},
};

static struct platform_driver charger_gpio_driver = {
	.probe		= charger_gpio_driver_probe,
	.remove		= charger_gpio_remove,
	.driver		= {
		.name	= "charger_gpio",
#ifdef CONFIG_OF
		 .of_match_table = charger_gpio_of_match,
#endif		
		.owner	= THIS_MODULE,
	},
};

static int __init charger_gpio_init(void)
{
	if (get_hw_flag() == 0/*flag == 0, for v1.3, flag != 0,for hardware v1.2  or less*/) {
        pr_err("here is the hardware v1.2, go into%s\n", __func__);
        return -ENODEV;
    }

	if (platform_driver_register(&charger_gpio_driver) != 0)
		PRINT_CHARGER("Failed to register charger_gpio driver.\n");
	else
		PRINT_CHARGER("Success to register charger_gpio driver.\n");

	return 0;
}

static void __exit charger_gpio_exit(void)
{
	platform_driver_unregister(&charger_gpio_driver);
}

module_init(charger_gpio_init);
module_exit(charger_gpio_exit);
MODULE_LICENSE("GPL");
