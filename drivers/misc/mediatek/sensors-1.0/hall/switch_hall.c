#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
//#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

#include <linux/delay.h>
#include <linux/irq.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#endif
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/input.h>
#include "lid-switch.h"
#define HALL_DEBUG
#ifdef HALL_DEBUG
#define HALL_LOG(fmt, args...)  printk(fmt, ##args)
#else
#define HALL_LOG(fmt, args...)
#endif

#define EM_HALL_PROC_FILE "driver/cust_hall_support"
static unsigned int EINT_HALL_PIN;
struct work_struct work_hall;
static int hall_irq;
//static int hall_suspend_flag=0;
DECLARE_WAIT_QUEUE_HEAD(waiter2);


extern void mt_gpio_opt_pullen(int gpio, int val);
extern void mt_gpio_opt_pullsel(int gpio, int val);
extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
extern int irq_set_irq_type(unsigned int irq, unsigned int type);
static int cur_state = 0;
struct mutex hall_lock;
static struct input_dev *hall_dev = NULL;
int lid_close;
int lcm_is_resume;
static int counter = 0;
static int hall_is_far = 1;

static void hall_do_sleep(void)
{
	counter = 0;
	HALL_LOG("zxl>>> hall_do_sleep lid_close=%d\n" ,lid_close);
	while(counter < 50)
	{
		if(1 == __gpio_get_value(EINT_HALL_PIN)){
			
			
			input_report_switch(hall_dev, SW_LID, 0);
			input_sync(hall_dev);
                        lid_close = 0;
			HALL_LOG("zxl>>> lid open\n");
			break;
		}
		if(0 ==__gpio_get_value(EINT_HALL_PIN))
		{
			if(0 ==__gpio_get_value(EINT_HALL_PIN))
			{
			input_report_switch(hall_dev, SW_LID, 1);
			input_sync(hall_dev);
			lid_close = 1;
			
			wake_up(&waiter2);
			
			HALL_LOG("zxl>>> lid closed\n");
				break;
			}
			else
			{
				break;
			}
		}
		//else
			counter++;
		msleep(10);
	}
}
static void hall_do_sleep_work(struct work_struct *work)
{
	hall_do_sleep();
}

static irqreturn_t hall_irq_handler(int irq, void *data)
{

	cur_state = __gpio_get_value(EINT_HALL_PIN);
	HALL_LOG("zxl>>>hall_irq_handler>>> cur_state =%d\n", cur_state);
	if (0 == cur_state) {//靠近
        hall_is_far = 0;
		irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_HIGH);
	} else {//远离
        hall_is_far = 1;
		irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_LOW);
	}
	schedule_work(&work_hall);
	return IRQ_HANDLED;
}
static int hall_event_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
	struct fb_event *event = data;
	int blank_mode = 0;
	mutex_init(&hall_lock);
	if (action != FB_EARLY_EVENT_BLANK)
		return 0;

	if ((event == NULL) || (event->data == NULL))
		return 0;

	blank_mode = *((int *)event->data);

		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
			mutex_lock(&hall_lock);
			lcm_is_resume = 1;
			mutex_unlock(&hall_lock);
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			break;
		case FB_BLANK_POWERDOWN:
			mutex_lock(&hall_lock);
			lcm_is_resume = 0;
			mutex_unlock(&hall_lock);
			break;
		default:
			return -EINVAL;
		}
	return 0;
}

static struct notifier_block hall_event_notifier = {
	.notifier_call  = hall_event_notifier_callback,
};

static int em_hall_read_proc(struct seq_file *seq,void *v)
{
	seq_printf(seq, "%d\n", hall_is_far);

	return 0;
}

static int em_hall_open(struct inode *inode,struct file *file)
{
	return single_open(file,em_hall_read_proc,inode->i_private);
}

static const struct file_operations em_hall_seq_fops = {
	.open = em_hall_open,
	.read = seq_read,
	.release = single_release,
	.write = NULL,
	.owner = THIS_MODULE,
};


static int hall_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

	if(node)
	{
		HALL_LOG("%s;node name=%s,full_name=%s\n",__FUNCTION__,node->name,node->full_name);
		EINT_HALL_PIN = of_get_named_gpio(node, "gpio_hall_pin", 0);
		ret=gpio_request(EINT_HALL_PIN, "hall_irq");
		if(ret<0)
		{
			HALL_LOG("%s;Failed to request EINT_HALL_PIN gpio\n",__FUNCTION__);
		}
		gpio_direction_input(EINT_HALL_PIN);

		hall_irq =irq_of_parse_and_map(node, 0);
		if (hall_irq < 0)
		{
			HALL_LOG("%s;Failed to irq_of_parse_and_map EINT_HALL_PIN irq\n",__FUNCTION__);
		}
		HALL_LOG("%s;hall_irq =%d\n",__FUNCTION__,hall_irq);
		ret = request_irq(hall_irq , hall_irq_handler, IRQ_TYPE_LEVEL_LOW, "hall-eint", NULL);
		if (ret < 0)
		{
			HALL_LOG("%s;Failed to request_irq EINT_HALL_PIN irq\n",__FUNCTION__);
		}
	}
	INIT_WORK(&work_hall, hall_do_sleep_work);

	hall_dev = input_allocate_device();
	hall_dev->name = "hall-switch";
	input_set_capability(hall_dev, EV_SW, SW_LID);
	ret = input_register_device(hall_dev);
	if(ret){
		HALL_LOG("emdoor register hall input err!\n");
	}

	fb_register_client(&hall_event_notifier);
	proc_create(EM_HALL_PROC_FILE,0,NULL,&em_hall_seq_fops);
	HALL_LOG("%s; EINT_HALL_PIN gpio=%d\n",__FUNCTION__,EINT_HALL_PIN);
	HALL_LOG("%s; hall_irq gpio=%d\n",__FUNCTION__,hall_irq);
	return 0;

}

static int hall_remove(struct platform_device *pdev)
{

	cancel_work_sync(&work_hall);
	gpio_free(EINT_HALL_PIN);
	return 0;
}
static int hall_suspend(struct platform_device *pdev, pm_message_t state)
{
enable_irq_wake(hall_irq);
HALL_LOG("zxl>>>%s\n",__FUNCTION__);
	return 0;
}

static int hall_resume(struct platform_device *pdev)
{

    disable_irq_wake(hall_irq);
	HALL_LOG("zxl>>>%s\n",__FUNCTION__);
	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id hall_of_match[] = {
	{.compatible = "mediatek,hall",},
	{},
};
#endif

static struct platform_driver hall_driver = {
	.probe		= hall_probe,
	.remove		= hall_remove,
	.suspend    =hall_suspend,
	.resume     =hall_resume,
	.driver		= {
		.name	= "hall",
#ifdef CONFIG_OF
		 .of_match_table = hall_of_match,
#endif
			
		.owner	= THIS_MODULE,
	},
};


static int __init hall_switch_init(void)
{
	int ret = 0;
 
	ret = platform_driver_register(&hall_driver);
	if (ret) 
	{
        HALL_LOG("failed to register hall_device\n");
	}

	return 0;
}

static void __exit hall_switch_exit(void)
{
	platform_driver_unregister(&hall_driver);
}

late_initcall(hall_switch_init);
module_exit(hall_switch_exit);


MODULE_DESCRIPTION("HALL Switch Driver");
MODULE_LICENSE("GPL");
