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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <mt-plat/mtk_boot.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>

static struct mutex keyboard_led_lock;
struct regulator *keyboard_led;
static struct workqueue_struct *kb_led_workq;
static int power_flag = 0;
struct keyboard_led_work {
    struct delayed_work dwork;
    int ops;
};

static void do_kb_led_work(struct work_struct *data)
{
	int count = 6;
	int ret;
	unsigned int isenable;

    struct keyboard_led_work *work =
        container_of(data, struct keyboard_led_work, dwork.work);
    bool led_on = (work->ops == 1 ? true : false);

	/* Avoid turning on and off the screen quickly. 
	 * If the bright screen timer has not been completed, 
	 * the original timer will be powered on immediately 
	 * after the screen is turned off, resulting in 
	 * incorrect LED status.Therefore, the status of the 
	 * current FB must prevail.
	 */
	if (led_on && power_flag) {
		if(regulator_enable(keyboard_led))
			pr_err("keyboard_led enable fail\n");
		pr_err("keyboard_led_on\n");
	} else {
    /* check regulator status*/
    isenable = regulator_is_enabled(keyboard_led);

    pr_debug("Keyboard led: query regulator enable status[%d]\n", isenable);

    if (isenable) {
        while(count-- && isenable)
        {
            ret = regulator_disable(keyboard_led);
            isenable = regulator_is_enabled(keyboard_led);
        }
        if (ret != 0) {
            pr_err("Keyboard led :failed to disable Vcama1: %d\n", ret);
            return ;
        }
        /* verify */
        isenable = regulator_is_enabled(keyboard_led);
        if (!isenable)
            pr_err("Keyboard led: regulator disable successfully\n");
    }

		pr_debug("keyboard_led_off\n");
	}
    /* free kfree */
    kfree(work);
}

static void keyboard_led_power_work(int ops, int delay)
{
    struct keyboard_led_work *work;

    /* create and prepare worker */
    work = kzalloc(sizeof(struct keyboard_led_work), GFP_ATOMIC);
    if (!work) {
        pr_err("keyboard led work is NULL, directly return\n");
        return;
    }
    work->ops = ops;
    INIT_DELAYED_WORK(&work->dwork, do_kb_led_work);

    /* keyboard led power work */
    pr_err("keyboard led work, ops<%d>, delay<%d>\n", ops, delay);

    queue_delayed_work(kb_led_workq,
                &work->dwork, msecs_to_jiffies(delay));
}

static void kb_led_on(int delay)
{
    pr_err("keyboard led power on\n");
    keyboard_led_power_work(1, delay);
}

static void kb_led_off(int delay)
{
    pr_err("keyboard led power off\n");
    keyboard_led_power_work(0, delay);
}

static int kb_led_event_handler(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = NULL;
	int blank;
    
	evdata = data;
					   /*Power on only in normal mode,else bypass*/
	if (event != FB_EVENT_BLANK || (get_boot_mode() != NORMAL_BOOT))
		return 0;

	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
            mutex_lock(&keyboard_led_lock);
			power_flag = 1;
			kb_led_on(3000);
		    pr_info("%s()  trig kb_led on!\n", __func__);
            mutex_unlock(&keyboard_led_lock);
        break;
	case FB_BLANK_POWERDOWN:
            mutex_lock(&keyboard_led_lock);
			power_flag = 0;
			kb_led_off(0);
		    pr_info("%s()  trig kb_led off!\n", __func__);
            mutex_unlock(&keyboard_led_lock);
        break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block kb_led_notifier_func = {
	.notifier_call = kb_led_event_handler,
	.priority = 0,
};

static int keyboard_led_probe(struct platform_device *pdev)
{
	int ret = 0;

    ret = fb_register_client(&kb_led_notifier_func);
    if (ret) {
        pr_err("Failed to register kb led notifier.\n");
    }

	keyboard_led = devm_regulator_get(&pdev->dev, "keyboard-led");
	if (IS_ERR(keyboard_led))
		dev_info(&pdev->dev, "keyboard_led 3v0 error\n");
	else {
		ret = regulator_set_voltage(keyboard_led, 3000000, 3000000);
		if (ret < 0)
			dev_info(&pdev->dev, "keyboard_led 3v0 enable fail\n");
	}

	kb_led_workq = create_singlethread_workqueue("keyboard_led_power");
    mutex_init(&keyboard_led_lock);
	
	dev_info(&pdev->dev, "%s successfully\n",__func__);

	return 0;
}

static int keyboard_led_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id keyboard_led_of_match[] = {
	{.compatible = "mediatek,keyboard-led"},
	{},
};

static struct platform_driver keyboard_led_driver = {
	.probe = keyboard_led_probe,
	.remove = keyboard_led_remove,
	.driver = {
		.name = "keyboard_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(keyboard_led_of_match),
	},
};

static int __init keyboard_led_init(void)
{
	return platform_driver_register(&keyboard_led_driver);
}

static void __exit keyboard_led_cleanup(void)
{
	platform_driver_unregister(&keyboard_led_driver);
}

module_init(keyboard_led_init);
module_exit(keyboard_led_cleanup);

MODULE_AUTHOR("zhenlin.zhang");
MODULE_DESCRIPTION("Keyboard led timing for ML218");
MODULE_LICENSE("GPL");
