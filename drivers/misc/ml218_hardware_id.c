#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>	/* platform device */
#include <linux/kernel.h>
#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/proc_fs.h>

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

static int proc_show(struct seq_file *m, void *v)
{
	int hardware_id = get_hw_flag();
	if (hardware_id == 0)
		seq_puts(m, "V1.3\n");
	else
		seq_puts(m, "V1.2\n");

	return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static const struct file_operations hardwareid_proc_fops = {
	.open = proc_open,
	.read = seq_read,
    .llseek = seq_lseek,
};

static ssize_t show_hardware_id(
    struct device *dev, struct device_attribute *attr,
                           char *buf)
{
	int hardware_id = get_hw_flag();
	if (hardware_id == 0)
		return sprintf(buf, "%s\n", "V1.3");
	else
		return sprintf(buf, "%s\n", "V1.2");
}

static DEVICE_ATTR(hardware_id, 0664, show_hardware_id,NULL);

static int hardwareid_probe(struct platform_device *dev)
{
	device_create_file(&(dev->dev),&dev_attr_hardware_id);
	proc_create("hardware_id", 0644,
		NULL, &hardwareid_proc_fops);
	return 0;
}

static struct platform_device hardwareid_device = {
	.name = "ml218_hardware_id",
	.id = -1,
};

static struct platform_driver hardwareid_driver = {
	.probe = hardwareid_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "ml218_hardware_id",
	},
};

static int __init hardwareid_init(void)
{
	int ret;

	ret = platform_device_register(&hardwareid_device);
	ret = platform_driver_register(&hardwareid_driver);

	pr_err("[%s] Initialization : DONE\n",__func__);

	return 0;
}

static void __exit hardwareid_exit(void)
{
	remove_proc_entry("hardware_id",NULL);
}
module_init(hardwareid_init);
module_exit(hardwareid_exit);

MODULE_AUTHOR("zhenlin.zhang");
MODULE_DESCRIPTION("Get hardwareid for ML218");
MODULE_LICENSE("GPL");
