/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <extcon_usb.h>
#include <mt-plat/mtk_boot.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include "../../misc/mediatek/sensors-1.0/hall/lid-switch.h"
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>

#define RET_SUCCESS 0
#define RET_FAIL 1

static struct task_struct *thread1;
static struct task_struct *thread2;
static struct task_struct *thread3;
static struct task_struct *thread4;
static struct task_struct *thread5;

int display_power_down=0;
bool hdmi_in_use=0;
struct usb_iddig_info {
	struct device *dev;
	struct gpio_desc *id_gpiod;
	int id_irq;
	unsigned long id_swdebounce;
	unsigned long id_hwdebounce;
	struct delayed_work id_delaywork;
	struct pinctrl *pinctrl;
	struct pinctrl_state *id_init;
	struct pinctrl_state *id_enable;
	struct pinctrl_state *id_disable;
    //add for usb hub
    bool usb_switch_to_host_enable;
    struct pinctrl_state *usb_sel_high;
    struct pinctrl_state *usb_sel_low;
    struct pinctrl_state *hub_pwr_en_high;
    struct pinctrl_state *hub_pwr_en_low;
    struct pinctrl_state *host1_5v_en_high;
    struct pinctrl_state *host1_5v_en_low;
    struct pinctrl_state *host2_5v_en_high;
    struct pinctrl_state *host2_5v_en_low;
    struct pinctrl_state *cam_vcc_en_high;
    struct pinctrl_state *cam_vcc_en_low;
    //struct pinctrl_state *keyboard_db_ctrl_high;
    //struct pinctrl_state *keyboard_db_ctrl_low;
    //struct pinctrl_state *touch_3v3_en_high;
    //struct pinctrl_state *touch_3v3_en_low;
    struct pinctrl_state *hub_rst_high;
    struct pinctrl_state *hub_rst_low;
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
    struct wakeup_source otg_iddig_wakelock;
#endif
    //add end
};
static int usb_switch_to_host_flag = -1;
static int ui_switch_usb_to_host_en = 1;
static struct mutex usb_host_switch_lock;
static struct mutex usb_host_power_lock;
static struct mutex usb_id_status_switch_lock;
static struct usb_iddig_info *pm_info = NULL;
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
extern bool get_usbmouse_in_status(void);
int otg_iddig_power_suspend = -1;
#endif

enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;
static void usb_switch_to_host_en(struct usb_iddig_info *info, bool enable);
extern int get_vbus_det_pin_status(void);
#ifdef CONFIG_USB_JIOBOOK
static int wakelock_timeout_ms;
static struct dentry *sysfs_wakelock_dir;
static struct dentry *wakelock_value;
static int debugfs_usb_wakelock(void)
{
// Default - 3 minutes timeout

	wakelock_timeout_ms = 1000;

	sysfs_wakelock_dir = debugfs_create_dir("deepsleep_timeout", NULL);
	if (IS_ERR_OR_NULL(sysfs_wakelock_dir))
		return PTR_ERR(sysfs_wakelock_dir);

	wakelock_value = debugfs_create_u32("wakelock_timeout_ms", 0600, sysfs_wakelock_dir, &wakelock_timeout_ms);

	return 0;
}
#endif
//cat
static ssize_t usb_host_switch_show(struct device * dev, struct device_attribute * attr, char * buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", ui_switch_usb_to_host_en);
}

//echo
static ssize_t usb_host_switch_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    unsigned long v;
    int err;
    
    err = kstrtoul(buf, 10,  &v);
    if (err)
        return err;
    
    mutex_lock(&usb_host_switch_lock);
    if (ui_switch_usb_to_host_en != v)
        ui_switch_usb_to_host_en = v;
    mutex_unlock(&usb_host_switch_lock);
    
    //printk("%s: ui_switch_usb_to_host_en = %d\n", __func__, ui_switch_usb_to_host_en);
    
    if (1 == ui_switch_usb_to_host_en) //host
    {
        mt_usb_disconnect();
        mdelay(50);
        usb_switch_to_host_en(pm_info, 1);   
    }
    else if (0 == ui_switch_usb_to_host_en)
    {
        mt_usbhost_disconnect();
        mdelay(50);
        usb_switch_to_host_en(pm_info, 0);
        mdelay(200);//must 200ms
        //printk("%s: get_vbus_det_pin_status = %d\n", __func__, get_vbus_det_pin_status());
        if(get_vbus_det_pin_status() == 1)
            mt_usb_connect();
    }
    
    return size;
}
static DEVICE_ATTR(usb_host_switch, 0644, usb_host_switch_show, usb_host_switch_store);

int usb_swith_to_host(void)
{
    return usb_switch_to_host_flag;
}
EXPORT_SYMBOL_GPL(usb_swith_to_host);

static void host_power_enable(struct usb_iddig_info *info, bool enable)
{
    pr_debug("[usb-iddig]%s :  enable = %d\n", __func__, enable);
    if(enable)
    {
        if(!IS_ERR(info->hub_pwr_en_high))
            pinctrl_select_state(info->pinctrl, info->hub_pwr_en_high);
        mdelay(1);           
        if(!IS_ERR(info->hub_rst_high))
            pinctrl_select_state(info->pinctrl, info->hub_rst_high);
        mdelay(70);             
        if(!IS_ERR(info->host1_5v_en_high))
            pinctrl_select_state(info->pinctrl, info->host1_5v_en_high);
                    
       if(!IS_ERR(info->host2_5v_en_high))
            pinctrl_select_state(info->pinctrl, info->host2_5v_en_high);
                    
        if(!IS_ERR(info->cam_vcc_en_high))
            pinctrl_select_state(info->pinctrl, info->cam_vcc_en_high);
		
		if(!IS_ERR(info->usb_sel_high))
		{
           
			pinctrl_select_state(info->pinctrl, info->usb_sel_high);
		}
    }
    else
    {
		 pinctrl_select_state(info->pinctrl, info->usb_sel_low);
			
        if(!IS_ERR(info->hub_pwr_en_low))
            pinctrl_select_state(info->pinctrl, info->hub_pwr_en_low);      
        
        if(!IS_ERR(info->hub_rst_low))
            pinctrl_select_state(info->pinctrl, info->hub_rst_low);
        
        if(!IS_ERR(info->host1_5v_en_low))
            pinctrl_select_state(info->pinctrl, info->host1_5v_en_low);
                    
       if(!IS_ERR(info->host2_5v_en_low))
            pinctrl_select_state(info->pinctrl, info->host2_5v_en_low);
                    
        if(!IS_ERR(info->cam_vcc_en_low))
            pinctrl_select_state(info->pinctrl, info->cam_vcc_en_low);
    }
}

static void usb_switch_to_host_en(struct usb_iddig_info *info, bool enable)
{
    //printk("%s: enable = %d\n", __func__, enable);
    if(enable)
    {
        mutex_lock(&usb_id_status_switch_lock);
        if(!IS_ERR(info->usb_sel_high))
            pinctrl_select_state(info->pinctrl, info->usb_sel_high);
        
        usb_switch_to_host_flag = 1;
        mutex_unlock(&usb_id_status_switch_lock);
    }
    else
    {
        mutex_lock(&usb_id_status_switch_lock);
        if(!IS_ERR(info->usb_sel_low))
            pinctrl_select_state(info->pinctrl, info->usb_sel_low);
        
        usb_switch_to_host_flag = 0;
        mutex_unlock(&usb_id_status_switch_lock);
    }     
}

void otg_iddig_pm_resume(void)
{
    if(usb_switch_to_host_flag == 1)
    {
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND 
        otg_iddig_power_suspend = 1;
#endif
        pr_err("%s pradeep, enter\n",__func__);
        host_power_enable(pm_info, 1);  
    }
}

void otg_iddig_pm_suspend(void)
{
	
    if(usb_switch_to_host_flag == 1)
    {
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
        otg_iddig_power_suspend = 0;
#endif
		pr_err("%s pradeep, enter\n",__func__);
        host_power_enable(pm_info, 0); 
    }
}

static int fb_event_handler(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = NULL;
	int blank;
    
    if(usb_switch_to_host_flag == 0)
        return 0;

    //printk("fb_event_handler: get_usbmouse_in_status = %d\n", get_usbmouse_in_status());
	evdata = data;
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
            mutex_lock(&usb_host_power_lock);
            otg_iddig_pm_resume();
            mutex_unlock(&usb_host_power_lock);
        break;
	case FB_BLANK_POWERDOWN:
        pr_err("[usb-iddig], FB_BLANK_POWERDOWN \n");
        display_power_down = 1;
        wake_up_interruptible(&waiter1);
        mutex_lock(&usb_host_power_lock);
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
        if (get_usbmouse_in_status())
        {
            if (!pm_info->otg_iddig_wakelock.active)
                __pm_stay_awake(&pm_info->otg_iddig_wakelock);
			otg_iddig_power_suspend = 1;
        }
        else
        {
            __pm_relax(&pm_info->otg_iddig_wakelock);
#endif
#ifdef CONFIG_USB_JIOBOOK
	   

       
#else
            otg_iddig_pm_suspend();
#endif
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
        }
#endif
        mutex_unlock(&usb_host_power_lock);
        break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block pm_notifier_func = {
	.notifier_call = fb_event_handler,
	.priority = 0,
};

static void mtk_set_iddig_out_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_HIGH);
	//enable_irq_wake(info->id_irq);
	enable_irq(info->id_irq);
}

static void mtk_set_iddig_in_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_LOW);
	enable_irq(info->id_irq);
	//enable_irq_wake(info->id_irq);
}


static void iddig_mode_switch(struct work_struct *work)
{
	struct usb_iddig_info *info = container_of(to_delayed_work(work),
						    struct usb_iddig_info,
						    id_delaywork);

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
        printk("[usb-iddig]switch to host\n");
		mtk_idpin_cur_stat = IDPIN_IN_HOST;
        host_power_enable(info, 1);
		mt_usbhost_connect();
		//mt_vbus_on();
		mtk_set_iddig_out_detect(info);
	} else {
        printk("[usb-iddig]switch to device\n");
		mtk_idpin_cur_stat = IDPIN_OUT;
		mt_usbhost_disconnect();
        host_power_enable(info, 0);
		//mt_vbus_off();
		mtk_set_iddig_in_detect(info);
	}
}




static int hdmi_connect_handler(void *unused)
{
struct sched_param param = { .sched_priority = 4 };

sched_setscheduler(current, SCHED_RR, &param);
      do {
  	  set_current_state(TASK_INTERRUPTIBLE);
          pr_err("[usb-iddig], hdmi_connect_handler \n");

	  wait_event_interruptible(waiter3, (hdmi_connected != 0));
          
	  pr_err("[usb-iddig], hdmi_connected waiter3 event arrived\n");

	  hdmi_in_use = 1;

         host_power_enable(pm_info, 1);

         hdmi_connected = 0;
 	  pr_err("[usb-iddig], hdmi_connected:%d \n",hdmi_connected);
         set_current_state(TASK_RUNNING);
   	 

	} while (!kthread_should_stop());

}

static int lidclose_powerdown_handler(void *unused)
{
struct sched_param param = { .sched_priority = 4 };

sched_setscheduler(current, SCHED_RR, &param);
    do {
  	set_current_state(TASK_INTERRUPTIBLE);
        pr_err("[usb-iddig], lidclose_powerdown_handler \n");
      

	wait_event_interruptible(waiter2, (lid_close != 0));
	wait_event_interruptible(waiter1, (display_power_down != 0));

	pr_err("[usb-iddig], lidclose_powerdown_handler arrived\n");
	if(!hdmi_in_use){
	   otg_iddig_pm_suspend();
           hdmi_in_use = 0;
        }
	if(pm_info->otg_iddig_wakelock.active){
	pr_err("[usb-iddig], release wakelock held with lidclose_powerdown_handler\n");
	__pm_relax(&pm_info->otg_iddig_wakelock);
	}
	
          display_power_down = 0;
          lid_close = 0;
          set_current_state(TASK_RUNNING);
   	 

	} while (!kthread_should_stop());

}

static int lid_close_event_handler(void *unused)
{
struct sched_param param = { .sched_priority = 4 };

sched_setscheduler(current, SCHED_RR, &param);
      do {
  	  set_current_state(TASK_INTERRUPTIBLE);
          pr_err("[usb-iddig], lidclose_event_handler \n");
        
	
	   wait_event_interruptible(waiter2, (lid_close != 0));

	   pr_err("[usb-iddig], waiter2(lid_close event) arrived\n");
	   if(!hdmi_in_use){
	   hdmi_in_use = 0;
	   otg_iddig_pm_suspend();
	   }
	   if(pm_info->otg_iddig_wakelock.active){
	    pr_err("[usb-iddig] release wakelock held with powerdown\n");
	    __pm_relax(&pm_info->otg_iddig_wakelock);
	   }
	

          lid_close = 0;
          set_current_state(TASK_RUNNING);
   	 

	} while (!kthread_should_stop());

}
static int display_event_handler(void *unused)
{
struct sched_param param = { .sched_priority = 4 };

sched_setscheduler(current, SCHED_RR, &param);
      do {
  	  set_current_state(TASK_INTERRUPTIBLE);
          pr_err("[usb-iddig], display power down event handler,\n");
          wait_event_interruptible(waiter1, (display_power_down != 0));
          pr_err("[usb-iddig], event (waiter 1) display power down event arrived,\n");
          if(display_power_down == 1) {

           if ((!pm_info->otg_iddig_wakelock.active )){
	    pr_err("[usb-iddig]: 3 min timeout running\n");
	    __pm_stay_awake(&pm_info->otg_iddig_wakelock);
	    __pm_wakeup_event(&pm_info->otg_iddig_wakelock, wakelock_timeout_ms);
	 
	    }

           }
          
          display_power_down = 0;
       
          set_current_state(TASK_RUNNING);
   	 
	} while (!kthread_should_stop());

}
static int hdmi_disconnect_handler(void *unused)
{
struct sched_param param = { .sched_priority = 4 };

sched_setscheduler(current, SCHED_RR, &param);
      do {
	  pr_err("[usb-iddig], hdmi_disconnect_handler \n");
  	  set_current_state(TASK_INTERRUPTIBLE);
          wait_event_interruptible(waiter4, (hdmi_disconnected != 0));
	  pr_err("[usb-iddig], event(waiter4), hdmi disconnect arrived \n");
          hdmi_in_use = 0;
          hdmi_disconnected = 0;
 	  pr_err("[usb-iddig], hdmi_disconnected:%d \n",hdmi_disconnected);
          set_current_state(TASK_RUNNING);
   	 
	} while (!kthread_should_stop());

}
static irqreturn_t iddig_eint_isr(int irqnum, void *data)
{
	struct usb_iddig_info *info = data;

	disable_irq_nosync(irqnum);
	schedule_delayed_work(&info->id_delaywork,
		msecs_to_jiffies(info->id_swdebounce));

	return IRQ_HANDLED;
}

static int otg_iddig_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct usb_iddig_info *info;
	struct pinctrl *pinctrl;
	u32 ints[2] = {0, 0};
	int id;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	info->id_irq = irq_of_parse_and_map(node, 0);
	if (info->id_irq < 0)
		return -ENODEV;

	pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR(pinctrl)) {
		dev_info(&pdev->dev, "No find id pinctrl!\n");
	} else {
		info->pinctrl = pinctrl;
#if 0
		info->id_init = pinctrl_lookup_state(pinctrl, "id_init");
		if (IS_ERR(info->id_init))
			dev_info(&pdev->dev, "No find pinctrl id_init\n");
		else
			pinctrl_select_state(info->pinctrl, info->id_init);
		info->id_enable = pinctrl_lookup_state(pinctrl, "id_enable");
		info->id_disable = pinctrl_lookup_state(pinctrl, "id_disable");
		if (IS_ERR(info->id_enable))
			dev_info(&pdev->dev, "No find pinctrl iddig_enable\n");
		if (IS_ERR(info->id_disable))
			dev_info(&pdev->dev, "No find pinctrl iddig_disable\n");
#else
        ret = fb_register_client(&pm_notifier_func);
        if (ret) {
            pr_err("Failed to register PM notifier.\n");
        }

        info->usb_switch_to_host_enable =  of_property_read_bool(node, "usb_switch_to_host_enable");

        //hub_vcc
        info->hub_pwr_en_high = pinctrl_lookup_state(pinctrl, "hub_pwr_en_high");
        if (IS_ERR(info->hub_pwr_en_high))
			dev_info(&pdev->dev, "No find pinctrl hub_pwr_en_high\n");
		
        info->hub_pwr_en_low = pinctrl_lookup_state(pinctrl, "hub_pwr_en_low");
        if (IS_ERR(info->hub_pwr_en_low))
			dev_info(&pdev->dev, "No find pinctrl hub_pwr_en_low\n"); 
        
        //hub_rst
        info->hub_rst_high = pinctrl_lookup_state(pinctrl, "hub_rst_high");
        if (IS_ERR(info->hub_rst_high))
			dev_info(&pdev->dev, "No find pinctrl hub_rst_high\n");
        
        info->hub_rst_low = pinctrl_lookup_state(pinctrl, "hub_rst_low");
        if (IS_ERR(info->hub_rst_low))
			dev_info(&pdev->dev, "No find pinctrl hub_rst_low\n");
        
        //usb prot1
        info->host1_5v_en_high = pinctrl_lookup_state(pinctrl, "host1_5v_en_high");
        if (IS_ERR(info->host1_5v_en_high))
			dev_info(&pdev->dev, "No find pinctrl host1_5v_en_high\n");
        
        info->host1_5v_en_low = pinctrl_lookup_state(pinctrl, "host1_5v_en_low");
        if (IS_ERR(info->host1_5v_en_low))
			dev_info(&pdev->dev, "No find pinctrl host1_5v_en_low\n");
        
        //usb prot2 HOST2_5V_ENB
		info->host2_5v_en_high = pinctrl_lookup_state(pinctrl, "host2_5v_en_high");
        if (IS_ERR(info->host2_5v_en_high))
			dev_info(&pdev->dev, "No find pinctrl host2_5v_en_high\n");
        
        info->host2_5v_en_low = pinctrl_lookup_state(pinctrl, "host2_5v_en_low");
        if (IS_ERR(info->host2_5v_en_low))
			dev_info(&pdev->dev, "No find pinctrl host2_5v_en_low\n");
        
        //usb cam 
		info->cam_vcc_en_high = pinctrl_lookup_state(pinctrl, "cam_vcc_en_high");
        if (IS_ERR(info->cam_vcc_en_high))
			dev_info(&pdev->dev, "No find pinctrl cam_vcc_en_high\n");
        
        info->cam_vcc_en_low = pinctrl_lookup_state(pinctrl, "cam_vcc_en_low");
        if (IS_ERR(info->cam_vcc_en_low))
			dev_info(&pdev->dev, "No find pinctrl cam_vcc_en_low\n");
        
        //usb_sel
        info->usb_sel_high = pinctrl_lookup_state(pinctrl, "usb_sel_high");
        if (IS_ERR(info->usb_sel_high))
			dev_info(&pdev->dev, "No find pinctrl usb_sel_high\n");
		
        info->usb_sel_low = pinctrl_lookup_state(pinctrl, "usb_sel_low");
        if (IS_ERR(info->usb_sel_low))
			dev_info(&pdev->dev, "No find pinctrl usb_sel_low\n");
        
        if(info->usb_switch_to_host_enable && get_boot_mode() == NORMAL_BOOT)
            usb_switch_to_host_en(info, 1);
        else
            usb_switch_to_host_en(info, 0);
        
        ret = device_create_file(&pdev->dev, &dev_attr_usb_host_switch);
        if (ret) {
            dev_info(&pdev->dev, "Failed to register device file\n");
        }
#endif
	}
	ret = of_property_read_u32_array(node, "debounce",
		ints, ARRAY_SIZE(ints));
	if (!ret)
		info->id_hwdebounce = ints[1];

	info->id_swdebounce = msecs_to_jiffies(50);

	INIT_DELAYED_WORK(&info->id_delaywork, iddig_mode_switch);

	ret = devm_request_irq(dev, info->id_irq, iddig_eint_isr,
					0, pdev->name, info);
	if (ret < 0) {
		dev_info(dev, "failed to request handler for ID IRQ\n");
		return ret;
	}

	platform_set_drvdata(pdev, info);

	info->id_gpiod = devm_gpiod_get_optional(&pdev->dev, "id", GPIOD_IN);
	if (info->id_gpiod && !IS_ERR(info->id_gpiod)) {
		gpiod_set_debounce(info->id_gpiod, info->id_swdebounce);

		id = gpiod_get_value_cansleep(info->id_gpiod);
		if (id == 0) {
			disable_irq_nosync(info->id_irq);

			/* Perform initial detection */
			iddig_mode_switch(&info->id_delaywork.work);
		}
		dev_info(dev, "usb id-gpio value: %i success!!!\n", id);
	} else {
		dev_info(dev, "cannot get id-gpio node from dts\n");
	}
#ifdef CONFIG_USB_JIOBOOK
	debugfs_usb_wakelock();
#endif
#ifdef CONFIG_USBMOUSE_IN_NO_POWEROFF_SUSPEND
	wakeup_source_init(&info->otg_iddig_wakelock, "otg_iddig suspend wakelock");
#endif
    mutex_init(&usb_host_switch_lock);
    mutex_init(&usb_host_power_lock);
    mutex_init(&usb_id_status_switch_lock);
    pm_info = info;

        thread1 = kthread_run(lid_close_event_handler, 0, "lid-handling");
        if (IS_ERR(thread1)) {
        pr_err("Thread 1 error \n");
        
        }

        thread2 = kthread_run(display_event_handler, 0, "display-handling");
        if (IS_ERR(thread2)) {
        pr_err("Thread 2 error \n");
        
        }

	
	thread3 = kthread_run(lidclose_powerdown_handler, 0, "lidclose-powerdown-handling");
	if (IS_ERR(thread3)) {
	pr_err("Thread 3 error \n");

	}
	
	thread4 = kthread_run(hdmi_connect_handler, 0, "hdmi-connect-handling");
	if (IS_ERR(thread4)) {
	pr_err("Thread 4 error \n");

	}
	thread5 = kthread_run(hdmi_disconnect_handler, 0, "hdmi-disconnect-handling");
	if (IS_ERR(thread5)) {
	pr_err("Thread 5 error \n");

	}
	return 0;
}

static int otg_iddig_remove(struct platform_device *pdev)
{
	struct usb_iddig_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->id_delaywork);

	return 0;
}

static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	.remove = otg_iddig_remove,
	.driver = {
		.name = "otg_iddig",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_iddig_of_match),
	},
};

static int __init otg_iddig_init(void)
{
	return platform_driver_register(&otg_iddig_driver);
}
//late_initcall(otg_iddig_init);
module_init(otg_iddig_init);

static void __exit otg_iddig_cleanup(void)
{
	platform_driver_unregister(&otg_iddig_driver);
}

module_exit(otg_iddig_cleanup);

