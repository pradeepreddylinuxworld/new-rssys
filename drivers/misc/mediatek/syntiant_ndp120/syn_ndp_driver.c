/* vim: set noic ts=4 sw=4 expandtab: */
/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2019-2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 *  process, and are protected by trade secret or copyright law.  Dissemination
 *  of this information or reproduction of this material is strictly forbidden
 *  unless prior written permission is obtained from Syntiant Corporation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
/* begin device integration code */
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
/* end  device integration code */

#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#include <linux/compat.h>
#endif
#include "linux_backports.h"

#include "ndp10x_config.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp_error.h"
#include "syntiant_ilib/syntiant_ndp10x.h"
#include "syntiant_ilib/syntiant_ndp120.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/ndp120_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp_driver.h"
#include "syntiant_ilib/syntiant_ndp_ilib_version.h"
#include "syntiant_packager/syntiant_package.h"
#include "syntiant-firmware/ndp10x_firmware.h"

#include "ndp10x_ioctl.h"
#include "ndp120_ioctl.h"
#include "es_conversion.h"

/* begin device integration code */
#include "./../pmic/include/mt6358/mtk_pmic_api.h"
/* end device integration code */


#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/fs.h>

#define SYNTIANT_LTR559_I2C_ADDRESS (0x23)
#define SYNTIANT_DA217_I2C_ADDRESS (0x27)

#endif /* CONFIG_DEBUG_FS */

#define BYTES_PER_AUDIO_SAMPLE (2)
#define NDP10X_BYTES_PER_MILLISECOND (16000 * BYTES_PER_AUDIO_SAMPLE / 1000)
#define RESULT_RING_SIZE (16)
#define NDP10X_MATCH_INFO_SHIFT 8
#define NDP120_MATCH_INFO_SHIFT NDP10X_MATCH_INFO_SHIFT	// FIXME?
#define SENSOR_RESULT_RING_SIZE (8)
#define RESET_GPIO_9120 (110 + 320)
#define NDP120_DEFAULT_RESET_DELAY (100) // msec

#if defined(DEBUG) && defined(DEBUG_VERBOSE)
#define syntiant_debug(...)	pr_debug(__VA_ARGS__)
#else
#define syntiant_debug(...)
#endif

/* begin device integration code */
#define HARDCODE_NDP120_CHIPID
/* end device integration code */

/* watch data */
struct ndp_result_s {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
    struct timespec ts;
#else
    struct timespec64 ts;
#endif
    uint32_t summary;
    uint32_t extract_len_avail;
};

/* circular ring metadata */
struct ndp_ring_s {
    int producer;
    int consumer;
    int size;
    int element_size;
    int quantum;
    uint8_t *buf;
};

struct syn_ndp_spi_dev_s {
    /* TODO: maybe collapse/untangle some of this? */
    dev_t spi_devt;
    int registered;
    struct cdev cdev;
    struct class *spi_cls;
    struct device *spi_dev;
    struct spi_device *spi;
    struct mutex lock;
    struct syn_ndp_s *ndp;
    int spi_split;
    int spi_speed;
    int spi_read_pad_bytes;
    int spi_send_speed;
};

struct syn_ndp_s {
    int minor;
    int opens;
    struct device *device;
    struct syn_ndp_spi_dev_s spi_dev;
    struct syntiant_ndp_device_s *ndp;
    struct mutex ndp_mutex;
    wait_queue_head_t mbwait_waitq;
    int mbwait_condition;
    int sends_max_outstanding;
    spinlock_t send_ring_lock;
    struct ndp_ring_s send_ring;
    wait_queue_head_t send_waitq;
    int send_waiting;
    int sends_outstanding;
    struct mutex send_ioctl_mutex;    /* ensures only 1 send ioctl running */
    int extract_scratch_size;
    uint8_t *extract_scratch;
    spinlock_t extract_ring_lock;
    struct ndp_ring_s extract_ring;
    int extracts_left;
    int extract_waiting;
    wait_queue_head_t extract_waitq;
    struct mutex extract_ioctl_mutex; /* ensures only 1 extract ioctl running */
    struct mutex unbuffered_extract_mutex;

    /* ensures only 1 config (init, config, ndp10x_config, load) running */
    struct mutex config_mutex;
    struct mutex serial_mutex;

    spinlock_t result_ring_lock;
    spinlock_t sensor_result_ring_lock;
    struct ndp_ring_s result_ring;
    struct ndp_ring_s sensor_result_ring;
    wait_queue_head_t result_waitq;
    wait_queue_head_t sensor_result_waitq;
#ifdef CONFIG_PM_SLEEP
    int suspended;
    unsigned int irq_during_suspend_cnt;
    int irq_always_enabled;
#endif
    uint64_t isrs;
    uint64_t polls;
    uint64_t frames;
    uint64_t results;
    uint64_t results_dropped;
    uint64_t extracts;
    uint64_t extract_bytes;
    uint64_t extract_bytes_dropped;
    uint64_t sends;
    uint64_t send_bytes;
    int package_loaded;
    int audio_frame_step;     /* 0 -> no package loaded */
    int extract_buffer_size;
    int send_buffer_size;
    int result_per_frame;
    int armed;
    int armed_watch_active;
    int pcm_input;
    int extract_before_match;

    struct proc_dir_entry *procfs_dir_ent;
    struct proc_dir_entry *procfs_mic_ctl_file;
    struct proc_dir_entry *procfs_info_file;
#ifdef CONFIG_DEBUG_FS
    struct dentry *dbgfs;
    uint32_t devmem;
    uint32_t memrange;
    uint32_t dev_address;
#endif
/* begin device integration code */
    int dsp_vdd_en;
/* end device integration code */
    int reset_gpio;
    int reset_gpio_assert_direction;
    int ndp_reset_delay;
    syntiant_ndp_device_type_t chip_type;
};

static int syn_ndp_spi_probe(struct spi_device *spi);
static void ndp_uninit(struct syn_ndp_s *ndp10x);
static int syn_ndp_spi_remove(struct spi_device *spi);
static void syn_ndp_uninit(struct syn_ndp_s *ndp10x);

static int syn_ndp_spi_transfer(struct syn_ndp_s *ndp10x, int mcu, uint32_t addr,
                               void *out, void *in, unsigned int count);
static int syn_ndp_open(struct inode *inode, struct file *file);
static long syn_ndp_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg);
#ifdef CONFIG_COMPAT
static long ndp10x_compat_ioctl(struct file *f, unsigned int cmd,
                                unsigned long arg);
#endif
static int syn_ndp_release(struct inode *inode, struct file *file);

static int
ndp10x_ioctl_ndp10x_config(struct syn_ndp_s *ndp10x,
                           struct syntiant_ndp10x_config_s *ndp10x_config);

static ssize_t ndp10x_procfs_read_mic_ctl(struct file *file,
                                          char __user *ubuf,
                                          size_t count,
                                          loff_t *ppos);
static ssize_t ndp10x_procfs_write_mic_ctl(struct file *file,
                                           const char __user *ubuf,
                                           size_t count,
                                           loff_t *ppos);

/* begin device integration code */
static void ndp_config_ext_clk(void);
static void ndp_config_ext_pwr(struct spi_device *spi, struct syn_ndp_s *ndp10x);
/* end device integration code */
static void syn_ndp_reset_chip(struct syn_ndp_s *ndp10x, bool keep_in_reset);

#ifdef CONFIG_PM_SLEEP
static int ndp10x_driver_suspend(struct device *spi);
static int ndp10x_driver_resume(struct device *spi);
#else
#define ndp10x_driver_suspend NULL
#define ndp10x_driver_resume NULL
#endif

#define CLASS_NAME "syn_ndp"
#define DEVICE_NAME "syn_ndp"

#define MAX_DEV  16

static int ndp_major = -1;
static struct class *ndp_class = NULL;
static struct syn_ndp_s ndps[MAX_DEV];

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = syn_ndp_open,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl
#endif
    = syn_ndp_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = ndp10x_compat_ioctl,
#endif
    .release = syn_ndp_release,
};

static int ndp10x_procfs_info_show(struct seq_file *m, void *data);
static int ndp10x_procfs_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, ndp10x_procfs_info_show, PDE_DATA(inode));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
static const struct file_operations ndp10x_procfs_info_fops = {
    .owner = THIS_MODULE,
    .open = ndp10x_procfs_info_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops ndp10x_procfs_info_fops = {
    .proc_open = ndp10x_procfs_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
static struct file_operations procfs_mic_ctl_fops = {
    .owner = THIS_MODULE,
    .read = ndp10x_procfs_read_mic_ctl,
    .write = ndp10x_procfs_write_mic_ctl,
};
#else
static struct proc_ops procfs_mic_ctl_fops = {
    .proc_read = ndp10x_procfs_read_mic_ctl,
    .proc_write = ndp10x_procfs_write_mic_ctl,
};
#endif

static const struct of_device_id spi_match[] = {
    { .compatible = SPI_DEVICE_NAME, },
    {}
};
MODULE_DEVICE_TABLE(of, spi_match);

#ifdef CONFIG_PM_SLEEP
static SIMPLE_DEV_PM_OPS(syn_ndp_spi_driver_pm_ops, ndp10x_driver_suspend,
                         ndp10x_driver_resume);
#endif

static struct spi_driver syn_ndp_spi_driver = {
    .driver = {
        .name = SPI_DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = spi_match,
#ifdef CONFIG_PM_SLEEP
        .pm = &syn_ndp_spi_driver_pm_ops,
#endif
    },
    .probe = syn_ndp_spi_probe,
    .remove = syn_ndp_spi_remove,
};

static const struct file_operations syn_ndp_spi_fops = {
    .owner =    THIS_MODULE,
};

#ifdef CONFIG_PM_SLEEP
/* PM level functions */
static int ndp10x_driver_suspend(struct device *spi)
{
    struct syn_ndp_spi_dev_s *spi_dev =
        spi_get_drvdata((struct spi_device *)spi);
    pr_debug("%s irq=%d\n", __func__, spi_dev->spi->irq);
    if (!spi_dev->ndp->irq_always_enabled) {
        /* disable irq waits for any pending IRQ handlers */
        disable_irq(spi_dev->spi->irq);
    }
    WRITE_ONCE(spi_dev->ndp->suspended, 1);
    if (spi_dev->ndp->irq_always_enabled) {
        synchronize_irq(spi_dev->spi->irq);
    }
    enable_irq_wake(spi_dev->spi->irq);
    return 0;
}

static int ndp10x_driver_resume(struct device *spi)
{
    struct syn_ndp_spi_dev_s *spi_dev =
        spi_get_drvdata((struct spi_device *)spi);
    pr_debug("%s irq=%d\n", __func__, spi_dev->spi->irq);
    disable_irq_wake(spi_dev->spi->irq);
    WRITE_ONCE(spi_dev->ndp->suspended, 0);
    if (!spi_dev->ndp->irq_always_enabled) {
        enable_irq(spi_dev->spi->irq);
    } else {
         /* there may have been an irq during our suspend.
          * In this mode, since we can't rely on the SOC to run
          * the ISR as needed when irq is re-enabled,
          * instead force a run of ISR to check & process any pending irqs */
        irq_wake_thread(spi_dev->spi->irq, spi_dev);
    }
    return 0;
}
#endif

static void *
syn_ndp_malloc(int size)
{
    void *p;
#ifdef USE_VMALLOC
    p = vmalloc(size);
#else
    p = kmalloc(size, GFP_KERNEL);
#endif
    return p;
}

static void
syn_ndp_free(void *p)
{
#ifdef USE_VMALLOC
    vfree(p);
#else
    kfree(p);
#endif
}


/*
 * these rings:
 * - are suitable for objects of any size (`element_size`)
 * - implement a tail-drop behavior when doing ring_add()
 * - ensure there is always at least 1 `quantum` of space free after
 *   a ring_add()
 *
 * to avoid tail-dropping, simply do not ring_add() more than is reported
 * available by ring_space()
 */

static void
ring_reset(struct ndp_ring_s *ring)
{
    WRITE_ONCE(ring->producer, 0);
    WRITE_ONCE(ring->consumer, 0);
}

static void
ring_free(struct ndp_ring_s *ring)
{
    if (ring->buf) {
        syn_ndp_free(ring->buf);
        ring->buf = NULL;
    }
    ring->size = 0;
}

static int
ring_allocate(struct ndp_ring_s *ring, int size, int element_size, int quantum)
{
    int new_bytes = size * element_size;
    int current_bytes = ring->size * ring->element_size;
    int s = 0;

    ring_reset(ring);
    if (current_bytes != new_bytes || !ring->buf) {
        ring_free(ring);
        if (new_bytes) {
            ring->buf = syn_ndp_malloc(new_bytes);
            if (!ring->buf) {
                size = 0;
                s = -ENOMEM;
            }
        }
    }
    ring->size = size;
    ring->element_size = element_size;
    ring->quantum = quantum;

    return s;
}

void *
ring_producer(struct ndp_ring_s *ring)
{
    int es = ring->element_size;
    int p = READ_ONCE(ring->producer);
    uint8_t *b = ring->buf;

    return (void *) (b + p * es);
}

void *
ring_consumer(struct ndp_ring_s *ring)
{
    int es = ring->element_size;
    int c = READ_ONCE(ring->consumer);
    uint8_t *b = ring->buf;

    return (void *) (b + c * es);
}

int
ring_empty(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    return p == c;
}

/* this is for debugging only -- free space including reserved quantum */
int
ring_unused(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int space = p == c ? s : (c + s - p) % s;
    return space;
}

int
ring_space(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int q = ring->quantum;
    int space = (c + s - p - q) % s;
    return space;
}

int
ring_space_to_end(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int s = ring->size;
    int space = ring_space(ring);
    int to_end = s - p;
    int space_to_end = min(to_end, space);
    return space_to_end;
}

int
ring_cnt(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int cnt = s ? (p + s - c) % s : 0;
    return cnt;
}

int
ring_cnt_to_end(struct ndp_ring_s *ring)
{
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int cnt = ring_cnt(ring);
    int to_end = s - c;
    int cnt_to_end = min(cnt, to_end);
    return cnt_to_end;
}

void
ring_remove(struct ndp_ring_s *ring, int n)
{
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    WRITE_ONCE(ring->consumer, (c + n) % s);
}

int
ring_add(struct ndp_ring_s *ring, int n)
{
    int rs = ring_space(ring);
    int tail_drop = rs < n;
    int p = READ_ONCE(ring->producer);
    int s = ring->size;
    int dropped = 0;
    int c0, c, q;

    p = (p + n) % s;
    WRITE_ONCE(ring->producer, p);
    if (tail_drop) {
        q = ring->quantum;
        c0 = READ_ONCE(ring->consumer);
        c = p + s + q;
        dropped = (c - c0) % s;
        if (dropped) {
            WRITE_ONCE(ring->consumer, c % s);
        }
    }
    return dropped;
}

static int
syn_ndp_translate_error(int e)
{
    int s = 0;

    switch (e) {
    case SYNTIANT_NDP_ERROR_NONE:
        break;
    case SYNTIANT_NDP_ERROR_FAIL:
        s = -EIO;
        break;
    case SYNTIANT_NDP_ERROR_ARG:
    case SYNTIANT_NDP_ERROR_UNINIT:
        s = -EINVAL;
        break;
    case SYNTIANT_NDP_ERROR_UNSUP:
        s = -ENOSYS;
        break;
    case SYNTIANT_NDP_ERROR_NOMEM:
        s = -ENOMEM;
        break;
    case SYNTIANT_NDP_ERROR_BUSY:
        s = -EBUSY;
        break;
    case SYNTIANT_NDP_ERROR_TIMEOUT:
        s = -ETIME;
        break;
    case SYNTIANT_NDP_ERROR_MORE:
        break;
    }
    return s;
}


int
syn_ndp_mbwait(void *d)
{
    struct syn_ndp_s *ndp10x = d;
    long s0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    while (!(READ_ONCE(ndp10x->mbwait_condition))) {
        mutex_unlock(&ndp10x->ndp_mutex);
	    pr_debug("%s: unlock mutex\n", __func__);
        s0 = wait_event_interruptible_timeout
            (ndp10x->mbwait_waitq, READ_ONCE(ndp10x->mbwait_condition), HZ);
        if (!s0) {
            pr_info("%s: timed out\n", __func__);
            s = SYNTIANT_NDP_ERROR_TIMEOUT;
            goto out;
        } else if (s0 < 0) {
            pr_err("%s: error: %d\n", __func__, -s);
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }

        s0 = mutex_lock_interruptible(&ndp10x->ndp_mutex);
	    pr_debug("%s: lock mutex ? ret:%ld\n", __func__, s0);
        if (s0) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }
    }

 out:
    WRITE_ONCE(ndp10x->mbwait_condition, 0);
    return s;
}

int
syn_ndp_get_type(void *d, unsigned int *type)
{

    struct syn_ndp_s *ndp10x = d;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t in;

    struct device *spidev = &ndp10x->spi_dev.spi->dev;

    if (ndp10x->chip_type != SYNTIANT_NDP_CORE_2) {
        if (!of_find_property(spidev->of_node, "reset-fix-disable", NULL)) {
            s = syntiant_ndp10x_reset_fix(ndp10x->ndp);
            if (s) {
                pr_info("%s: syntiant_ndp10x_reset_fix fail %d\n", __func__, s);
                if (ndp10x->chip_type == SYNTIANT_NDP_CORE_1)
                    return s;
            }
        }
    }

    pr_info("%s: reading device id\n", __func__);

    s = syn_ndp_spi_transfer(ndp10x, 0, NDP10X_SPI_ID0, NULL, &in, 1);
    if (s) return s;

/* begin device integration code */
    pr_info("%s: device id read: 0x%02x\n", __func__, in);

    if (!in) {

#ifdef HARDCODE_NDP120_CHIPID
        in = 0x38;
#endif
/* end device integration code */

        pr_crit("%s: device id overriden to %d\n", __func__, in);
    }

    *type = in;

    return SYNTIANT_NDP_ERROR_NONE;
}

int
syn_ndp_sync(void *d)
{
    struct syn_ndp_s *ndp10x;
    int s;

    ndp10x = d;

    s = mutex_lock_interruptible(&ndp10x->ndp_mutex);

    return s ? SYNTIANT_NDP_ERROR_FAIL : SYNTIANT_NDP_ERROR_NONE;
}

int
syn_ndp_unsync(void *d)
{
    struct syn_ndp_s *ndp10x;

    ndp10x = d;

    mutex_unlock(&ndp10x->ndp_mutex);

    return SYNTIANT_NDP_ERROR_NONE;
}

int
syn_ndp_transfer(void *d, int mcu, uint32_t addr, void *out, void *in,
                unsigned int count)
{
    struct syn_ndp_s *dev = d;
    int s = SYNTIANT_NDP_ERROR_NONE;

#if 0
    pr_info("%s: mcu %d, addr 0x%08x, out %p, in %p, count %dB\n", __func__,
             mcu, addr, out, in, count);
#endif

    if(out && in) {
        pr_err("%s: error in transfer parameters\n", __func__);
    }

    s = syn_ndp_spi_transfer(dev, mcu, addr, out, in, count);
    if (s) {
        pr_crit("%s: unable to transfer to the SPI device\n", __func__);
    }

    return s;
}

static int
syn_ndp_enable(struct syn_ndp_s *dev)
{
    uint32_t causes;
    int s;

    causes = SYNTIANT_NDP_INTERRUPT_DEFAULT;
    s = syntiant_ndp_interrupts(dev->ndp, &causes);
    if (s) {
        pr_alert("%s: unable to enable interrupt: %s\n", __func__,
                 syntiant_ndp_error_name(s));
    }
    return syn_ndp_translate_error(s);
}

static int
ndp_quiesce(struct syn_ndp_s *dev)
{
    uint32_t interrupts;
    int s;

    interrupts = 0;
    s = syntiant_ndp_interrupts(dev->ndp, &interrupts);
    if (s) {
        pr_err("%s: unable to disable interrupts: %s\n", __func__,
               syntiant_ndp_error_name(s));
    }

    synchronize_irq(dev->spi_dev.spi->irq);

    s = syn_ndp_translate_error(s);
    return s;
}


static ssize_t ndp10x_procfs_read_mic_ctl(struct file *file,
                                          char __user *ubuf,
                                          size_t count,
                                          loff_t *ppos)
{
    char tmp[10];

    struct syn_ndp_s *ndp10x = PDE_DATA(file_inode(file));
    struct syntiant_ndp10x_config_s ndp10x_config;
    int s;

    /* bail out on zero reads & successive reads */
    if ((count == 0) || (*ppos > 0)) {
        return 0;
    }

    count = 0;

    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
    ndp10x_config.get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (!s) {
        sprintf(tmp, "%d\n",
                ndp10x_config.pdm_clock_ndp);
        /* +1 for trailing 0 */
        count = 1 + strlen(tmp);
        s = copy_to_user(ubuf, tmp, count);
        *ppos += count;
    }

    return count;
}


static ssize_t ndp10x_procfs_write_mic_ctl(struct file *file,
                                           const char __user *ubuf,
                                           size_t count,
                                           loff_t *ppos)
{
    struct syn_ndp_s *ndp10x = PDE_DATA(file_inode(file));
    struct syntiant_ndp10x_config_s ndp10x_config;
    char tmp[10];
    int s;
    int disable;

    /* bail out on zero reads & successive reads */
    if ((count == 0) || (*ppos > 0)) {
        return 0;
    }

    s = copy_from_user(tmp, ubuf, 1);
    if (s) {
        return -EFAULT;
    }

    /* only use first byte of data, but pretend we used it all */
    memset(&ndp10x_config, 0, sizeof(ndp10x_config));

    ndp10x_config.get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (s) {
        pr_err("%s: Error getting NDP10x config %d\n", __func__, s);
        *ppos += count;
        return count;
    }

    disable = tmp[0] == '0';
    pr_info("%s: %sabling NDP mic clock & input\n",
            __func__, disable ? "Dis" : "En");

    ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP
        | SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT
        | SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT;
    ndp10x_config.set1 = 0;
    ndp10x_config.get_all = 0;
    ndp10x_config.pdm_clock_ndp = !disable;
    ndp10x_config.dnn_input =
        disable ?
        SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0;
    ndp10x_config.tank_input =
        disable ?
        SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE:
        SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN;
    s = ndp10x_ioctl_ndp10x_config(ndp10x, &ndp10x_config);
    if (s) {
        pr_err("%s: Error setting NDP10x config %d\n", __func__, s);
    }
    *ppos += count;

    return count;
}

int freq_active(int dnn_input, int tank_input)
{
    return (((dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE)
        &&  (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)
        &&  (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT))
        ||  (tank_input != SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK));
}

int pdm_active(int dnn_input, int tank_input, int n)
{
    return  (((!n) && (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0))
        ||  ((n) && (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1))
        ||  (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM)
        ||  ((!n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0))
        ||  ((n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1))
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM)
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH)
        ||  ((!n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW))
        ||  ((n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW))
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW));
}

int i2s_active(int dnn_input, int tank_input)
{
    return ((dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH));
}

#define CONFIG_STRING_LEN 64
#define MAX_LABELS 64
static int ndp10x_procfs_info_show(struct seq_file *m, void *data)
{
    struct syn_ndp_s *ndp10x = m->private;

    int dnn_input;
    int tank_input;
    int have_input;
    int have_freq;
    int have_pdm;
    int have_i2s;
    char *mp;
    int freq;
    int eq;

    struct syntiant_ndp_config_s config;
    struct syntiant_ndp10x_config_s *ndp10x_config;
    int s;

    char fwver[CONFIG_STRING_LEN] = "";
    char paramver[CONFIG_STRING_LEN] =  "";
    char pkgver[CONFIG_STRING_LEN] = "";
    char label_data[CONFIG_STRING_LEN] = "";

    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    config.firmware_version = fwver;
    config.firmware_version_len = STRING_LEN;
    config.parameters_version = paramver;
    config.parameters_version_len = STRING_LEN;
    config.pkg_version = pkgver;
    config.pkg_version_len = STRING_LEN;
    config.labels = label_data;
    config.labels_len = STRING_LEN;

    s = syntiant_ndp_get_config(ndp10x->ndp, &config);
    if (s) {
        if (s == SYNTIANT_NDP_ERROR_UNINIT) {
            seq_printf(m, "NDP Device not initialized\n");
        } else {
            seq_printf(m, "NDP Get config error %d\n", s);
        }
        return 0;
    }

    ndp10x_config = (struct syntiant_ndp10x_config_s *)
                    syn_ndp_malloc(sizeof(*ndp10x_config));
    if (!ndp10x_config) {
        return -ENOMEM;
    }
    memset(ndp10x_config, 0, sizeof(*ndp10x_config));
    ndp10x_config->get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, ndp10x_config);

    seq_printf(m, "NDP10x Driver Version: %s\nFirmware: %s\nParameters: %s\n",
               SYNTIANT_NDP_ILIB_VERSION,fwver, paramver);

    seq_printf(m, "NDP MIC Output: %d [%d Hz]\n",
               ndp10x_config->pdm_clock_ndp,
               ndp10x_config->pdm_clock_rate);

    dnn_input = ndp10x_config->dnn_input;
    tank_input = ndp10x_config->tank_input;

    have_input = (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE);

    have_freq = freq_active(dnn_input, tank_input);

    have_pdm = pdm_active(dnn_input, tank_input, 0)
        || pdm_active(dnn_input, tank_input, 1);

    have_i2s = i2s_active(dnn_input, tank_input);

    seq_printf(m, "input clock rate: %d\n",
               ndp10x_config->input_clock_rate);

    seq_printf(m, "core clock rate: %d\n",
               ndp10x_config->core_clock_rate);
    seq_printf(m, "mcu clock rate: %d\n",
               ndp10x_config->mcu_clock_rate);
    seq_printf(m, "holding tank input: %s\n",
               syntiant_ndp10x_config_tank_input_s(ndp10x_config->tank_input));
    if (have_freq) {
        seq_printf(m, "holding tank size: %d\n",
                   ndp10x_config->tank_size);
        seq_printf(m, "holding tank maximum size: %d\n",
                   ndp10x_config->tank_max_size);
        seq_printf(m, "holding tank sample width: %d bits\n",
                   ndp10x_config->tank_bits);
    }

    if (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI) {
        seq_printf(m, "spi max PCM input rate: %d\n",
                   ndp10x_config->spi_max_pcm_input_rate);
        seq_printf(m, "spi word bits: %d\n",
                   ndp10x_config->spi_word_bits);
    }

    if(have_pdm) {
        seq_printf(m, "pdm clock frequency: %d\n",
                   ndp10x_config->pdm_clock_rate);
        seq_printf(m, "pdm clock source: %s\n",
                   (ndp10x_config->pdm_clock_ndp) ?
                   "ndp": "external");
        for(s=0; s<2; s++) {
            if(pdm_active(dnn_input, tank_input, s)) {
                seq_printf(m, "pdm%d in shift: %d\n",
                           s, ndp10x_config->pdm_in_shift[s]);
                seq_printf(m, "pdm%d out shift: %d\n",
                           s, ndp10x_config->pdm_out_shift[s]);
                seq_printf(m, "pdm%d DC offset: %d\n",
                           s, ndp10x_config->pdm_dc_offset[s]);
            }
        }
    }

    if(have_i2s) {
        seq_printf(m, "i2s frame size: %d\n",
                   ndp10x_config->i2s_frame_size);
        seq_printf(m, "i2s sample size: %d\n",
                   ndp10x_config->i2s_sample_size);
        seq_printf(m, "i2s sample msbit: %d\n",
                   ndp10x_config->i2s_sample_msbit);
    }

    if(have_freq) {
        seq_printf(m, "frequency domain clock rate: %d\n",
                   ndp10x_config->freq_clock_rate);
        seq_printf(m, "audio frame size: %d\n",
                   ndp10x_config->audio_frame_size);
        seq_printf(m, "audio frame step: %d\n",
                   ndp10x_config->audio_frame_step);
        seq_printf(m, "audio buffer used: %d\n",
                   ndp10x_config->audio_buffer_used);
        seq_printf(m, "audio buffer low water mark notification: %s\n",
                   (ndp10x_config->water_mark_on) ?
                   "on" : "off");
        seq_printf(m, "filter bank feature extractor bin count: %d\n",
                   ndp10x_config->freq_frame_size);
        seq_printf(m, "preemphasis filter decay exponent: %d\n",
                   ndp10x_config->preemphasis_exponent);
        seq_printf(m, "dsp power offset: %d\n", ndp10x_config->power_offset);
        seq_printf(m, "dsp power scale exponent: %d\n",
                   ndp10x_config->power_scale_exponent);
    }

    seq_printf(m, "dnn input: %s\n",
               syntiant_ndp10x_config_dnn_input_s(ndp10x_config->dnn_input));
    seq_printf(m, "dnn clock rate: %d\n", ndp10x_config->dnn_clock_rate);

    if(have_input) {
        seq_printf(m, "dnn frame size: %d\n",
                   ndp10x_config->dnn_frame_size);
    }

    if((dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT)
       || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)) {
        seq_printf(m, "dnn signed input: %s\n",
                   (ndp10x_config->dnn_signed) ?
                   "on" : "off");
    }

    if (have_input) {
        seq_printf(m, "dnn minimum input threshold: %d\n",
                   ndp10x_config->dnn_minimum_threshold);
        seq_printf(m, "dnn run input threshold: %d\n",
                   ndp10x_config->dnn_run_threshold);
        seq_printf(m, "dnn inputs: %d\n", ndp10x_config->dnn_inputs);
        seq_printf(m, "dnn static inputs: %d\n",
                   ndp10x_config->dnn_static_inputs);
        seq_printf(m, "dnn outputs: %d\n", ndp10x_config->dnn_outputs);
    }

    if(have_pdm && ndp10x_config->agc_on) {
        seq_printf(m, "agc: %s\n", (ndp10x_config->agc_on)?"on":"off");
        seq_printf(m, "agc_max_adj: %d, %d\n",
                   ndp10x_config->agc_max_adj[0],
                   ndp10x_config->agc_max_adj[1]);
        seq_printf(m, "agc_nom_speech_quiet: %d\n",
                   ndp10x_config->agc_nom_speech_quiet);
    }
    if (ndp10x_config->noise_thresh_win) {
        seq_printf(m, "noise threshold level: 0x%x\n",
                   ndp10x_config->noise_threshold);
        seq_printf(m, "noise threshold window: %d\n",
                   ndp10x_config->noise_thresh_win);
    }
    if(have_input && ndp10x_config->fw_pointers_addr) {
        seq_printf(m, "match per frame: %s\n",
                   (ndp10x_config->match_per_frame_on) ?
                   "on" : "off");
    }

    if(ndp10x_config->fw_pointers_addr) {
        seq_printf(m, "fw pointers address: %08x\n",
                   ndp10x_config->fw_pointers_addr);
    }

    mp = ndp10x_config->memory_power;
    for(s = 0; s <= SYNTIANT_NDP10X_CONFIG_MEMORY_MAX; s++) {
        if(mp[s] != SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON) {
            seq_printf(m, "memory_power of %s : %s\n",
                       syntiant_ndp10x_memory_s(s),
                       syntiant_ndp10x_memory_power_s(mp[s]));
        }
    }

    if (have_freq) {
        for(s = 0; s < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS; s++) {
            freq = (int)(ndp10x_config->filter_frequency[s]
                         * SYNTIANT_NDP10X_AUDIO_FREQUENCY/512/2);
            eq = ndp10x_config->filter_eq[s] / 2;
            seq_printf(m, "filter bin %d: %d hz,"
                       " eq %d dB\n",
                       s, freq, eq);
        }
    }
    seq_printf(m, "armed: %d\n",
               READ_ONCE(ndp10x->armed));
    seq_printf(m, "armed watch active: %d\n",
               READ_ONCE(ndp10x->armed_watch_active));
#ifdef CONFIG_PM_SLEEP
    seq_printf(m, "interrupts during suspend: %u\n",
               ndp10x->irq_during_suspend_cnt);
#endif
    seq_printf(m, "extract ring: %d/%d/%d\n",
               ndp10x->extract_ring.size,
               ndp10x->extract_ring.element_size,
               ndp10x->extract_ring.quantum);
    seq_printf(m, "send ring: %d/%d/%d\n",
               ndp10x->send_ring.size,
               ndp10x->send_ring.element_size,
               ndp10x->send_ring.quantum);
    if (ndp10x_config) {
        syn_ndp_free(ndp10x_config);
    }
    return 0;
}

int
syn_ndp_spi_transfer(struct syn_ndp_s *dev, int mcu, uint32_t addr,
                    void *out, void *in, unsigned int count)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syn_ndp_spi_dev_s *d = &dev->spi_dev;
    struct spi_device *spi = dev->spi_dev.spi;
    struct spi_transfer tr[3];
    uint8_t spi_cmd[4];
    uint8_t spi_cmd_addr[5];
    uint32_t spi_addr;
    uint32_t spi_mdata;
    uint32_t spi_sample;
    int s0;
    int i;
    struct spi_message m;

    if (!spi) {
        pr_crit("%s: SPI is not configured properly\n", __func__);
        return SYNTIANT_NDP_ERROR_UNINIT;
    }
    if (count > MAX_BUFFER_SIZE) {
        pr_err("%s: transfer size is greater than MAX_BUFFER_SIZE\n", __func__);
        return SYNTIANT_NDP_ERROR_NOMEM;
    }

	spi_addr = NDP10X_SPI_MADDR(0);
	spi_mdata = NDP10X_SPI_MDATA(0);
	spi_sample = NDP10X_SPI_SAMPLE;

    i = 0;
    spi_message_init(&m);
    if (mcu) {
        if ((count & 0x3) != 0) {
            pr_err("%s: SYNTIANT_NDP_ERROR_ARG, trying to transfer:%d bytes\n",
                    __func__, count);
            return SYNTIANT_NDP_ERROR_ARG;
        }
        memset(tr, 0, (2 + (in ? 1 : 0)) * sizeof(struct spi_transfer));

        /* Loads cmd + addr byte in 1 transaction */
        spi_cmd_addr[0] = spi_addr;
        memcpy(&spi_cmd_addr[1], &addr, sizeof(addr));
        tr[i].tx_buf = (char*)&spi_cmd_addr[0];
        tr[i].speed_hz = d->spi_speed;
        tr[i].len = 5;
        /* for reads, need to restart a read command packet */
        if (in) {
            if (d->spi_split) {
                /* some OS/chip combinations don't cs_change signal so flush */
                spi_message_add_tail(&tr[i], &m);
                mutex_lock(&d->lock);
                s0 = spi_sync(spi, &m);
                mutex_unlock(&d->lock);
                if (s0 < 0) {
                    pr_err("%s: unable to transfer first split on SPI device\n",
                           __func__);
                    s = SYNTIANT_NDP_ERROR_FAIL;
                    goto error;
                }
            } else {
                tr[i].cs_change = 1;
                spi_message_add_tail(&tr[i], &m);
                i += 1;
                tr[i].speed_hz = d->spi_speed;
            }
            /*
             * this adds time between the command+address write
             * and the read of the first MCU data, during which
             * time the hardware is fetching data in the chip.
             * spi_read_delay > 0 enables SPI clock rates > 1 mbps
             * we read from bytes 1-3 of MADDR which are located before
             * MDATA.
             */
            memset(spi_cmd, 0, sizeof(spi_cmd));
            spi_cmd[0] = 0x80 | (spi_mdata - d->spi_read_pad_bytes);
            tr[i].tx_buf = (char*) &spi_cmd[0];
            tr[i].len = 1 + d->spi_read_pad_bytes;
        }
    } else {
        if (0xff < addr) {
            pr_err("%s: SYNTIANT_NDP_ERROR_ARG\n", __func__);
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        memset(tr, 0, 2 * sizeof(*tr));
        spi_cmd[0] = (in ? 0x80 : 0) | addr;
        tr[i].tx_buf = (char*) &spi_cmd[0];
        tr[i].len = 1;
    }
    spi_message_add_tail(&tr[i], &m);
    i += 1;
    tr[i].tx_buf = out;
    tr[i].rx_buf = in;
    tr[i].len = count;
    tr[i].speed_hz =
        (!mcu && !in && addr == spi_sample)
        ? d->spi_send_speed
        : d->spi_speed;
    spi_message_add_tail(&tr[i], &m);

    mutex_lock(&d->lock);
    s0 = spi_sync(spi, &m);
    mutex_unlock(&d->lock);
    if (s0 < 0) {
	pr_err("dump SPI\n");
	pr_err("spi_addr: 0x%x addr:0x%x\n", spi_addr, addr);
	while (i >= 0) {
		pr_err("speed: %d hz len:%d tx_buf:%p rx_buf:%p i:%d\n",
			tr[i].speed_hz, tr[i].len, tr[i].tx_buf, tr[i].rx_buf, i);
		i--;
	}
	pr_err("spi msg: frame len:%d actual len:%d is_dma_mapped: 0x%x\n", m.frame_length,
			m.actual_length, m.is_dma_mapped);
        pr_err("%s: unable to transfer on SPI device: %d\n", __func__, s0);
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

 error:
    return s;
}

static void
ndp10x_poll_sensors(struct syn_ndp_s *ndp10x, uint32_t summary)
{
    struct ndp_ring_s *result_ring;
    struct ndp_result_s *result;

    if (NDP10X_SPI_MATCH_MULT_MASK & summary) {
        result_ring =  &ndp10x->sensor_result_ring;
        result = ring_producer(result_ring);
        /* always 1 quantum free due to the design of the ring logic */
        result->summary = summary;
        spin_lock(&ndp10x->sensor_result_ring_lock);
        ring_add(result_ring, 1);
        spin_unlock(&ndp10x->sensor_result_ring_lock);
        wake_up_interruptible_all(&ndp10x->sensor_result_waitq);
    }
}

static int
syn_ndp_configure_match_per_frame(struct syn_ndp_s *dev)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_config_s ndp10x_config = {0};
    struct syntiant_ndp120_config_misc_s ndp120_config;

    if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
        memset(&ndp120_config, 0, sizeof(ndp120_config));
        ndp120_config.set = NDP120_CONFIG_SET_MISC_MATCH_PER_FRAME_ON;
        ndp120_config.match_per_frame_on = 1;
        s = syntiant_ndp120_config_misc(dev->ndp, &ndp120_config);
    } else if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        ndp10x_config.set =
            SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
        ndp10x_config.match_per_frame_on = 1;
        s = syntiant_ndp10x_config(dev->ndp, &ndp10x_config);
    } else {
        s = SYNTIANT_NDP_ERROR_UNSUP;
        goto out;
    }
    if (s) {
        pr_err("%s: failed to set match per frame %s\n", __func__,
               syntiant_ndp_error_name(s));
    }
out:
    return s;
}

static void
syn_ndp_poll_one(struct syn_ndp_s *dev)
{
    int s, frame;
    uint32_t notifications, summary;
    struct ndp_result_s *result;
    struct ndp_ring_s *result_ring;
    int len;

    dev->polls++;

    s = syntiant_ndp_poll(dev->ndp, &notifications, 1);
    if (s) {
        pr_err("%s: poll failed: %s\n", __func__, syntiant_ndp_error_name(s));
    }

    syntiant_debug("%s: poll, isr %llu notifications 0x%x\n",
            __func__, dev->isrs, notifications);

    if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        frame = notifications & SYNTIANT_NDP_NOTIFICATION_DNN;
    } else {
        frame = notifications & SYNTIANT_NDP_NOTIFICATION_MATCH;
    }

    if (frame) {
        dev->frames++;

        s = syntiant_ndp_get_match_summary(dev->ndp, &summary);
        if (s) {
            pr_err("%s: get_match_summary failed: %s\n", __func__,
                   syntiant_ndp_error_name(s));
            return;
        }

        if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
            ndp10x_poll_sensors(dev, summary);
        }

        syntiant_debug("%s: frame:%lld summary: 0x%x\n",
                __func__, dev->frames, summary);
        if (dev->result_per_frame
            || (NDP10X_SPI_MATCH_MATCH_MASK & summary)
            || (summary & NDP10X_MATCH_NOISE_THRESHOLD_MASK)) {
#ifdef CONFIG_PM_SLEEP
            pm_wakeup_event(&dev->spi_dev.spi->dev, 2500);
#endif
            /* NDP10X_SPI_MATCH_MULT_MASK -> not tank-correlated match */
            if (((NDP10X_SPI_MATCH_MATCH_MASK & summary) ==
            NDP10X_SPI_MATCH_MATCH_MASK) && READ_ONCE(dev->armed)) {
                spin_lock(&dev->extract_ring_lock);
                ring_reset(&dev->extract_ring);
                spin_unlock(&dev->extract_ring_lock);

                len = dev->extract_before_match;
                s = syntiant_ndp_extract_data
                    (dev->ndp,
                     SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                     SYNTIANT_NDP_EXTRACT_FROM_MATCH,
                     NULL,
                     &len);
                if (s) {
                    pr_err("%s: extract len=%d from _MATCH failed: %s\n",
                           __func__, dev->extract_before_match,
                           syntiant_ndp_error_name(s));
                    return;
                }

                dev->extracts_left = len / dev->audio_frame_step;

                s = syn_ndp_configure_match_per_frame(dev);
                if (s) {
                    pr_err("%s: failed to set match-per-frame: %s\n",
                           __func__,
                           syntiant_ndp_error_name(s));
                }
                WRITE_ONCE(dev->armed, 0);
                pr_debug("%s: disarming match, len = %d, left = %d\n",
                         __func__, len, dev->extracts_left);
            }
            syntiant_debug("%s: result recorded\n", __func__);
            dev->results++;
            result_ring =  &dev->result_ring;
            result = ring_producer(result_ring);
            /* always 1 quantum free due to the design of the ring logic */
            result->summary = summary;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
            getnstimeofday(&result->ts);
#else
            ktime_get_real_ts64(&result->ts);
#endif
            result->extract_len_avail = len;
            spin_lock(&dev->result_ring_lock);
            dev->results_dropped += ring_add(result_ring, 1);
            spin_unlock(&dev->result_ring_lock);
            wake_up_interruptible(&dev->result_waitq);
        }
    }

    if (notifications & SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN) {
        pr_debug("%s: mailbox_in notification \n", __func__);
        WRITE_ONCE(dev->mbwait_condition, 1);
        wake_up_interruptible(&dev->mbwait_waitq);
    }

    /* none of these interrupts is actionable with current firmware */
    if (notifications & SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT) {
        pr_debug("%s: mailbox_out notification \n", __func__);
    }

    if (notifications & SYNTIANT_NDP_NOTIFICATION_ERROR) {
        pr_info("%s: error notification \n", __func__);
    }
}

static int
syn_ndp_can_extract(struct syn_ndp_s *dev)
{
    int s = 0;
    uint32_t size;
    int package_loaded = READ_ONCE(dev->package_loaded);
    int frame_step = READ_ONCE(dev->audio_frame_step);

    if (package_loaded) {
        if (!dev->extracts_left) {
            size = dev->chip_type == SYNTIANT_NDP_CORE_2 ? frame_step * 2 : 0;
            s = syntiant_ndp_extract_data(dev->ndp,
                                          SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                                          SYNTIANT_NDP_EXTRACT_FROM_UNREAD,
                                          NULL, &size);
            if (s) {
                // FIXME? Do we care about the "reread" error?
                pr_debug_ratelimited("%s: extract_data failed: %s (size %u)\n", __func__,
                       syntiant_ndp_error_name(s), size);
            } else {
                dev->extracts_left = size / frame_step;
                if (dev->pcm_input) {
                    pr_debug("%s: fs: %d, size %d, left %d\n", __func__,
                             frame_step, size, dev->extracts_left);
                }
            }
        }
    } else {
        dev->extracts_left = 0;
    }

    return 0 < dev->extracts_left;
}

static void
syn_ndp_extract_one(struct syn_ndp_s *dev)
{
    struct ndp_ring_s *extract_ring = &dev->extract_ring;
    int frame_step = READ_ONCE(dev->audio_frame_step);
    uint8_t *buf = ring_producer(extract_ring);
    int armed = READ_ONCE(dev->armed);
    int extract_used;
    uint32_t size, left;
    int s;

    BUG_ON(!frame_step);
    BUG_ON(extract_ring->size % frame_step);
    BUG_ON(ring_unused(extract_ring) < frame_step);

    size = frame_step;
    s = syntiant_ndp_extract_data(dev->ndp,
                                  SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                                  SYNTIANT_NDP_EXTRACT_FROM_UNREAD,
                                  armed ? NULL : buf, &size);
    if (s) {
        pr_err("%s: extract failed: %s\n", __func__,
               syntiant_ndp_error_name(s));
        return;
    }
    if (size < frame_step) {
        pr_err("%s: unexpectedly short extract size=%d, left=%d\n", __func__,
               size, dev->extracts_left);
        return;
    }


    left = size - frame_step;
    dev->extracts_left = left / frame_step;
    dev->extracts++;
    dev->extract_bytes += frame_step;
    if (dev->sends_outstanding) {
        dev->sends_outstanding--;
    }

    if (dev->pcm_input) {
        pr_debug("%s: size: %d, left: %d sends_out: %d, armed: %d\n",
                 __func__, size, dev->extracts_left,
                 dev->sends_outstanding, armed);
    }

    if (!armed) {
        spin_lock(&dev->extract_ring_lock);
        dev->extract_bytes_dropped += ring_add(extract_ring, frame_step);
        extract_used = ring_cnt(extract_ring);
        if (dev->extract_waiting <= extract_used
            || extract_ring->size / 2 <= extract_used) {
            wake_up_interruptible(&dev->extract_waitq);
        }
        spin_unlock(&dev->extract_ring_lock);
    }
}

static int
syn_ndp_can_send(struct syn_ndp_s *ndp10x)
{
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;
    int package_loaded = READ_ONCE(ndp10x->package_loaded);
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);
    int used;
    int send = 0;

    if (package_loaded) {
        used = ring_cnt(send_ring);
        send = ndp10x->sends_outstanding < ndp10x->sends_max_outstanding
            && frame_step <= used;
    }

    return send;
}

static void
syn_ndp_send_one(struct syn_ndp_s *ndp10x)
{
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);
    uint8_t *buf = ring_consumer(send_ring);
    int send_free;
    int s;

    BUG_ON(!frame_step);

    s = syntiant_ndp_send_data(ndp10x->ndp, buf, frame_step,
                               SYNTIANT_NDP_SEND_DATA_TYPE_STREAMING, 0);
    if (s) {
        pr_info("%s:%d error sending %d bytes of data\n",__func__,
                __LINE__, frame_step);
        return;
    }

    ndp10x->sends++;
    ndp10x->send_bytes += frame_step;

    pr_debug("%s: sent %d, send_waiting: %d, send_free: %d\n", __func__,
             frame_step, ndp10x->send_waiting, ring_space(send_ring));

    spin_lock(&ndp10x->send_ring_lock);
    ring_remove(send_ring, frame_step);
    send_free = ring_space(send_ring);
    ndp10x->sends_outstanding++;
    if (ndp10x->send_waiting <= send_free
        || send_ring->size / 2 <= send_free) {
        wake_up_interruptible(&ndp10x->send_waitq);
    }
    spin_unlock(&ndp10x->send_ring_lock);
}

static irqreturn_t
ndp_isr(int irq, void *dev_id)
{
    int continue_io = 1;
    struct syn_ndp_spi_dev_s *spi_dev = dev_id;
    struct syn_ndp_s *ndp = spi_dev->ndp;

#ifdef CONFIG_PM_SLEEP
    if (READ_ONCE(ndp->suspended)) {
        pr_debug("%s: interrupt while suspended\n", __func__);
        ndp->irq_during_suspend_cnt++;
        return IRQ_HANDLED;
    }
#endif

    ndp->isrs++;

    /* BUG_ON(!ndp10x->ndp); TODO: level-triggered should fix stray interrupt? */
    if (!ndp->ndp) {
        pr_debug("%s: interrupt without NDP\n", __func__);
        goto out;
    }

    syn_ndp_poll_one(ndp);
    while (continue_io) {
        continue_io = 0;
        if (syn_ndp_can_extract(ndp)) {
            syn_ndp_extract_one(ndp);
            syn_ndp_poll_one(ndp);
            continue_io = 1;
        }
        if (syn_ndp_can_send(ndp)) {
            syn_ndp_send_one(ndp);
            syn_ndp_poll_one(ndp);
            continue_io = 1;
        }
    }

out:

    return IRQ_HANDLED;
}


#ifdef CONFIG_COMPAT
static long
ndp10x_compat_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    pr_debug("%s: compat ioctl cmd=%X\n", __func__, cmd);
    return syn_ndp_ioctl(f, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int
syn_ndp_ioctl_init(struct syn_ndp_s *dev, unsigned long mode)
{
    struct syntiant_ndp_integration_interfaces_s iif;
    struct syntiant_ndp_config_s ndp_config;
    int s;

    if (mode == NDP10X_IOCTL_INIT_UNINIT && !dev->ndp) {
        return -EINVAL;
    }

    iif.d = dev;
    iif.malloc = syn_ndp_malloc;
    iif.free = syn_ndp_free;
    iif.mbwait = syn_ndp_mbwait;
    iif.get_type = syn_ndp_get_type;
    iif.sync = syn_ndp_sync;
    iif.unsync = syn_ndp_unsync;
    iif.transfer = syn_ndp_transfer;
    memset(&ndp_config, 0, sizeof(ndp_config));

    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        return s;
    }

    if (dev->ndp) {
        pr_debug("%s: NDP already initialized\n", __func__);
        if (mode == NDP10X_IOCTL_INIT_NO_OVERRIDE) {
            s = -EINVAL;
            goto out;
        }
        pr_debug("%s: uninitializing NDP\n", __func__);
        ndp_uninit(dev);
        if (mode == NDP10X_IOCTL_INIT_UNINIT) {
            goto out;
        }
    }

    s = syntiant_ndp_init(&dev->ndp, &iif, SYNTIANT_NDP_INIT_MODE_RESET);
    if (s) {
        pr_alert("%s: chip %d init failed: %s\n", __func__, dev->minor,
                 syntiant_ndp_error_name(s));
        s = syn_ndp_translate_error(s);
        goto out;
    }
    s = syn_ndp_enable(dev);
    if (s) {
        ndp_uninit(dev);
        goto out;
    }
    s = syntiant_ndp_get_config(dev->ndp, &ndp_config);
    if (s) {
        ndp_uninit(dev);
        pr_alert("%s: ndp10x_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
        s = syn_ndp_translate_error(s);
        goto out;
    }

    pr_info("syn_ndp%d: %s initialized\n", dev->minor,
            ndp_config.device_type);
out:
    mutex_unlock(&dev->config_mutex);

    return s;
}

static int
ndp120_ioctl_init(struct syn_ndp_s *dev, struct ndp120_init_s *init)
{
    struct syntiant_ndp_integration_interfaces_s iif;
    struct syntiant_ndp_config_s ndp_config;
    int s;

    if (init->mode == NDP10X_IOCTL_INIT_UNINIT && !dev->ndp) {
        return -EINVAL;
    }

    iif.d = dev;
    iif.malloc = syn_ndp_malloc;
    iif.free = syn_ndp_free;
    iif.mbwait = syn_ndp_mbwait;
    iif.get_type = syn_ndp_get_type;
    iif.sync = syn_ndp_sync;
    iif.unsync = syn_ndp_unsync;
    iif.transfer = syn_ndp_transfer;
    memset(&ndp_config, 0, sizeof(ndp_config));

    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        return s;
    }

    if (dev->ndp) {
        pr_debug("%s: NDP already initialized\n", __func__);
        if (init->mode == NDP10X_IOCTL_INIT_NO_OVERRIDE) {
            s = -EINVAL;
            goto out;
        }
        pr_debug("%s: uninitializing NDP\n", __func__);
        WRITE_ONCE(dev->package_loaded, 0);
        ndp_uninit(dev);
        if (init->mode == NDP10X_IOCTL_INIT_UNINIT) {
            goto out;
        }
        syn_ndp_reset_chip(dev, false);
    }

    s = syntiant_ndp_init(&dev->ndp, &iif, SYNTIANT_NDP_INIT_MODE_RESET);
    if (s) {
        pr_alert("%s: chip %d init failed: %s\n", __func__, dev->minor,
                 syntiant_ndp_error_name(s));
        s = syn_ndp_translate_error(s);
        goto out;
    }
    s = syn_ndp_enable(dev);
    if (s) {
        ndp_uninit(dev);
        goto out;
    }
    /* configure clock */
    pr_info("configure clock to: %u Hz\n", init->input_freq);
    s = syntiant_ndp120_get_put_ext_clk_freq(dev->ndp, &init->input_freq);
    if (s) {
	    pr_alert("%s: clock config failed\n", __func__);
	    s = syn_ndp_translate_error(s);
	    goto out;
    }
    pr_info("configured clock: %u Hz\n", init->input_freq);
    s = syntiant_ndp_get_config(dev->ndp, &ndp_config);
    if (s) {
        ndp_uninit(dev);
        pr_alert("%s: ndp120_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
        s = syn_ndp_translate_error(s);
        goto out;
    }

    pr_info("ndp120%d: %s initialized\n", dev->minor,
            ndp_config.device_type);
out:
    mutex_unlock(&dev->config_mutex);

    return s;
}

static int
ndp10x_ioctl_ndp10x_config(struct syn_ndp_s *ndp10x,
                           struct syntiant_ndp10x_config_s *ndp10x_config)
{
    int s, spi, s0;
    int set_input = ndp10x_config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT;

    s = mutex_lock_interruptible(&ndp10x->config_mutex);
    if (s) {
        return s;
    }

    if (set_input) {
       spi = (ndp10x_config->dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI);
       WRITE_ONCE(ndp10x->pcm_input, spi);
        if (READ_ONCE(ndp10x->armed)) {
            ndp10x_config->set |= SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
            ndp10x_config->match_per_frame_on = spi;
        }
        s = ndp_quiesce(ndp10x);
        if (s) {
            pr_err("%s: quiesce failed: %d\n", __func__, -s);
            goto out;
        }
    }

    s = syntiant_ndp10x_config(ndp10x->ndp, ndp10x_config);
    if (s) {
        pr_alert("%s: ndp10x_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
    }
    s = syn_ndp_translate_error(s);

 out:
    if (set_input) {
        WRITE_ONCE(ndp10x->extracts_left, 0);
        s0 = syn_ndp_enable(ndp10x);
        s = s ? s : s0;
    }

    mutex_unlock(&ndp10x->config_mutex);

    return s;
}

static int
syn_ndp_ioctl_ndp_config(struct syn_ndp_s *ndp10x,
                        struct ndp_config_s *ndp_config)
{
    int s = 0;
    int s0;
    struct syntiant_ndp_config_s s_ndp_config;
    struct syntiant_ndp_config_s *c = &s_ndp_config;
    char *buf = NULL;
    unsigned int len, device_type_len;

    pr_debug("%s: devtypelen: %d, fwlen: %d, parlen: %d, lablen: %d"
             ", pkglen: %d , dsplen: %d\n",
             __func__, ndp_config->device_type_len,
             ndp_config->firmware_version_len,
             ndp_config->parameters_version_len,
             ndp_config->labels_len,
             ndp_config->pkg_version_len,
             ndp_config->dsp_firmware_version_len);

    len = ndp_config->firmware_version_len
        + ndp_config->parameters_version_len
        + ndp_config->labels_len
        + ndp_config->pkg_version_len
        + ndp_config->dsp_firmware_version_len;

    buf = syn_ndp_malloc(len);
    if (!buf) {
    pr_err("%s: can't allocate memory for user buffer\n", __func__);
        s = -ENOMEM;
        goto out;
    }

    c->firmware_version = buf;
    c->firmware_version_len = ndp_config->firmware_version_len;
    c->parameters_version = c->firmware_version + c->firmware_version_len;
    c->parameters_version_len = ndp_config->parameters_version_len;
    c->labels = c->parameters_version + c->parameters_version_len;
    c->labels_len = ndp_config->labels_len;
    c->pkg_version = c->labels + c->labels_len;
    c->pkg_version_len = ndp_config->pkg_version_len;
    c->dsp_firmware_version = c->pkg_version + c->pkg_version_len;
    c->dsp_firmware_version_len = ndp_config->dsp_firmware_version_len;

    s0 = syntiant_ndp_get_config(ndp10x->ndp, c);
    s = syn_ndp_translate_error(s0);
    if (s0) {
        pr_alert("%s: unable to get configuration: %s\n", __func__,
                 syntiant_ndp_error_name(s0));
        goto out;
    }

    device_type_len = strlen(c->device_type) + 1;
    len = min(ndp_config->device_type_len, device_type_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->device_type), c->device_type, len)) {
        pr_debug("%s: device type protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->firmware_version_len, c->firmware_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->firmware_version),
                     c->firmware_version, len)) {
        pr_debug("%s: firmware version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->parameters_version_len, c->parameters_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->parameters_version),
                     c->parameters_version, len)) {
        pr_debug("%s: parameters version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->labels_len, c->labels_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->labels), c->labels, len)) {
        pr_debug("%s: labels_version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->pkg_version_len, c->pkg_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->pkg_version), c->pkg_version, len)) {
        pr_debug("%s: pkg_version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->dsp_firmware_version_len, c->dsp_firmware_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->dsp_firmware_version), c->dsp_firmware_version, len)) {
        pr_debug("%s: dsp_firmware_version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    ndp_config->classes = c->classes;
    ndp_config->device_type_len = device_type_len;
    ndp_config->firmware_version_len = c->firmware_version_len;
    ndp_config->parameters_version_len = c->parameters_version_len;
    ndp_config->labels_len = c->labels_len;
    ndp_config->pkg_version_len = c->pkg_version_len;
    ndp_config->dsp_firmware_version_len = c->dsp_firmware_version_len;

 out:
    if (buf) {
        syn_ndp_free(buf);
    }
    return s;
}

static int
syn_ndp_pcm_rings_init(struct syn_ndp_s *ndp10x, int frame_step)
{
    int s = 0;
    int extract_ring_size, send_ring_size;

    WRITE_ONCE(ndp10x->extracts_left, 0);
    extract_ring_size =
        ndp10x->extract_buffer_size * NDP10X_BYTES_PER_MILLISECOND
        / frame_step * frame_step;
    s = ring_allocate(&ndp10x->extract_ring, extract_ring_size, 1, frame_step);
    if (s) {
        pr_alert("%s: failed to allocate extract ring\n", __func__);
        goto error;
    }

    WRITE_ONCE(ndp10x->sends_outstanding, 0);
    send_ring_size =
        ndp10x->send_buffer_size * NDP10X_BYTES_PER_MILLISECOND
        / frame_step * frame_step;
    s = ring_allocate(&ndp10x->send_ring, send_ring_size, 1, frame_step);
    if (s) {
        pr_alert("%s: failed to allocate send ring\n", __func__);
        goto error;
    }

error:
    return s;
}

static void
syn_ndp_pcm_rings_reset(struct syn_ndp_s *ndp)
{
    synchronize_irq(ndp->spi_dev.spi->irq);

    WRITE_ONCE(ndp->package_loaded, 0);
    WRITE_ONCE(ndp->audio_frame_step, 0);

    spin_lock(&ndp->send_ring_lock);
    ring_reset(&ndp->send_ring);
    spin_unlock(&ndp->send_ring_lock);

    spin_lock(&ndp->extract_ring_lock);
    ring_reset(&ndp->extract_ring);
    spin_unlock(&ndp->extract_ring_lock);

    wake_up_interruptible(&ndp->send_waitq);

    wake_up_interruptible(&ndp->extract_waitq);
}

static int
ndp120_load(struct syn_ndp_s *ndp120, struct ndp_load_s *load)
{
    uint8_t *package = NULL;
    int package_len, frame_step;
    struct syntiant_ndp120_config_misc_s ndp120_config;
    int s0 = SYNTIANT_NDP_ERROR_NONE;
    int s = 0;

    s = mutex_lock_interruptible(&ndp120->config_mutex);
    if (s) {
        return s;
    }

    if (load->length == 0) {
        pr_debug("%s: zero-length load params\n", __func__);

        syn_ndp_pcm_rings_reset(ndp120);

	    s0 = syntiant_ndp_load(ndp120->ndp, NULL, 0);
	    s = syn_ndp_translate_error(s0);
	    if (s) {
          pr_alert("%s: initialize pkg load error: %s\n",
            __func__, syntiant_ndp_error_name(s0));
	    }
	    goto out;
    }

    package_len = load->length;
    package = syn_ndp_malloc(package_len);
    if (!package) {
        pr_alert("%s: unable to allocate %d bytes for package\n", __func__,
                 package_len);
        s = -ENOMEM;
        goto out;
    }

    if (copy_from_user(package, u64_to_user_ptr(load->package), package_len)) {
        pr_debug("%s: load package protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    pr_debug("** load %s len %d **\n", package, package_len);
    s0 = syntiant_ndp_load(ndp120->ndp, package, package_len);
    if (s0 == SYNTIANT_NDP_ERROR_NONE) {
	    memset(&ndp120_config, 0, sizeof(ndp120_config));
	    s0 = syntiant_ndp120_config_misc(ndp120->ndp, &ndp120_config);
	    if (s0) {
	        pr_err("%s: failed to get ndp120 config data: %s\n", __func__,
		     syntiant_ndp_error_name(s));
	        s = syn_ndp_translate_error(s0);
	        goto out;
	    }
	    frame_step = ndp120_config.audio_frame_step * BYTES_PER_AUDIO_SAMPLE;
	    s = syn_ndp_pcm_rings_init(ndp120, frame_step);
	    if (s) {
	        pr_err("%s: failed to init pcm rings\n", __func__);
	        goto out;
	    }
	    WRITE_ONCE(ndp120->audio_frame_step, frame_step);
    } else {
        if (s0 != SYNTIANT_NDP_ERROR_MORE) {
            pr_info("%s: load package failed: %s\n", __func__,
                    syntiant_ndp_error_name(s0));
        }
        s = syn_ndp_translate_error(s0);
        goto out;
    }

 out:
    if (s0 == SYNTIANT_NDP_ERROR_NONE && ndp120->audio_frame_step) {
      WRITE_ONCE(ndp120->package_loaded, 1);
    }
    mutex_unlock(&ndp120->config_mutex);

    load->error_code = s0;
    if (package) {
        syn_ndp_free(package);
    }
    return s;
}

static int
ndp10x_load(struct syn_ndp_s *ndp10x, struct ndp_load_s *load)
{
    uint8_t *package = NULL;
    int package_len, frame_step;
    struct syntiant_ndp10x_config_s ndp10x_config;
    int s0 = SYNTIANT_NDP_ERROR_NONE;
    int s = 0;

    s = mutex_lock_interruptible(&ndp10x->config_mutex);
    if (s) {
        return s;
    }

    if (load->length == 0) {
        memset(&ndp10x_config, 0, sizeof(ndp10x_config));
        ndp10x_config.get_all = 1;
        s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
        if (s0) {
            pr_err("%s: failed to get ndp10x config data: %s\n", __func__,
                     syntiant_ndp_error_name(s));
            s = syn_ndp_translate_error(s0);
            goto out;
        }
        if (ndp10x_config.fw_pointers_addr) {
            memset(&ndp10x_config, 0, sizeof(ndp10x_config));
            ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
            ndp10x_config.match_per_frame_on = 0;
            s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
            if (s0) {
                pr_err("%s: failed to disable match per frame: %s\n", __func__,
                       syntiant_ndp_error_name(s));
                s = syn_ndp_translate_error(s0);
            }
            syn_ndp_pcm_rings_reset(ndp10x);
        }

        s0 = syntiant_ndp_load(ndp10x->ndp, NULL, 0);
        s = syn_ndp_translate_error(s0);
        if (s) {
            pr_alert("%s: initialize pkg load error: %s\n",
                     __func__, syntiant_ndp_error_name(s0));
        }
        goto out;
    }


    package_len = load->length;
    package = syn_ndp_malloc(package_len);
    if (!package) {
        pr_alert("%s: unable to allocate %d bytes for package\n", __func__,
                 package_len);
        s = -ENOMEM;
        goto out;
    }

    if (copy_from_user(package, u64_to_user_ptr(load->package), package_len)) {
        pr_debug("%s: load package protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    s0 = syntiant_ndp_load(ndp10x->ndp, package, package_len);
    if (s0 == SYNTIANT_NDP_ERROR_NONE) {
        memset(&ndp10x_config, 0, sizeof(ndp10x_config));
        ndp10x_config.get_all = 1;
        s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
        if (s0) {
            pr_err("%s: failed to get ndp10x config data: %s\n", __func__,
                     syntiant_ndp_error_name(s));
            s = syn_ndp_translate_error(s0);
            goto out;
        }
        frame_step = ndp10x_config.audio_frame_step * BYTES_PER_AUDIO_SAMPLE;
        s = syn_ndp_pcm_rings_init(ndp10x, frame_step);
        if (s) {
           pr_err("%s: failed to init pcm rings\n", __func__);
           goto out;
        }
        WRITE_ONCE(ndp10x->audio_frame_step, frame_step);
        if (ndp10x_config.fw_pointers_addr) {
            s0 = syn_ndp_configure_match_per_frame(ndp10x);
            if (s0) {
                pr_err("%s: failed to set match-per-frame: %s\n", __func__,
                       syntiant_ndp_error_name(s0));
            }
            s = syn_ndp_translate_error(s0);
            if (s) {
                goto out;
            }
        }
    } else {
        if (s0 != SYNTIANT_NDP_ERROR_MORE) {
            pr_info("%s: load package failed: %s\n", __func__,
                    syntiant_ndp_error_name(s0));
        }
        s = syn_ndp_translate_error(s0);
        goto out;
    }

 out:
    if (s0 == SYNTIANT_NDP_ERROR_NONE && ndp10x->audio_frame_step) {
      WRITE_ONCE(ndp10x->package_loaded, 1);
    }
    mutex_unlock(&ndp10x->config_mutex);

    load->error_code = s0;
    if (package) {
        syn_ndp_free(package);
    }
    return s;
}

static int
syn_ndp_ioctl_load(struct syn_ndp_s *dev, struct ndp_load_s *load)
{
    if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        return ndp10x_load(dev, load);
    } else if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
        return ndp120_load(dev, load);
    } else {
        pr_info("%s: unknown chip type:%d\n", __func__, dev->chip_type);
        return -EINVAL;
    }
}

static int
ndp_ioctl_transfer(struct syn_ndp_s *dev,
                      struct ndp_transfer_s *transfer)
{
    int s = 0;
    void *buf = NULL;
    void *outbuf = NULL;
    void *inbuf = NULL;

    if (transfer->out && transfer->in) {
        pr_info("%s: duplex transfer requested\n", __func__);
        s = -EINVAL;
        goto out;
    }

    if (!transfer->count) {
        goto out;
    }

    buf = syn_ndp_malloc(transfer->count);
    if (!buf) {
        pr_alert("%s: unable to allocate %d bytes for transfer\n",
                 __func__, transfer->count);
        s = -ENOMEM;
        goto out;
    }

    if (transfer->out) {
        if (copy_from_user(buf, u64_to_user_ptr(transfer->out),
                           transfer->count)) {
            pr_debug("%s: transfer out protection error\n", __func__);
            s = -EACCES;
            goto out;
        }
        outbuf = buf;
    } else {
        inbuf = buf;
    }

    s = syn_ndp_transfer(dev, transfer->mcu, transfer->addr,
                        outbuf, inbuf, transfer->count);
    if (s) {
        pr_info("%s: transfer failed %s\n", __func__,
                syntiant_ndp_error_name(s));
        s = syn_ndp_translate_error(s);
        goto out;
    }

    if (inbuf) {
        if (copy_to_user(u64_to_user_ptr(transfer->in), inbuf, transfer->count)) {
            pr_debug("%s: transfer in protection error\n", __func__);
            s = -EACCES;
            goto out;
        }
    }

 out:
    if (buf) {
        syn_ndp_free(buf);
    }
    return s;
}

static int
ndp_ioctl_pcm_extract(struct syn_ndp_s *dev,
                         struct ndp_pcm_extract_s *extract)
{
    int extract_size;
    int s = 0;
    int extracted = 0;
    int remaining = 0;
    int all;
    int ioctl_size = extract->buffer_length;
    int block = !extract->nonblock;
    void *buf = NULL;
    struct ndp_ring_s *extract_ring = &dev->extract_ring;

    pr_debug("%s: buffer 0x%llx length: %d, nonblock: %d, flush: %d"
             ", unbuffered: %d, type: %u, from: %u\n",
             __func__, extract->buffer, extract->buffer_length,
             extract->nonblock, extract->flush,
             extract->unbuffered, extract->type, extract->from);

    if (extract->unbuffered) {
	if (s) {
	    pr_debug("%s: unbuffered mutex interrupted\n", __func__);
	    goto unbuf_out;
	}

        if (extract->flush) {
            s = -EINVAL;
	    goto unbuf_out;
	}

        if (block) {
            s = -EINVAL;
	    goto unbuf_out;
	}

	if (extract->buffer_length < 0) {
	    s = -EINVAL;
	    goto unbuf_out;
	}

    if (extract->buffer_length) {
        buf = kmalloc(extract->buffer_length, GFP_KERNEL);
        if (!buf) {
            s = -ENOMEM;
            goto unbuf_out;
        }
    }

    extract_size = extract->buffer_length;
	s = mutex_lock_interruptible(&dev->unbuffered_extract_mutex);
	s = syntiant_ndp_extract_data
	    (dev->ndp, extract->type, extract->from, buf,
	     &extract_size);
	if (s) {
	    pr_err("%s: unbuffered extract of len %d failed: %s\n",
		   __func__, extract->buffer_length,
		   syntiant_ndp_error_name(s));
            s = syn_ndp_translate_error(s);
            goto unbuf_out;
	}

    all = extract->buffer_length <= extract_size;
    extract->extracted_length =
        all ? extract->buffer_length : extract_size;
    extract->remaining_length =
        extract_size - extract->buffer_length;

    if (buf) {
	    if (copy_to_user(u64_to_user_ptr(extract->buffer), buf,
                         extract->extracted_length)) {
	        pr_debug("%s: extract buffer access error\n", __func__);
	        s = -EACCES;
        }
	}

    unbuf_out:
	mutex_unlock(&dev->unbuffered_extract_mutex);
	if (buf)
	    kfree(buf);
	return s;

    } else {
        if (extract->type != SYNTIANT_NDP_EXTRACT_TYPE_INPUT) {
	    s = -EINVAL;
	    goto out;
	}
    }

    s = mutex_lock_interruptible(&dev->extract_ioctl_mutex);
    if (s) {
        pr_debug("%s: mutex interrupted\n", __func__);
        goto out;
    }

    if (extract->flush) {
        spin_lock(&dev->extract_ring_lock);
        ring_reset(extract_ring);
        spin_unlock(&dev->extract_ring_lock);
    }

    while (extracted < ioctl_size) {
        extract_size = ioctl_size - extracted;
        WRITE_ONCE(dev->extract_waiting, extract_size);
        if (block) {
            s = wait_event_interruptible
                (dev->extract_waitq,
                 !READ_ONCE(dev->package_loaded)
                 || ring_cnt(extract_ring));
            if (s == -ERESTARTSYS) {
                pr_debug("%s: extract wait interrupted\n", __func__);
                break;
            } else if (s) {
                pr_err("%s: extract wait error: %d\n", __func__, -s);
                break;
            }
        } else if (ring_empty(extract_ring)) {
            break;
        }

        if (!READ_ONCE(dev->package_loaded)) {
            s = -EINVAL;
            break;
        }

        extract_size = min(extract_size, dev->extract_scratch_size);
        spin_lock(&dev->extract_ring_lock);
        extract_size = min(extract_size, ring_cnt_to_end(extract_ring));
        buf = ring_consumer(extract_ring);
        memcpy(dev->extract_scratch, buf, extract_size);
        ring_remove(extract_ring, extract_size);
        spin_unlock(&dev->extract_ring_lock);
        if (extract->buffer) {
            if (copy_to_user(u64_to_user_ptr(extract->buffer + extracted),
                             dev->extract_scratch,
                             extract_size)) {
                pr_debug("%s: extract buffer access error\n", __func__);
                s = -EACCES;
                break;
            }
        }
        extracted += extract_size;
    }

    spin_lock(&dev->extract_ring_lock);
    remaining = ring_cnt(extract_ring);
    spin_unlock(&dev->extract_ring_lock);

    WRITE_ONCE(dev->extract_waiting, 0);
    mutex_unlock(&dev->extract_ioctl_mutex);

 out:
    extract->extracted_length = extracted;
    extract->remaining_length = remaining;
    return s;
}

static int
ndp10x_ioctl_pcm_send(struct syn_ndp_s *ndp10x, struct ndp_pcm_send_s *send)
{
    int send_size, used;
    int sent = 0;
    int block = !send->nonblock;
    int ioctl_size = send->buffer_length;
    int s = 0;
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;

    s = mutex_lock_interruptible(&ndp10x->send_ioctl_mutex);
    if (s) {
        pr_debug("%s: mutex interrupted\n", __func__);
        goto out;
    }

    WRITE_ONCE(ndp10x->send_waiting, ioctl_size);

    while (sent < ioctl_size) {
        if (block) {
            s = wait_event_interruptible
                (ndp10x->send_waitq,
                 !READ_ONCE(ndp10x->package_loaded) || ring_space(send_ring));
            if (s == -ERESTARTSYS) {
                pr_debug("%s: send wait interrupted\n", __func__);
                break;
            } else if (s) {
                pr_err("%s: send wait error: %d\n", __func__, -s);
                break;
            }
        } else if (READ_ONCE(ndp10x->package_loaded)
                   && !ring_space(send_ring)) {
            break;
        }

        if (!READ_ONCE(ndp10x->package_loaded)) {
            s = -EINVAL;
            break;
        }

        send_size = ring_space_to_end(send_ring);
        send_size = min(ioctl_size - sent, send_size);
        if (copy_from_user(ring_producer(send_ring),
                           u64_to_user_ptr(send->buffer + sent),
                           send_size)) {
            pr_debug("%s: send buffer access error\n", __func__);
            s = -EACCES;
            break;
        }

        sent += send_size;
        WRITE_ONCE(ndp10x->send_waiting, ioctl_size - sent);

        pr_debug("%s: adding %d bytes, idle: %d\n", __func__, send_size,
                 ring_empty(send_ring));

        spin_lock(&ndp10x->send_ring_lock);
        used = ring_cnt(send_ring);
        ring_add(send_ring, send_size);
        spin_unlock(&ndp10x->send_ring_lock);
        if (used < ndp10x->audio_frame_step
            && ndp10x->audio_frame_step <= used + send_size ) {
            irq_wake_thread(ndp10x->spi_dev.spi->irq, &ndp10x->spi_dev);
        }
    }

    if (sent < ioctl_size) {
        WRITE_ONCE(ndp10x->send_waiting, 0);
    }
    mutex_unlock(&ndp10x->send_ioctl_mutex);

 out:
    send->sent_length = sent;
    return s;
}

static int ndp10x_watch_sensor(struct syn_ndp_s *ndp10x,
    struct ndp_watch_s *watch)
{
    int s = 0;
    long timeout = watch->timeout;
    struct ndp_ring_s *sensor_result_ring = &ndp10x->sensor_result_ring;
    uint64_t classes = watch->classes;
    struct ndp_result_s *result;
    uint32_t summary;
    int match;
    int class_index;
    int empty;
    int found = 0;

    for (;;) {
        if (timeout < 0) {
            s = wait_event_interruptible(ndp10x->sensor_result_waitq,
                                         !ring_empty(sensor_result_ring));
        } else if (timeout) {
            s = wait_event_interruptible_timeout
                (ndp10x->sensor_result_waitq, !ring_empty(sensor_result_ring), timeout);
            if (0 <= s) {
                timeout = s;
                s = 0;
            }
        }
        spin_lock(&ndp10x->sensor_result_ring_lock);
        empty = ring_empty(sensor_result_ring);
        spin_unlock(&ndp10x->sensor_result_ring_lock);

        if (!timeout && empty) {
            s = -ETIME;
            pr_debug("%s: !timeout & result ring empty - returning %d\n",
                     __func__, s);
            break;
        }

        if (s == -ERESTARTSYS) {
            pr_debug("%s: watch wait interrupted\n", __func__);
            break;
        } else if (s == -ETIME) {
            pr_debug("%s: result wait timeout\n", __func__);
            break;
        } else if (s) {
            pr_err("%s: watch wait error: %d\n", __func__, -s);
            break;
        }
        spin_lock(&ndp10x->sensor_result_ring_lock);
        if (!ring_empty(sensor_result_ring)) {
          result = ring_consumer(sensor_result_ring);
          summary = result->summary;
          class_index = NDP10X_SPI_MATCH_WINNER_EXTRACT(summary);
          match = !!(summary & NDP10X_SPI_MATCH_MATCH_MASK);
          if ((match && (classes & (uint64_t) (1ULL << class_index)))) {
            found = 1;
            ring_remove(sensor_result_ring, 1);
          } else {
            found = 0;
          }
        }
        spin_unlock(&ndp10x->sensor_result_ring_lock);

        if (found) {
            pr_debug("%s Thread (%d): s:%d returning match, 0x%x\n",
            __func__, current->pid, s, summary);
            break;
        }
    }

    if (found) {
        watch->ts.tv_sec = result->ts.tv_sec;
        watch->ts.tv_nsec = result->ts.tv_nsec;
        watch->match = match;
        watch->class_index = class_index;
        watch->info = summary >> NDP10X_MATCH_INFO_SHIFT;
    }
    return s;
}

static int
ndp120_ioctl_watch_config(struct syn_ndp_s *dev, int match_per_frame)
{
    int s = 0;
    struct syntiant_ndp120_config_misc_s ndp120_config;

    memset(&ndp120_config, 0, sizeof(ndp120_config));
    ndp120_config.set = NDP120_CONFIG_SET_MISC_MATCH_PER_FRAME_ON;
    ndp120_config.match_per_frame_on = match_per_frame;
    s = syntiant_ndp120_config_misc(dev->ndp, &ndp120_config);
    if (s) {
        pr_err("%s: failed to set match per frame %s\n", __func__,
               syntiant_ndp_error_name(s));
    }
    return s;
}

static int
ndp10x_ioctl_watch_config(struct syn_ndp_s *dev, int match_per_frame)
{
    int s = 0;
    struct syntiant_ndp10x_config_s ndp10x_config;

    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
    ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
    ndp10x_config.match_per_frame_on = match_per_frame;
    s = syntiant_ndp10x_config(dev->ndp, &ndp10x_config);
    if (s) {
        pr_err("%s: failed to set match per frame %s\n", __func__,
               syntiant_ndp_error_name(s));
    }
    return s;
}

static int
ndp_ioctl_watch(struct syn_ndp_s *dev, struct ndp_watch_s *watch)
{
    int s = 0;
    struct ndp_ring_s *result_ring = &dev->result_ring;
    long timeout = watch->timeout;
    uint64_t classes = watch->classes;
    struct ndp_result_s *result;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
    struct timespec ts;
#else
    struct timespec64 ts;
#endif
    uint32_t summary;
    unsigned int info;
    int match;
    int class_index;
    int armed_mode = watch->extract_match_mode;
    int armed_watch_active = READ_ONCE(dev->armed_watch_active);
    uint32_t avail;
    uint8_t noise_level = 0;
    uint32_t addr = 0;
    int32_t noise_data;
    int match_per_frame = 0;
    int s0;

    pr_debug("%s: armed: %d, armed_active: %d, timeout: %ld, flush: %d"
             ", classes %llu\n",
             __func__, armed_mode, armed_watch_active, timeout, watch->flush,
             classes);

    timeout *= HZ;

    watch->match = 0;

    if (watch->flush) {
        spin_lock(&dev->result_ring_lock);
        ring_reset(result_ring);
        spin_unlock(&dev->result_ring_lock);
        if (watch->extract_match_mode) {
            WRITE_ONCE(dev->armed, 0);
            WRITE_ONCE(dev->armed_watch_active, 0);
        }
        goto out;
    }

    if (!armed_mode && armed_watch_active) {
        s = -EBUSY;
        pr_debug("%s: already armed, returning BUSY\n", __func__);
        goto out;
    }

    if (armed_mode && !armed_watch_active) {
        pr_debug("%s: arming\n", __func__);

        dev->extract_before_match = watch->extract_before_match *
            NDP10X_BYTES_PER_MILLISECOND;
        WRITE_ONCE(dev->armed, 1);
        synchronize_irq(dev->spi_dev.spi->irq);
        spin_lock(&dev->extract_ring_lock);
        ring_reset(&dev->extract_ring);
        spin_unlock(&dev->extract_ring_lock);
        wake_up_interruptible(&dev->extract_waitq);
        WRITE_ONCE(dev->armed_watch_active, 1);
    }

    match_per_frame = !armed_mode || READ_ONCE(dev->pcm_input);
    if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        s = ndp10x_ioctl_watch_config(dev, match_per_frame);
    } else if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
        s = ndp120_ioctl_watch_config(dev, match_per_frame);
    } else {
        pr_err("%s: Unknown chip type (0x%x)\n", __func__, dev->chip_type);
        s = -EINVAL;
    }
    if (s) goto out;

    for (;;) {
        /* FIXME */
        if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
            s = syntiant_ndp120_init_ring_buffer_pointers(dev->ndp);
            if (s) {
                pr_err("%s: failed to init ring buffer pointers %s\n", __func__,
                    syntiant_ndp_error_name(s));
                goto out;
            }
        }
        if (timeout < 0) {
            s = wait_event_interruptible(dev->result_waitq,
                                         !ring_empty(result_ring));
        } else if (timeout) {
            s = wait_event_interruptible_timeout
                (dev->result_waitq, !ring_empty(result_ring), timeout);
            if (0 <= s) {
                timeout = s;
                s = 0;
            }
        }

        if (!timeout && ring_empty(result_ring)) {
            s = -ETIME;
            pr_debug("%s: !timeout & result ring empty - returning %d\n",
                     __func__, s);
            break;
        }

        if (s == -ERESTARTSYS) {
            pr_debug("%s: watch wait interrupted\n", __func__);
            break;
        } else if (s == -ETIME) {
            pr_debug("%s: result wait timeout\n", __func__);
            break;
        } else if (s) {
            pr_err("%s: watch wait error: %d\n", __func__, -s);
            break;
        }

        spin_lock(&dev->result_ring_lock);
        result = ring_consumer(result_ring);
        ts = result->ts;
        avail = result->extract_len_avail;
        summary = result->summary;
        ring_remove(result_ring, 1);
        spin_unlock(&dev->result_ring_lock);
        match = !!(summary & NDP10X_SPI_MATCH_MATCH_MASK);

        if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
            noise_level = (summary & NDP10X_MATCH_NOISE_THRESHOLD_MASK)
                                >> NDP10X_MATCH_NOISE_THRESHOLD_SHIFT;
            if (noise_level) {
                pr_debug("%s: noise crossing %s threshold\n", __func__,
                    noise_level == NDP10X_NOISE_HI_THRESHOLD_MARK? "over":"below");
                addr = syntiant_ndp10x_get_fw_agc_addr(dev->ndp) +
                  (uint32_t) offsetof(struct ndp10x_agc_state_s, noise_level_data);
                s = syntiant_ndp10x_read(dev->ndp, 1, addr, &noise_data);
                if (s) {
                    pr_err("%s: Error in reading noise level\n", __func__);
                    break;
                }
                pr_debug("%s: noise data: 0x%x\n", __func__, noise_data);
            }
        }

        class_index = NDP10X_SPI_MATCH_WINNER_EXTRACT(summary);
        info = summary >> NDP10X_MATCH_INFO_SHIFT;
        pr_debug("%s: match %d, class_index: %d, noise_level: %d, rpf: %d\n",
                 __func__, match, class_index, noise_level,
                 dev->result_per_frame);
        if ((match && (classes & (uint32_t) (1U << class_index)))
            || noise_level
            || dev->result_per_frame) {
            pr_debug("%s: returning match\n", __func__);
            break;
        }
    }

    if (!s) {
        watch->ts.tv_sec = ts.tv_sec;
        watch->ts.tv_nsec = ts.tv_nsec;
        watch->match = match;
        watch->class_index = class_index;
        watch->info = info;
        watch->extract_before_match = avail / NDP10X_BYTES_PER_MILLISECOND;
        if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
            watch->noise_level = noise_level;
            watch->noise_data = noise_data;
        }
    }

    if (armed_mode && s != -ERESTARTSYS) {
        pr_debug("%s: disarming\n", __func__);
        WRITE_ONCE(dev->armed, 0);
        WRITE_ONCE(dev->armed_watch_active, 0);

        if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
            s0 = ndp10x_ioctl_watch_config(dev, 1);
        } else if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
            s0 = ndp120_ioctl_watch_config(dev, 1);
        } else {
            /* Should never happen */
            s = -EINVAL;
        }
        if (!s) {
            s = syn_ndp_translate_error(s0);
        }
    }

 out:
    return s;
}

#ifdef CONFIG_DEBUG_FS
static int dump_registers(struct syn_ndp_s *ndp120, struct seq_file *m)
{
    int s = 0;
    uint32_t value = 0;

    s = syntiant_ndp120_read(ndp120->ndp, 1,
            NDP120_DSP_CONFIG_BUFFILLLEVEL(0), &value);
    if (s) {
        pr_err("Can't read buffillevel(0) %d\n", s);
        goto error;
    }
    seq_printf(m, "Buff fill level (pdm fifo_0): 0x%x\n",
            NDP120_DSP_CONFIG_BUFFILLLEVEL_BUF_FILL_LEVEL_EXTRACT(value));
    s = syntiant_ndp120_read(ndp120->ndp, 1,
            NDP120_DSP_CONFIG_BUFFILLLEVEL(1), &value);
    if (s) {
        pr_err("Can't read buffillevel(1) %d\n", s);
        goto error;
    }
    seq_printf(m, "Buff fill level (pdm fifo_1): 0x%x\n",
            NDP120_DSP_CONFIG_BUFFILLLEVEL_BUF_FILL_LEVEL_EXTRACT(value));
    s = syntiant_ndp120_read(ndp120->ndp, 1, NDP120_DSP_CONFIG_BUFIRQSTAT, &value);
    if (s) {
        pr_err("Can't read buffirqstat %d\n", s);
        goto error;
    }
    seq_printf(m,"Buffer irq stat: 0x%x\n",
            NDP120_DSP_CONFIG_BUFIRQSTAT_STAT_BUF_FILL_ABOVE_THRESHOLD_EXTRACT(value));
    s = syntiant_ndp120_read(ndp120->ndp, 1, NDP120_DNN_CONFIG_DNNCTL,
            &value);
    if (s) {
        pr_err("Can't read DNNCTL%d\n", s);
        goto error;
    }
    seq_printf(m,"DNN ctl: 0x%x\n", value);
    s = syntiant_ndp120_read(ndp120->ndp, 1, NDP120_DNN_CONFIG_DNNSTS0, &value);
    if (s) {
        pr_err("Can't read DNNSTS0%d\n", s);
        goto error;
    }
    seq_printf(m,"DNNSTS0 running:0x%x layer start:0x%x layer_done:0x%x error:0x%x\n",
            NDP120_DNN_CONFIG_DNNSTS0_RUNNING_EXTRACT(value),
            NDP120_DNN_CONFIG_DNNSTS0_LAYER_START_EXTRACT(value),
            NDP120_DNN_CONFIG_DNNSTS0_LAYER_DONE_EXTRACT(value),
            NDP120_DNN_CONFIG_DNNSTS0_ERROR_CODE_EXTRACT(value));
error:
    return s;
}

static int dump_mcu_debug_state(struct syn_ndp_s *ndp120, struct seq_file *m)
{
    int s = 0;
    struct ndp120_debug_cnt_s dbg;
    struct ndp120_mb_state_s mb_state;
    uint32_t addr = syntiant_ndp120_get_mcu_dbg_state_addr(ndp120->ndp);

    if (!addr) {
        pr_err("Can't get valid debug state address\n");
        goto out;
    }
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &dbg, sizeof(dbg));
    if (s) {
        pr_err("Can't read debug state (%d)\n", s);
        goto out;
    }
    seq_printf(m,"signature: 0x%x\n", dbg.signature);
    seq_printf(m,"frame count: %d\n", dbg.frame_cnt);
    seq_printf(m,"dsp2mcu int count: %d\n", dbg.dsp2mcu_intr_cnt);
    seq_printf(m,"dsp2mcu dnn int count: %d\n", dbg.dsp2mcu_nn_done_cnt);
    seq_printf(m,"mcu2host match count: %d\n", dbg.mcu2host_match_cnt);
    seq_printf(m,"mcu2host mpf count: %d\n", dbg.mcu2host_mpf_cnt);
    seq_printf(m, "mcu2dsp done cnt:%d\n", dbg.mcu2dsp_done_cnt);
    seq_printf(m,"matches: %d\n", dbg.matches);
    seq_printf(m, "dsp2mcu queue cnt:%d\n", dbg.dsp2mcu_queue_cnt);
    seq_printf(m, "nn orch flowchange cnt:%d\n", dbg.nn_orch_flwchg_cnt);
    seq_printf(m, "Unknown activation cnt:%d\n", dbg.unknown_activation_cnt);
    seq_printf(m, "Unknown interrupt cnt:%d\n", dbg.unknown_int_count);
    seq_printf(m, "Invalid nn orchestrator node cnt:%d\n", dbg.inv_nn_orch_node_cnt);
    seq_printf(m,"mbin int count: %d\n", dbg.mbin_int_cnt);
    seq_printf(m,"mbout int count: %d\n", dbg.mbout_int_cnt);
    seq_printf(m,"number of frames: %d\n", dbg.num_frames);

    addr = syntiant_ndp120_get_mcu_fw_pointer(ndp120->ndp);
    addr += (uint32_t) offsetof(struct ndp120_fw_state_s, mb_state);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &mb_state,
            sizeof(mb_state));
    if (s) {
        pr_err("Can't read MCU fw mb_state (%d)\n", s);
        goto out;
    }
    seq_printf(m, "MCU fw mb state:\n");
    seq_printf(m, "enable_match_for_every_frame: %d\n",
            mb_state.enable_match_for_every_frame);

out:
    return s;
}

static void dump_flow_rules(struct seq_file *m,
        ndp120_dsp_data_flow_setup_t *flow_rules)
{
    int i;
    ndp120_dsp_data_flow_rule_t *rule;

    seq_printf(m, "  Flow Rules:  \n");
    for (i = 0; i < ARRAY_LEN(flow_rules->src_pcm_audio); i++) {
        rule = &flow_rules->src_pcm_audio[i];
        if (NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) {
            seq_printf(m, "[%d] %s%d-->%s%d\n", rule->set_id,
                NDP120_DSP_DATA_FLOW_SRC_TYPE_STR(
                    NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO),
                    rule->src_param,
                    NDP120_DSP_DATA_FLOW_RULE_DST_STR(*rule), rule->dst_param);
        }
    }
    for (i = 0; i < ARRAY_LEN(flow_rules->src_function); i++) {
        rule = &flow_rules->src_function[i];
        if (NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) {
            seq_printf(m, "[%d] %s%d-->%s%d\n", rule->set_id,
                NDP120_DSP_DATA_FLOW_SRC_TYPE_STR(
                    NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION),
                    rule->src_param,
                    NDP120_DSP_DATA_FLOW_RULE_DST_STR(*rule), rule->dst_param);
        }
    }
    for (i = 0; i < ARRAY_LEN(flow_rules->src_function); i++) {
        rule = &flow_rules->src_nn[i];
        if (NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) {
            seq_printf(m, "[%d] %s%d-->%s%d\n", rule->set_id,
                NDP120_DSP_DATA_FLOW_SRC_TYPE_STR(
                    NDP120_DSP_DATA_FLOW_SRC_TYPE_NN),
                    rule->src_param,
                    NDP120_DSP_DATA_FLOW_RULE_DST_STR(*rule), rule->dst_param);
        }
    }
}

static int dump_dsp_debug_state(struct syn_ndp_s *ndp120, struct seq_file *m)
{
    int s = 0;
    ndp120_dsp_counters_t counters;
    uint32_t addr = 0;
    int i, len;
    ndp120_dsp_config_t config;
    ndp120_dsp_fw_base_t *dsp_fw_state;
    ndp120_dsp_data_flow_setup_t flow_rules;
    ndp120_dsp_fw_aud2_config_t aud2_config;
    ndp120_dsp_audio_sync_config_t audio_sync_config;
    ndp120_dsp_algo_t algos;
    uint32_t magic, flow_current_set_id;

    if (!ndp120->ndp->d.ndp120.dsp_fw_state_addr) {
        /* FW hasn't been loaded yet, return */
        return 0;
    }
    len = sizeof(ndp120_dsp_counters_t);
    memset(&counters, 0, len);
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
        offsetof(ndp120_dsp_fw_base_t, counters);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &counters, len);
    if (s) {
        pr_err("Error reading DSP counters, %d\n", s);
        goto out;
    }
    dsp_fw_state = (ndp120_dsp_fw_base_t *) &ndp120->ndp->d.ndp120.dsp_fw_state_addr;
    syntiant_debug( "magic: 0x%x\n", dsp_fw_state->magic);
    syntiant_debug( "data_flow_current_set_id: 0x%x\n", dsp_fw_state->data_flow_current_set_id);
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
        offsetof(ndp120_dsp_fw_base_t, magic);
    len = sizeof(uint32_t);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &magic, len);
    if (s) {
        pr_err("Error reading DSP FW state magic cookie, %d\n", s);
        goto out;
    }
    seq_printf(m, "** DSP FW State **\n");
    seq_printf(m, "magic: 0x%x\n", magic);
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
        offsetof(ndp120_dsp_fw_base_t, data_flow_current_set_id);
    len = sizeof(uint32_t);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr,
            &flow_current_set_id, len);
    if (s) {
        pr_err("Error reading DSP FW state magic cookie, %d\n", s);
        goto out;
    }
    seq_printf(m, "data_flow_current_set_id: 0x%x\n", flow_current_set_id);
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
        offsetof(ndp120_dsp_fw_base_t, data_flow);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &flow_rules,
            sizeof(flow_rules));
    if (s) {
        pr_err("Error reading flow rules in DSP firmware, %d\n", s);
        goto out;
    }
    dump_flow_rules(m, &flow_rules);
    //TODO fill in dsp fw state
    seq_printf(m,"* DSP counters *\n");
    seq_printf(m,"frame count:%d\n", counters.frame_cnt);
    seq_printf(m,"dnn int count:%d\n", counters.dnn_int_cnt);
    seq_printf(m,"h2d mb count:%d\n", counters.h2d_mb_cnt);
    seq_printf(m,"d2m mb count:%d\n", counters.d2m_mb_cnt);
    seq_printf(m,"m2d mb count:%d\n", counters.m2d_mb_cnt);
    seq_printf(m,"watermark count:%d\n", counters.watermark_cnt);
    seq_printf(m,"fifo overflow count:%d\n", counters.fifo_overflow_cnt);
    seq_printf(m,"* DSP Config: *\n");
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
        offsetof(ndp120_dsp_fw_base_t, config);
    len = sizeof(ndp120_dsp_config_t);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &config, len);
    if (s) {
        pr_err("Error reading DSP config, %d\n", s);
        goto out;
    }
    seq_printf(m,"audio sample size (bytes):%d\n", config.aud_samp_size_bytes);
    seq_printf(m,"func sample size (bytes):%d\n", config.func_samp_size_bytes);
    seq_printf(m,"audio sample capacity:%d\n", config.aud_samp_cap);
    seq_printf(m,"func sample capacity:%d\n", config.func_samp_cap);
    seq_printf(m,"notify on sample ready:%d\n", config.notify_on_sample_ready);
    for (i = 0; i < FIFO_CNT; i++) {
        seq_printf(m, "fifo threshold[%d]:%d\n", i,
                config.fifo_threshold_bytes[i]);
    }
    seq_printf(m,"* AUD2 Configurations *\n");
    addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr;
    addr += offsetof(ndp120_dsp_fw_state_t, aud2_config);
    len = sizeof(aud2_config);
    s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &aud2_config, len);
    if (s) {
        pr_err("Error reading DSP fw aud2 config: %d\n", s);
        goto out;
    }
     seq_printf(m,"src_type: %d\n", aud2_config.src_type);
     seq_printf(m,"src_param: %d\n", aud2_config.src_param);
     seq_printf(m,"* Audio Sync Configurations *\n");
     addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr +
         offsetof(ndp120_dsp_fw_state_t, audio_sync_config);
     len = sizeof(audio_sync_config);
     s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &audio_sync_config, len);
     if (s) {
         pr_err("Error reading DSP fw audio sync config: %d\n", s);
         goto out;
     }
     seq_printf(m,"enable: %d\n", audio_sync_config.enable);
     seq_printf(m,"started: %d\n", audio_sync_config.started);
     seq_printf(m,"offset: %d\n", audio_sync_config.sample_count_offset);
     seq_printf(m,"Ref channel: %d\n", audio_sync_config.ref_chan);
     seq_printf(m,"Adj channel: %d\n", audio_sync_config.adj_chan);
     seq_printf(m,"* DSP Algo (Enabled) Configurations *\n");
     for (i = 0; i < NDP120_DSP_ALGO_MAX_COUNT; i++) {
         addr = ndp120->ndp->d.ndp120.dsp_fw_state_addr;
         addr += offsetof(ndp120_dsp_fw_state_t, algos) + i * sizeof(algos);
         len = sizeof(algos);
         s = syntiant_ndp120_read_block(ndp120->ndp, 1, addr, &algos, len);
         if (s) {
            pr_err("Error reading DSP fw audio algo config: %d\n", s);
            goto out;
         }
         if (0 <= algos.algo_config_index) {
            seq_printf(m,"algo_id: %d\n ", algos.algo_id);
            seq_printf(m,"algo_config_index: %d\n ", algos.algo_config_index);
            seq_printf(m,"algo_init_status: %d\n ", algos.algo_init_status);
         }
     }
out:
    return s;
}
#endif

static int
ndp120_ioctl_flow_setup(struct syn_ndp_s *dev, syntiant_ndp120_flow_setup_t *driver_flow_setup)
{

    int s = 0;

    enum syntiant_ndp120_flow_setup_e action = driver_flow_setup->action;
    ndp120_dsp_data_flow_setup_t *flow_setup = &driver_flow_setup->flow_setup;
    ndp120_dsp_data_flow_rule_t *rule = &driver_flow_setup->rule;
    uint32_t src_type = driver_flow_setup->src_type;

    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        return s;
    }

    switch (action) {
        case NDP120_FLOW_SETUP_RESET:
            s = syntiant_ndp120_dsp_flow_setup_reset(flow_setup);
            break;

        case NDP120_FLOW_SETUP_ADD_RULE:
            s = syntiant_ndp120_dsp_flow_setup_add_rule(flow_setup, rule, src_type);
            break;

        case NDP120_FLOW_SETUP_APPLY:
            s = syntiant_ndp120_dsp_flow_setup_apply(dev->ndp, flow_setup);
            if (s) {
                pr_err("%s: Error applying flow setup, %d\n", __func__, s);
                goto out;
            }
            break;

        default:
            pr_err("%s: Invalid action\n", __func__);
            s = -EINVAL;
            goto out;
    }

    s = syn_ndp_translate_error(s);

out:
    mutex_unlock(&dev->config_mutex);

    return s;
}

static int
ndp_ioctl_driver_config(struct syn_ndp_s *dev,
                           struct ndp_driver_config_s *driver_config)
{
    int s = 0;

    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        return s;
    }

    dev->result_per_frame = driver_config->result_per_frame;
    if (driver_config->spi_speed) {
        dev->spi_dev.spi_speed = driver_config->spi_speed;
        dev->spi_dev.spi_read_pad_bytes = driver_config->spi_read_pad_bytes;
    }
    if (driver_config->spi_send_speed) {
        dev->spi_dev.spi_send_speed = driver_config->spi_send_speed;
    }

    /* TODO the rest */
    mutex_unlock(&dev->config_mutex);
    return s;
}

static int
syn_ndp_serial_transfer(struct syn_ndp_s *ndp10x, struct ndp10x_serial_s *serial,
        uint8_t *out, uint8_t *in)
{
    int s = 0;

    s = mutex_lock_interruptible(&ndp10x->serial_mutex);
    if (s) {
        pr_debug("%s: can't acquire lock, %d\n", __func__, s);
        goto out;
    }

    s = syntiant_ndp10x_serial_transfer(ndp10x->ndp,
                    serial->ifc_type,
                    serial->ifc_addr,
                    out,
                    serial->outlen,
                    in,
                    serial->inlen,
                    serial->continue_);

    mutex_unlock(&ndp10x->serial_mutex);
    s = syn_ndp_translate_error(s);

out:
    return s;
}

static int
ndp10x_ioctl_statistics(struct syn_ndp_s *ndp10x,
                        struct syn_ndp_statistics_s *statistics)
{
    statistics->isrs = ndp10x->isrs;
    statistics->polls = ndp10x->polls;
    statistics->frames = ndp10x->frames;
    statistics->results = ndp10x->results;
    statistics->results_dropped = ndp10x->results_dropped;
    spin_lock(&ndp10x->result_ring_lock);
    statistics->result_ring_used = ring_cnt(&ndp10x->result_ring);
    spin_unlock(&ndp10x->result_ring_lock);
    statistics->extracts = ndp10x->extracts;
    statistics->extract_bytes = ndp10x->extract_bytes;
    statistics->extract_bytes_dropped = ndp10x->extract_bytes_dropped;
    spin_lock(&ndp10x->extract_ring_lock);
    statistics->extract_ring_used = ring_cnt(&ndp10x->extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);
    statistics->sends = ndp10x->sends;
    statistics->send_bytes = ndp10x->send_bytes;
    spin_lock(&ndp10x->send_ring_lock);
    statistics->send_ring_used = ring_cnt(&ndp10x->send_ring);
    spin_unlock(&ndp10x->send_ring_lock);
    if (statistics->clear) {
        ndp10x->isrs = 0;
        ndp10x->polls = 0;
        ndp10x->frames = 0;
        ndp10x->results = 0;
        ndp10x->results_dropped = 0;
        ndp10x->extracts = 0;
        ndp10x->extract_bytes = 0;
        ndp10x->extract_bytes_dropped = 0;
        ndp10x->sends = 0;
        ndp10x->send_bytes = 0;
    }

    return 0;
}

static int
ndp10x_ioctl_serial(struct syn_ndp_s *ndp10x, struct ndp10x_serial_s *serial)
{

    int s = 0;
    u8 *inbuf = NULL, *outbuf = NULL;

    if (!serial->inlen && !serial->outlen)
        return 0;

    if (serial->outlen) {
        outbuf = kmalloc(serial->outlen, GFP_KERNEL);
        if (!outbuf) {
            s = -ENOMEM;
            goto out;
        }

        if (copy_from_user(outbuf, u64_to_user_ptr(serial->out),
                           serial->outlen)) {
            s = -EFAULT;
            goto out;
        }
    };

    if (serial->inlen) {
        inbuf = kmalloc(serial->inlen, GFP_KERNEL);
        if (!inbuf) {
            s =  -ENOMEM;
            goto out;
        }
    }

    s = syn_ndp_serial_transfer(ndp10x, serial, outbuf, inbuf);
    if (s) {
        if (!serial->muted_) {
            pr_err("%s: Error in serial transfer:%d\n", __func__, s);
            if (outbuf && inbuf) {
                pr_err("%s: Address: 0x%x Type: %d Out:0x%x In:0x%x\n", __func__,
                    serial->ifc_addr, serial->ifc_type, outbuf[0], inbuf[0]);
            } else if (outbuf) {
                pr_err("%s: Address: 0x%x Type: %d Out:0x%x\n", __func__,
                    serial->ifc_addr, serial->ifc_type, outbuf[0]);
            } else if (inbuf) {
                pr_err("%s: Address: 0x%x Type: %d In:0x%x\n", __func__,
                    serial->ifc_addr, serial->ifc_type, inbuf[0]);
            }
        }
        goto out;
    }
    if (serial->inlen) {
        if (copy_to_user(u64_to_user_ptr(serial->in), inbuf, serial->inlen)) {
            s = -EFAULT;
            goto out;
        }
    }

out:
    if (outbuf)
        kfree(outbuf);
    if (inbuf)
        kfree(inbuf);

    return s;
}

static int
ndp10x_ioctl_gpio(struct syn_ndp_s *ndp10x, struct syntiant_ndp10x_gpio_s *gpio)
{
    int s;

    s = mutex_lock_interruptible(&ndp10x->config_mutex);
    if (s) {
        return s;
    }

    s = syntiant_ndp10x_gpio(ndp10x->ndp, gpio);
    s = syn_ndp_translate_error(s);

    mutex_unlock(&ndp10x->config_mutex);

    return s;
}

union ioctl_arg_u {
    struct syntiant_ndp10x_config_s ndp10x_config;
    struct ndp_config_s ndp_config;
    struct ndp_load_s load;
    struct ndp_watch_s watch;
    struct ndp_transfer_s transfer;
    struct ndp_driver_config_s driver_config;
    struct ndp_pcm_extract_s pcm_extract;
    struct ndp_pcm_send_s pcm_send;
    struct syn_ndp_statistics_s statistics;
    struct ndp10x_serial_s serial;
    struct syntiant_ndp10x_gpio_s gpio;
    struct syntiant_ndp120_config_misc_s ndp120_config;
    struct syntiant_ndp120_config_other_s ndp120_config_other;
    struct ndp120_init_s init;
    struct syntiant_ndp120_flow_setup_s ndp120_flow_setup;
};

static int
ndp120_ioctl_config(struct syn_ndp_s * dev,
			    struct syntiant_ndp120_config_misc_s *ndp120_config)
{
    int s;

    if (dev->chip_type != SYNTIANT_NDP_CORE_2)
        return -EINVAL;

    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        printk("couldn't get mutex\n");
        return s;
    }

    s = syntiant_ndp120_config_misc(dev->ndp, ndp120_config);
    if (s) {
        pr_alert("%s: ndp120_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
    }
    s = syn_ndp_translate_error(s);

    mutex_unlock(&dev->config_mutex);

    return s;
}

static int
ndp120_ioctl_config_other(struct syn_ndp_s *dev,
                           struct syntiant_ndp120_config_other_s *ndp120_config_other)
{
    int s = 0;
    enum syntiant_ndp120_config_other_e action;
    union syntiant_ndp120_config_other_u *arg;
    ndp120_dsp_audio_sync_config_t cfg;
    syntiant_ndp120_config_audio_sync_t *config;

    if (dev->chip_type != SYNTIANT_NDP_CORE_2)
        return -EINVAL;

    action = ndp120_config_other->config_other_action;
    arg = &ndp120_config_other->config_other_u;
    s = mutex_lock_interruptible(&dev->config_mutex);
    if (s) {
        return s;
    }

    switch(action) {

          case NDP120_CONFIG_OTHER_PDM:
          s = syntiant_ndp120_config_pdm(dev->ndp, &arg->ndp120_config_pdm);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_pdm error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CLK_SRC:
          s = syntiant_ndp120_config_clk_src(dev->ndp, &arg->ndp120_config_clk_src);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_clk_src error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CLK_PLL:
          s = syntiant_ndp120_config_clk_pll(dev->ndp, &arg->ndp120_config_clk_pll);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_clk_pll error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CLK_FLL:
          s = syntiant_ndp120_config_clk_fll(dev->ndp, &arg->ndp120_config_clk_fll);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_clk_fll error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_DECIMATION:
          s = syntiant_ndp120_config_decimation(dev->ndp, &arg->ndp120_config_decimation);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_decimation error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_GAIN:
          s = syntiant_ndp120_config_gain(dev->ndp, &arg->ndp120_config_gain);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_gain error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_DSP_PING:
          s = syntiant_ndp120_do_mailbox_req(dev->ndp, NDP120_DSP_MB_H2D_PING, NULL);
          if (s) {
              pr_alert("%s: DSP Ping error %s\n", __func__,
                syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CONFIG_I2S:
          s = syntiant_ndp120_config_i2s(dev->ndp, &arg->ndp120_config_i2s);
          if (s) {
              pr_alert("%s: syntiant_ndp120_config_i2s error: %s\n", __func__,
                   syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CONFIG_AUDIO_SYNC:
          config = &arg->ndp120_config_audio_sync;
          cfg.enable = config->enable;
          cfg.ref_chan = config->ref_chan;
          cfg.adj_chan = config->adj_chan;
          cfg.sample_count_offset = config->sample_count_offset;

          s = syntiant_ndp120_write_audio_sync_config(dev->ndp, &cfg);
          if (s) {
             pr_alert("%s: syntiant_ndp120_config_audio_sync error: %s\n",
                  __func__, syntiant_ndp_error_name(s));
          }
          break;

          case NDP120_CONFIG_OTHER_CONFIG_NOTIFY:
          s = syntiant_ndp120_config_notify_on_sample_ready(dev->ndp,
                arg->ndp120_config_notify.notify_on_sample_ready);
          if (s) {
             pr_alert("%s: syntiant_ndp120_config_notify_on_sample_ready error: %s\n",
                  __func__, syntiant_ndp_error_name(s));
          }
          break;

          default:
          s = -EINVAL;
          break;
    }

    s = syn_ndp_translate_error(s);

    mutex_unlock(&dev->config_mutex);
    return s;
}


static long
syn_ndp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syn_ndp_s *syn_ndp_device;
    int arg_size = 0;
    char *arg_name = "*unknown*";
    union ioctl_arg_u *argu;
    union ioctl_arg_u *argp;

    argu = syn_ndp_malloc(sizeof(*argu));
    if (!argu) {
        pr_err("%s: Can't allocate memory for ioctl\n", __func__);
        s = -ENOMEM;
        goto out;
    }
    memset(argu, 0, sizeof(*argu));
    argp = argu;

    syn_ndp_device = file->private_data;

    switch (cmd) {
    case INIT:
        arg_name = "init";
        argp = NULL;
        break;
    case NDP10X_CONFIG:
        arg_name = "ndp10x_config";
        arg_size = sizeof(struct syntiant_ndp10x_config_s);
        break;
    case NDP_CONFIG:
        arg_name = "ndp_config";
        arg_size = sizeof(struct ndp_config_s);
        break;
    case LOAD:
        arg_name = "load";
        arg_size = sizeof(struct ndp_load_s);
        break;
    case TRANSFER:
        arg_name = "transfer";
        arg_size = sizeof(struct ndp_transfer_s);
        break;
    case WATCH:
        arg_name = "watch";
        arg_size = sizeof(struct ndp_watch_s);
        break;
    case DRIVER_CONFIG:
        arg_name = "driver_config";
        arg_size = sizeof(struct ndp_driver_config_s);
        break;
    case PCM_EXTRACT:
        arg_name = "pcm_extract";
        arg_size = sizeof(struct ndp_pcm_extract_s);
        break;
    case PCM_SEND:
        arg_name = "pcm_send";
        arg_size = sizeof(struct ndp_pcm_send_s);
        break;
    case STATS:
        arg_name = "statistics";
        arg_size = sizeof(struct syn_ndp_statistics_s);
        break;
    case SERIAL:
        arg_name = "serial";
        arg_size = sizeof(struct ndp10x_serial_s);
        break;
    case GPIO:
        arg_name = "gpio";
        arg_size = sizeof(struct ndp10x_serial_s);
        break;
    case NDP120_INIT:
        arg_name = "ndp120_config";
        arg_size = sizeof(struct ndp120_init_s);
        break;
    case NDP120_CONFIG:
        arg_name = "ndp120_config";
        arg_size = sizeof(syntiant_ndp120_config_misc_t);
        break;
    case NDP120_CONFIG_OTHER:
        arg_name = "ndp120_config_other";
        arg_size = sizeof(syntiant_ndp120_config_other_t);
        break;
    case NDP120_FLOW_SETUP:
        arg_name = "ndp120_flow_setup";
        arg_size = sizeof(syntiant_ndp120_flow_setup_t);
        break;
    }

    switch (cmd) {
    case INIT:
    case NDP120_INIT:
    case TRANSFER:
    case DRIVER_CONFIG:
    case STATS:
    case SERIAL:
    case GPIO:
        break;
    case PCM_SEND:
    case PCM_EXTRACT:
        if (!READ_ONCE(syn_ndp_device->package_loaded)) {
            printk(KERN_INFO "%s: !syn_ndp_device->package_loaded\n", __func__);
            s = -EINVAL;
            goto out;
        }
        /* fall through */
    default:
        if (!syn_ndp_device->ndp) {
            s = -EINVAL;
            goto out;
        }
    }

    if (argp) {
        if (copy_from_user(argp, (void __user *)arg, arg_size)) {
            pr_debug("%s: %s arg protection error\n", __func__, arg_name);
            s = -EACCES;
            goto out;
        }
    }

    switch (cmd) {
    case INIT:
        s = syn_ndp_ioctl_init(syn_ndp_device, arg);
        break;

    case NDP10X_CONFIG:
        s = ndp10x_ioctl_ndp10x_config(syn_ndp_device, &argp->ndp10x_config);
        break;

    case NDP_CONFIG:
        s = syn_ndp_ioctl_ndp_config(syn_ndp_device, &argp->ndp_config);
        break;

    case LOAD:
        s = syn_ndp_ioctl_load(syn_ndp_device, &argp->load);
        break;

    case TRANSFER:
        s = ndp_ioctl_transfer(syn_ndp_device, &argp->transfer);
        break;

    case WATCH:
        if (argp->watch.sensor) {
          s = ndp10x_watch_sensor(syn_ndp_device, &argp->watch);
        } else {
          s = ndp_ioctl_watch(syn_ndp_device, &argp->watch);
        }
        break;

    case DRIVER_CONFIG:
        s = ndp_ioctl_driver_config(syn_ndp_device, &argp->driver_config);
        break;

    case PCM_EXTRACT:
        s = ndp_ioctl_pcm_extract(syn_ndp_device, &argp->pcm_extract);
        break;

    case PCM_SEND:
        s = ndp10x_ioctl_pcm_send(syn_ndp_device, &argp->pcm_send);
        break;

    case STATS:
        s = ndp10x_ioctl_statistics(syn_ndp_device, &argp->statistics);
        break;

    case SERIAL:
        s = ndp10x_ioctl_serial(syn_ndp_device, &argp->serial);
        break;

    case GPIO:
        s = ndp10x_ioctl_gpio(syn_ndp_device, &argp->gpio);
        break;

    case NDP120_INIT:
        s = ndp120_ioctl_init(syn_ndp_device, &argp->init);
        break;

    case NDP120_CONFIG:
        s = ndp120_ioctl_config(syn_ndp_device, &argp->ndp120_config);
        break;

    case NDP120_CONFIG_OTHER:
        s = ndp120_ioctl_config_other(syn_ndp_device, &argp->ndp120_config_other);
        break;

    case NDP120_FLOW_SETUP:
        s = ndp120_ioctl_flow_setup(syn_ndp_device, &argp->ndp120_flow_setup);
        break;

    default:
        s = -EINVAL;
        break;
    }

    if (!s && argp) {
        if (copy_to_user((void __user *)arg, argp, arg_size)) {
            pr_debug("%s: %s return protection error\n", __func__, arg_name);
            s = -EACCES;
        }
    }
out:
    if (argu) {
        syn_ndp_free(argu);
    }
    return s;
}

static int
syn_ndp_open(struct inode *inode, struct file *file)
{
    struct syn_ndp_s *syn_ndp_dev;
    int minor = MINOR(inode->i_rdev);
    if (minor >= MAX_DEV)
        return -ENODEV;

    syn_ndp_dev = &ndps[minor];
    file->private_data = syn_ndp_dev;

    if (syn_ndp_dev->spi_dev.spi->irq > 0)
        enable_irq(syn_ndp_dev->spi_dev.spi->irq);
    syn_ndp_dev->opens++;
    pr_debug("%s: opened (%d)\n", __func__, syn_ndp_dev->opens);
    return 0;
}

static int
syn_ndp_release(struct inode *inode, struct file *file)
{
    struct syn_ndp_s *syn_ndp;

    syn_ndp = file->private_data;

    syn_ndp->opens--;
    if (syn_ndp->spi_dev.spi->irq > 0)
        disable_irq_nosync(syn_ndp->spi_dev.spi->irq);
    pr_debug("%s: closed (%d)\n", __func__, syn_ndp->opens);
    return 0;
}

static int
syn_ndp_spi_setup(struct syn_ndp_s *ndp10x, struct spi_device *spi)
{
    struct syn_ndp_spi_dev_s *d;
    int s = 0;

    pr_debug("%s: spi setting up\n", __func__);

    spi->max_speed_hz = SPI_MAX_SPEED;
    spi->chip_select = CHIP_SELECT;
    spi->bits_per_word = 8;
    spi->mode = SPI_MODE;
    s = spi_setup(spi);
    if (s) {
        pr_err("%s: failed to setup spi\n", __func__);
        return s;
    }

    d = &ndp10x->spi_dev;
    d->ndp = ndp10x;
    d->spi = spi;
    d->spi_split = SPI_SPLIT;
    d->spi_speed = MAX_SPEED;
    d->spi_read_pad_bytes = d->spi_speed >= 1000000 ? SPI_READ_DELAY : 0;
    d->spi_send_speed = spi->max_speed_hz < 1000000
        ? spi->max_speed_hz
        : 1000000;
    mutex_init(&d->lock);
    spi_set_drvdata(spi, &ndp10x->spi_dev);

    return 0;
}

/* begin device integration code */
#if 0
static int
ndp_get_reset_config (struct syn_ndp_s *ndp)
{

    int s = 0;
    struct device_node * np = NULL;
    uint32_t gpio;

    ndp->reset_gpio = -1;
    ndp->reset_gpio_assert_direction = GPIOF_INIT_HIGH;
    ndp->ndp_reset_delay = NDP120_DEFAULT_RESET_DELAY;

    np = of_find_compatible_node(NULL, NULL, SPI_DEVICE_NAME);
    if (!np)
        return 0;

    s = of_property_read_u32(np, "reset-gpio-reset-value", &gpio);
    if (!s) {
        ndp->reset_gpio_assert_direction = gpio ? GPIOF_INIT_HIGH : GPIOF_INIT_LOW;
    }

    s = of_property_read_u32(np, "reset-gpio-num", &gpio);
    if (!s) {
        s = gpio_request(gpio, "ndp-reset-gpio");
        if (s < 0) {
            pr_err("%s: request reset gpio (%d) fail\n", __func__, gpio);
            return s;
        }
        ndp->reset_gpio = gpio;
    } else {
        return 0;
    }

    s = of_property_read_u32(np, "reset-delay-msec", &gpio);
    if (!s) {
        ndp->ndp_reset_delay = gpio;
    }

    pr_debug("%s: gpio %d is %sasserted %dms for reset\n", __func__,
        ndp->reset_gpio,
        ndp->reset_gpio_assert_direction == GPIOF_INIT_LOW ? "de" : "",
        ndp->ndp_reset_delay);

    return 0;
}
#endif

static void
ndp_config_ext_pwr(struct spi_device *spi, struct syn_ndp_s *ndp)
{
    /* get [optional] pwr info from device tree and configure */
    int s;
    struct device *dev = &spi->dev;

    ndp->dsp_vdd_en = of_get_named_gpio(dev->of_node, "dsp_vdd_en", 0);
    if (gpio_is_valid(ndp->dsp_vdd_en)) {
        /* enable NDP power source */
        s = gpio_request(ndp->dsp_vdd_en, "dsp_vdd_en");
        if (s < 0) {
            pr_err("%s: request ndp poweron gpio fail\n", __func__);
            ndp->dsp_vdd_en = -1;
            return;
        }
        
        gpio_direction_output(ndp->dsp_vdd_en, 1);
        pr_info("%s: gpio (%d) for output 1 \n", __func__, ndp->dsp_vdd_en);
    }
    else {
        pr_info("dsp_vdd_en fro ndp hasn't been assigned\n");
    }

    ndp->reset_gpio = of_get_named_gpio(dev->of_node, "dsp_rst", 0);
    if (gpio_is_valid(ndp->reset_gpio)) {
        /* enable NDP power source */
        s = gpio_request(ndp->reset_gpio, "dsp_rst");
        if (s < 0) {
            pr_err("%s: request ndp reset gpio fail\n", __func__);
            ndp->reset_gpio = -1;
            return;
        }
    }
    else {
        pr_info("reset_gpio fro ndp hasn't been assigned\n");
    }
}

static void
ndp_config_ext_clk(void)
{ 
    mt6358_upmu_set_rg_rtc32k_1v8_1_pdn(0x1c);
}
/* end device integration code */
#ifdef CONFIG_DEBUG_FS
static const char*
syntiant_ndp_get_device_name(struct syn_ndp_s *dev)
{
    if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        return "ndp10x";
    } else if (dev->chip_type == SYNTIANT_NDP_CORE_2) {
        return "ndp120";
    } else {
        return "Unknown";
    }
}
#endif

static int
syn_ndp_get_device_type(struct syn_ndp_s *dev)
{
    int s = 0;
    int mcu = 0;
    uint32_t addr = 0;
    int count = 1;
    uint8_t in = 0;

    s = syn_ndp_transfer(dev, mcu, addr, NULL, &in, count);
    if (s) {
	    pr_err("%s: Error in spi transfer:%d\n", __func__, s);
	    goto out;
    }
    dev->chip_type = syntiant_ndp_get_device_family(in);
out:
    return s;
}

static int
syn_ndp_spi_probe(struct spi_device *spi)
{
    int s, minor;
/* begin device integration code */
    unsigned long flags = IRQF_ONESHOT | IRQF_TRIGGER_HIGH;
/* end device integration code */
    struct syn_ndp_spi_dev_s *d;
    struct syn_ndp_s *ndp10x;

    pr_debug("%s: spi probing\n", __func__);

    minor = 0;
    ndp10x = &ndps[minor];
    d = &ndp10x->spi_dev;
    s = syn_ndp_spi_setup(ndp10x, spi);
    if (s) return s;

    /* enable power for the NDP10x if enabled in device tree */
/* begin device integration code */
    ndp_config_ext_pwr(spi, ndp10x);

    ndp_config_ext_clk();
/* end device integration code */

#ifdef CONFIG_PM_SLEEP
    device_init_wakeup(&spi->dev, 1);
    if (of_find_property(spi->dev.of_node, "pm-irq-always-enabled", NULL)) {
        ndp10x->irq_always_enabled = 1;
    }
#endif

    s = request_threaded_irq(spi->irq, NULL,
                             ndp_isr, flags, DEVICE_NAME, d);
    if (s) {
        pr_err("%s: failed to acquire irq %d\n", __func__, spi->irq);
    }
    disable_irq_nosync(spi->irq);

    ndp10x->procfs_dir_ent = proc_mkdir(DEVICE_NAME, NULL);
    if (ndp10x->procfs_dir_ent == NULL) {
        s = -ENOMEM;
        goto out;
    }

    ndp10x->procfs_info_file = proc_create_data("info", 0444,
                                                ndp10x->procfs_dir_ent,
                                                &ndp10x_procfs_info_fops,
                                                ndp10x);
    if (ndp10x->procfs_info_file == NULL) {
        s = -ENOMEM;
        goto no_info;
    }

    ndp10x->procfs_mic_ctl_file = proc_create_data("mic_ctl", 0666,
                                                   ndp10x->procfs_dir_ent,
                                                   &procfs_mic_ctl_fops,
                                                   ndp10x);
    if (ndp10x->procfs_info_file == NULL) {
        s = -ENOMEM;
        goto no_mic_ctl;
    }

    return s;

 no_mic_ctl:
    remove_proc_entry("info", ndp10x->procfs_dir_ent);
 no_info:
    remove_proc_entry(DEVICE_NAME, NULL);
 out:
    return s;
}

static int
syn_ndp_spi_init(struct syn_ndp_spi_dev_s *spi_dev)
{
    int s = 0;

    s = alloc_chrdev_region(&spi_dev->spi_devt, 0, 1, SPI_DEVICE_NAME);
    if (s) {
        pr_err("Error allocating chrdev region: %d\n", s);
        goto error;
    }

    spi_dev->spi_cls = class_create(THIS_MODULE, SPI_DEVICE_NAME);
    if (IS_ERR(spi_dev->spi_cls)) {
        s = PTR_ERR(spi_dev->spi_cls);
        pr_err("Error creating syn_ndp_spi_dev_s.spi_cls: %d\n", s);
        goto error;
    }

    cdev_init(&spi_dev->cdev, &syn_ndp_spi_fops);
    spi_dev->cdev.owner = THIS_MODULE;
    s = cdev_add(&spi_dev->cdev, spi_dev->spi_devt, 1);
    if (s != 0) {
        pr_err("Error calling cdev_add: %d\n", s);
        spi_dev->cdev.owner = NULL;
        goto error;
    }

    spi_dev->spi_dev = device_create(spi_dev->spi_cls, NULL,
                                     spi_dev->cdev.dev, spi_dev,
                                     SPI_DEVICE_NAME);
    if (IS_ERR(spi_dev->spi_dev)) {
        s = PTR_ERR(spi_dev->spi_dev);
        pr_err("device_create failed: %d\n", s);
        goto error;
    }

    s = spi_register_driver(&syn_ndp_spi_driver);
    if (s != 0) {
        pr_err("Error registering spi driver: %d\n", s);
        goto error;
    }
    spi_dev->registered = 1;

    pr_debug("%s: spi init %d\n", __func__, s);

 error:
    return s;
};

#ifdef CONFIG_DEBUG_FS

static int
ndp10x_debugfs_noise_read(struct seq_file *m, void *unused)
{
    int s;
    uint32_t addr;
    struct ndp10x_agc_state_s noise;
    struct syn_ndp_s *ndp10x = m->private;
    struct ndp10x_fw_state_s *fw_state;
    struct ndp10x_host_interface_s *hint;
    int len = (int) sizeof(*fw_state);

    fw_state = (struct ndp10x_fw_state_s*) syn_ndp_malloc(len);
    if (!fw_state) {
        return -ENOMEM;
    }
    memset(fw_state, 0, len);
    s = syntiant_ndp10x_debug_extract(ndp10x->ndp,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_FW_STATE, fw_state, &len);
    if (s) {
        pr_err("%s: Error in reading FW state data: %d\n", __func__, s);
        s = syn_ndp_translate_error(s);
        goto out;
    }

    /* read noise data */
    addr = syntiant_ndp10x_get_fw_agc_addr(ndp10x->ndp);
    memset(&noise, 0, sizeof(noise));
    s = syntiant_ndp10x_read_block(ndp10x->ndp, 1, addr, (void *) &noise,
               sizeof(noise));
    if (s) {
       pr_err("%s: Error in reading agc state\n", __func__);
       s = -ENOSYS;
       goto out;
    }
    hint = &fw_state->host_intf;
    seq_printf(m, "noise threshold: 0x%08x\n"
               "noise window: %d\n"
               "thresh counter_hi: 0x%08x\n"
               "thresh counter_lo: 0x%08x\n"
               "noise level at xing: 0x%08x\n"
               "current noise level: 0x%08x\n"
               "current speech Level: 0x%08x\n",
               hint->noise_threshold, hint->noise_threshold_win,
               hint->noise_threshold_cntr_hi,
               hint->noise_threshold_cntr_lo,
               noise.noise_level_data, noise.noise_level,
               noise.speech_level);
out:
    if (fw_state) {
        syn_ndp_free(fw_state);
        fw_state = NULL;
    }
    return s;
}

static int ndp10x_debugfs_noise_open(struct inode *inode, struct file *file)
{
    return single_open(file, ndp10x_debugfs_noise_read, inode->i_private);
}

static const struct file_operations ndp10x_dbg_io_ops = {
    .open = ndp10x_debugfs_noise_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int
syn_ndp_debugfs_noise_init(struct syn_ndp_s *ndp)
{
    struct dentry *dent;
    dent = debugfs_create_file("noise_data", S_IFREG | S_IRUGO,
                               ndp->dbgfs, ndp, &ndp10x_dbg_io_ops);
    if (!dent) {
        pr_err("%s: can't create file, noise_data\n", __func__);
       goto error;
    }
    return 0;
error:
    return -ENODEV;
}

static int
ndp10x_read_sensor_interrupt_count(struct syn_ndp_s *ndp10x, struct seq_file *m)
{
    int s = 0, i;
    struct ndp10x_fw_state_s *fw_state;
    int len = (int) sizeof(*fw_state);

    fw_state = (struct ndp10x_fw_state_s*) syn_ndp_malloc(sizeof(*fw_state));
    if (!fw_state) {
        return -ENOMEM;
    }
    memset(fw_state, 0, len);
    s = syntiant_ndp10x_debug_extract(ndp10x->ndp,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_FW_STATE, fw_state, &len);
    if (s) {
       pr_err("%s: Error in reading FW state data\n", __func__);
       goto out;
    }
    seq_printf(m, "DA217 Interrupts\n");
    for (i = 0; i < NDP10X_FW_DA217_EVENT_TYPES; i++) {
      seq_printf(m, "int[%d]: 0x%x\n", i, fw_state->da217_int_count[i]);
    }
    seq_printf(m, "Sensor (LTR559 and other edge-triggered) Interrupts\n");
    for (i = 0; i < NDP10X_FW_SENSOR_MAX; i++) {
      seq_printf(m, "int[%d]: 0x%x\n", i, fw_state->sensor_int_count[i]);
    }
out:
    if (fw_state) {
        syn_ndp_free(fw_state);
    }
    return s;
}

static int
ndp10x_debugfs_sensor_read_raw(struct syn_ndp_s *ndp10x, uint8_t sensor_addr,
    uint8_t *reg, uint8_t *data, size_t len)
{
    int s = 0;
    uint8_t cnt;
    struct ndp10x_serial_s serial;

    memset(&serial, 0, sizeof(serial));
    serial.ifc_type = SYNTIANT_NDP10X_SERIAL_INTERFACE_I2C;
    serial.ifc_addr = sensor_addr;
    serial.continue_ = 0;
    for (cnt = 0; cnt < len; cnt++) {
        serial.inlen = sizeof(data[cnt]);
        serial.outlen = sizeof(reg[cnt]);
        s = syn_ndp_serial_transfer(ndp10x, &serial, &reg[cnt], &data[cnt]);
        if (s) {
           s = syn_ndp_translate_error(s);
           goto out;
        }
    }
out:
    return s;
}

static int
ndp10x_debugfs_sensor_da217_reg_read(struct syn_ndp_s *ndp10x, struct seq_file *m)
{
    int s = 0;
    uint8_t i, len;
    uint8_t data[35];
    uint8_t reg [35];
    char *buf;
    char *buf_start;

    buf = syn_ndp_malloc(sizeof(char) * 1024);
    if (!buf) {
        return -ENOMEM;
    }
    buf_start = buf;

    reg[0] = 0x01;
    for (i = 0; i < 7; i++) {
      reg[1+i] = 0x08 + i;
    }
    for (i = 0; i < 4; i++) {
      reg[8+i] = 0x0f + i;
    }
    for (i = 0; i < 4; i++) {
      reg[12+i] = 0x14 + i;
    }
    for (i = 0; i < 3; i++) {
      reg[16+i] = 0x19 + i;
    }
    for (i = 0; i < 9; i++) {
      reg[19+i] = 0x20 + i;
    }
    for (i = 0; i < 5; i++) {
      reg[28+i] = 0x2a + i;
    }
    for (i = 0; i < 2; i++) {
      reg[33+i] = 0x33 + i;
    }

    s = ndp10x_debugfs_sensor_read_raw(ndp10x, SYNTIANT_DA217_I2C_ADDRESS,
        reg, data, ARRAY_SIZE(reg));
    if (s) {
       s = syn_ndp_translate_error(s);
       goto out;
    }

    len = 0;
    for (i = 0; i < ARRAY_SIZE(data); i++) {
        len = (uint8_t) snprintf(buf_start, 16, "0x%x: 0x%x\n", reg[i], data[i]);
        buf_start += len;
    }
    /* read interrupt counts */
    seq_printf(m, "DA217 registers\n\nreg: value\n%s\n", buf);

    if (s) {
        goto out;
    }
out:
    if(buf) {
        syn_ndp_free(buf);
    }
    return s;
}

static int
ndp10x_debugfs_sensor_ltr559_reg_read(struct syn_ndp_s *ndp10x, struct seq_file *m)
{
    int s = 0;
    uint8_t i, len;
    uint8_t data[22];
    uint8_t reg [22];
    char buf[512];
    char *buf_start = buf;

    for (i = 0; i < 8; i++) {
    reg[i] = 0x80 + i;
    }
    reg[8] = 0x8c;
    for (i = 0; i < 12; i++) {
    reg[9+i] = 0x8f + i;
    }
    reg[21] = 0x9e;

    s = ndp10x_debugfs_sensor_read_raw(ndp10x, SYNTIANT_LTR559_I2C_ADDRESS,
        reg, data, ARRAY_SIZE(reg));
    if (s) {
       s = syn_ndp_translate_error(s);
       goto out;
    }

    len = 0;
    for (i = 0; i < ARRAY_SIZE(data); i++) {
        len = (uint8_t) snprintf(buf_start, 16, "0x%x: 0x%x\n", reg[i], data[i]);
        buf_start += len;
    }
    /* read interrupt counts */
    seq_printf(m, "LTR559 registers\n\nreg: value\n%s\n", buf);

    if (s) {
        goto out;
    }
out:
    return s;
}

static int
ndp10x_debugfs_sensor_read(struct seq_file *m, void *unused)
{
    int s;
    struct syn_ndp_s *ndp10x = m->private;

    s = ndp10x_debugfs_sensor_ltr559_reg_read(ndp10x, m);
    s = ndp10x_debugfs_sensor_da217_reg_read(ndp10x, m);
    s = ndp10x_read_sensor_interrupt_count(ndp10x, m);
    return s;
}

static int ndp10x_debugfs_sensor_open(struct inode *inode, struct file *file)
{
    return single_open(file, ndp10x_debugfs_sensor_read, inode->i_private);
}

static const struct file_operations ndp10x_dbg_sensor_ops = {
    .open = ndp10x_debugfs_sensor_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int
syn_ndp_debugfs_sensor_init(struct syn_ndp_s *ndp)
{
    struct dentry *dent;
    dent = debugfs_create_file("sensor_regs", S_IFREG | S_IRUGO,
                               ndp->dbgfs, ndp, &ndp10x_dbg_sensor_ops);
    if (!dent) {
        printk("Can't create file, sensor_regs\n");
       goto error;
    }
    return 0;
error:
    return -ENODEV;
}

static int
ndp120_read_fw_state(struct syn_ndp_s *ndp120, struct seq_file *m)
{
    int s = 0;

    struct syntiant_ndp_config_s config;

    char fwver[CONFIG_STRING_LEN] = "";
    char paramver[CONFIG_STRING_LEN] =  "";
    char pkgver[CONFIG_STRING_LEN] = "";
    char label_data[CONFIG_STRING_LEN] = "";
    const char *dev_name = syntiant_ndp_get_device_name(ndp120);

    seq_printf(m, "Device: %s\n", dev_name);

    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    config.firmware_version = fwver;
    config.firmware_version_len = STRING_LEN;
    config.parameters_version = paramver;
    config.parameters_version_len = STRING_LEN;
    config.pkg_version = pkgver;
    config.pkg_version_len = STRING_LEN;
    config.labels = label_data;
    config.labels_len = STRING_LEN;

    s = syntiant_ndp_get_config(ndp120->ndp, &config);
    if (s) {
        pr_info("Error in get config:%d\n", s);
        seq_printf(m, "%s device not initialized\n", dev_name);
        s = 0;
        goto out;
    }

    s = dump_mcu_debug_state(ndp120, m);
    s = dump_dsp_debug_state(ndp120, m);
    s = dump_registers(ndp120, m);

out:
    return s;
}

static int
ndp10x_read_fw_state(struct syn_ndp_s *ndp10x, struct seq_file *m)
{
    int s = 0, i, j;
    uint32_t *data;
    struct ndp10x_fw_state_s *fw_state;
    struct ndp10x_host_interface_s *hint;
    int len = (int) sizeof(*fw_state);

    fw_state = syn_ndp_malloc(len);
    if (!fw_state) {
      pr_err("%s: Can't alloc memory for FW state\n", __func__);
      goto out;
    }
    memset(fw_state, 0, len);
    hint = &fw_state->host_intf;
    s = syntiant_ndp10x_debug_extract(ndp10x->ndp,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_FW_STATE, fw_state, &len);
    if (s) {
       pr_err("%s: Error in reading FW state data\n", __func__);
       goto out;
    }
    seq_printf(m, "FW_State:\n\n");
    seq_printf(m, "tankptr:%d\n", hint->tankptr);
    seq_printf(m, "match_ring_size:%d\n", hint->match_ring_size);
    seq_printf(m, "match_producer:%d\n", hint->match_producer);
    for (i = 0; i < ARRAY_SIZE(hint->match_ring); i++) {
       seq_printf(m, "ring[%d]: summary:0x%x tankptr:%d\n",
        i, hint->match_ring[i].summary, hint->match_ring[i].tankptr);
    }
    seq_printf(m, "enable:%d\n", hint->enable);
    seq_printf(m, "prev_enable:%d\n", fw_state->prev_enable);
    seq_printf(m, "result_fifo_full:%d\n", fw_state->result_fifo_full);
    seq_printf(m, "mb_int_count:%d\n", fw_state->mb_int_count);
    seq_printf(m, "freq_int_count:%d\n", fw_state->freq_int_count);
    seq_printf(m, "dnn_int_count:%d\n", fw_state->dnn_int_count);
    seq_printf(m, "unknown_int_count:%d\n", fw_state->unknown_int_count);
    seq_printf(m, "serial_not_configured:%d\n", fw_state->serial_not_configured);
    seq_printf(m, "version:%s\n", fw_state->version);
    seq_printf(m, "serial.control:0x%x\n", hint->serial.control);
    seq_printf(m, "serial.data:\n");
    for (i = 0; i < ARRAY_SIZE(hint->serial.data); i++) {
        seq_printf(m, "0x%x ", hint->serial.data[i]);
    }
    seq_printf(m, "serial_active:%d\n", fw_state->serial_active);
    seq_printf(m, "GPIO_role\n");
    for (i = 0; i < ARRAY_SIZE(hint->gpio_role); i++) {
       seq_printf(m, "0x%x ", hint->gpio_role[i]);
    }
    seq_printf(m, "\nGPIO_role_saved\n");
    for (i = 0; i < ARRAY_SIZE(fw_state->gpio_role_saved); i++) {
       seq_printf(m, "0x%x ", fw_state->gpio_role_saved[i]);
    }
    seq_printf(m, "\nidata:0x%x\n", fw_state->idata);
    seq_printf(m, "iclk:0x%x\n", fw_state->iclk);
    seq_printf(m, "mmiso:0x%x\n", fw_state->mmiso);
    seq_printf(m, "mmosi:0x%x\n", fw_state->mmosi);
    seq_printf(m, "msclk:0x%x\n", fw_state->msclk);
    seq_printf(m, "i2sclk:0x%x\n", fw_state->i2sclk);
    seq_printf(m, "i2sclk_init:0x%x\n", fw_state->i2sclk_init);
    seq_printf(m, "manual_dnn_running:0x%x\n", fw_state->manual_dnn_running);
    for (i = 0; i < ARRAY_SIZE(hint->sensor); i++) {
        seq_printf(m, "sensor[%d]: control:0x%x enable:0x%x\nParameters\n", i,
        hint->sensor[i].control, hint->sensor[i].enable);
    for (j = 0; j < ARRAY_SIZE(hint->sensor[i].parameter); j++) {
        seq_printf(m, " 0x%x ", hint->sensor[i].parameter[j]);
    }
    seq_printf(m, "\n");
    }
    seq_printf(m, "sensor_state\n");
    for (i = 0; i < ARRAY_SIZE(fw_state->sensor_state); i++) {
        seq_printf(m, "sensor[%d]: ", i);
    data = (uint32_t *) &fw_state->sensor_state[i].input;
        for (j = 0; j < sizeof(fw_state->sensor_state[i])/sizeof(*data); j++) {
        seq_printf(m, " 0x%x ", *data);
        data++;
    }
    seq_printf(m, "\n");
    }
    seq_printf(m, "mb_state\n");
    seq_printf(m, "m2h_state:0x%x\n", fw_state->mb_state.m2h_state);
    seq_printf(m, "m2h_req:0x%x\n", fw_state->mb_state.m2h_req);
    seq_printf(m, "m2h_rsp_success:0x%x\n", fw_state->mb_state.m2h_rsp_success);
    seq_printf(m, "m2h_rsp_unknown:0x%x\n", fw_state->mb_state.m2h_rsp_unknown);
    seq_printf(m, "m2h_match_skipped:0x%x\n", hint->m2h_match_skipped);
    seq_printf(m, "h2m_state:0x%x\n", fw_state->mb_state.h2m_state);
    seq_printf(m, "h2m_req_nop:0x%x\n", fw_state->mb_state.h2m_req_nop);
    seq_printf(m, "h2m_req_nop_serial:0x%x\n", fw_state->mb_state.h2m_req_nop_serial);
    seq_printf(m, "h2m_req_extop:0x%x\n", fw_state->mb_state.h2m_req_extop);
    seq_printf(m, "h2m_req_data:0x%x\n", fw_state->mb_state.h2m_req_data);
    seq_printf(m, "h2m_req_cont:0x%x\n", fw_state->mb_state.h2m_req_cont);
    seq_printf(m, "h2m_unexpected_nop:0x%x\n",
    fw_state->mb_state.h2m_unexpected_nop);
    seq_printf(m, "h2m_unexpected_extop:0x%x\n",
    fw_state->mb_state.h2m_unexpected_extop);
    seq_printf(m, "h2m_unexpected_cont:0x%x\n",
    fw_state->mb_state.h2m_unexpected_cont);
    seq_printf(m, "h2m_unexpected_data:0x%x\n",
    fw_state->mb_state.h2m_unexpected_data);
    seq_printf(m, "h2m_req_unknown:0x%x\n",
    fw_state->mb_state.h2m_req_unknown);
    seq_printf(m, "h2m_extop_unknown:0x%x\n",
    fw_state->mb_state.h2m_extop_unknown);
    seq_printf(m, "h2m_data:0x%x\n", fw_state->mb_state.h2m_data);
    seq_printf(m, "h2m_data_count:0x%x\n", fw_state->mb_state.h2m_data_count);
    seq_printf(m, "previous_mbox:0x%x\n", fw_state->mb_state.previous_mbox);
out:
    if (fw_state) {
      syn_ndp_free(fw_state);
      fw_state = NULL;
    }
    return s;
}

static int
syn_ndp_debugfs_fwstate_read(struct seq_file *m, void *unused)
{
    int s;
    struct syn_ndp_s *dev = m->private;

    s = dev->chip_type == SYNTIANT_NDP_CORE_2 ?
	    ndp120_read_fw_state(dev, m) : ndp10x_read_fw_state(dev, m);
    return s;
}

static int syn_ndp_debugfs_fwstate_open(struct inode *inode, struct file *file)
{
    return single_open(file, syn_ndp_debugfs_fwstate_read, inode->i_private);
}

static const struct file_operations syn_ndp_dbg_fwstate_ops = {
    .open = syn_ndp_debugfs_fwstate_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

int
syn_ndp_debugfs_fwstate_init(struct syn_ndp_s *dev)
{
    struct dentry *dent;
    dent = debugfs_create_file("fw_state", S_IFREG | S_IRUGO,
                               dev->dbgfs, dev, &syn_ndp_dbg_fwstate_ops);
    if (!dent) {
        pr_err("Can't create file, firmware state\n");
       goto error;
    }
    return 0;
error:
    return -ENODEV;
}

static int syn_ndp_debugfs_devmem_read(struct seq_file *m, void *unused)
{
    int s = 0, i;

    uint32_t value = 0;
    void *buffer;
    struct syn_ndp_s *dev = m->private;

    if (!dev->memrange) {
        seq_printf(m, "Set a valid (1 or multiple of 4 bytes) mem range\n");
        return s;
    }
    if (dev->dev_address) {
        if (dev->memrange == 1) {
            s = syntiant_ndp_read(dev->ndp, 0, dev->dev_address, &value);
            if (s) goto error;
            seq_printf(m, "Address: 0x%x Value: 0x%x\n",
                    dev->dev_address, value);
        } else if (dev->memrange == 4) {
            s = syntiant_ndp_read(dev->ndp, 1, dev->dev_address, &value);
            if (s) goto error;
            seq_printf(m, "Address: 0x%x Value:0x%x\n",
                    dev->dev_address, value);
        } else {
            if (dev->memrange%4) {
                dev->memrange = ((dev->memrange + 3)/4)*4;
            }
            buffer = vzalloc(dev->memrange);
            if (!buffer) {
                s = -ENOMEM;
                goto error;
            }
            s = syntiant_ndp_read_block(dev->ndp, 1, dev->dev_address, buffer,
                dev->memrange);
            if (!s) {
                seq_printf(m, "0x0: ");
                for (i = 0; i < dev->memrange; i++) {
                    seq_printf(m, "0x%x ", ((uint8_t*)buffer)[i]);
                    if (i && !(i%16)) seq_printf(m, "\n0x%x:", i);
                }
                seq_printf(m, "\n");
            } else {
                pr_err("Error reading address: 0x%x\n", dev->dev_address);
            }
            vfree(buffer);
        }
    }
    return s;
error:
    pr_err("error reading device memory: 0x%x, error:%d\n",
            dev->dev_address, s);
    return s;
}

static int syn_ndp_debugfs_devmem_open(struct inode *inode, struct file *file)
{
    return single_open(file, syn_ndp_debugfs_devmem_read, inode->i_private);
}

static const struct file_operations syn_ndp_dbg_devmem_ops = {
    .open = syn_ndp_debugfs_devmem_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

int
syn_ndp_debugfs_devmem_init(struct syn_ndp_s *dev)
{
    struct dentry *dent;
    dent = debugfs_create_file("dev_mem", S_IFREG | S_IRUGO,
                               dev->dbgfs, dev, &syn_ndp_dbg_devmem_ops);
    if (!dent) {
        pr_err("Can't create file, ndp device memory\n");
       goto error;
    }
    return 0;
error:
    return -ENODEV;
}

#endif /* CONFIG_DEBUG_FS */

/* begin device integration code */
static void syn_ndp_reset_chip(struct syn_ndp_s *ndp, bool keep_in_reset)
{
    (void) keep_in_reset;
    if (gpio_is_valid(ndp->reset_gpio)) {
        pr_info("%s: ndp gpio (%d) for output\n", __func__, ndp->reset_gpio);
	
        gpio_direction_output(ndp->reset_gpio, 0);
        mdelay(NDP120_DEFAULT_RESET_DELAY);
		
        gpio_direction_output(ndp->reset_gpio, 1); 
        mdelay(NDP120_DEFAULT_RESET_DELAY);
    }
}
/* end device integration code */

#ifdef CONFIG_DEBUG_FS
static int
syn_ndp_setup_debugfs(struct syn_ndp_s *dev)
{
    int s = 0;

    if (!dev->dbgfs) {
      dev->dbgfs = debugfs_create_dir(DEVICE_NAME, NULL);
      if (!dev->dbgfs) {
        pr_err("Can't create debugfs directory, %s\n", DEVICE_NAME);
        s = -ENOMEM;
        goto error;
      }
    }
    if (dev->chip_type == SYNTIANT_NDP_CORE_1) {
        s = syn_ndp_debugfs_noise_init(dev);
        if (s) {
            pr_err("Can't initialize noise node under %s: %d\n", DEVICE_NAME, s);
            goto error;
        }
        s = syn_ndp_debugfs_sensor_init(dev);
        if (s) {
            pr_err("Can't initialize sensor node under %s: %d\n", DEVICE_NAME, s);
            goto error;
        }
    }
    s = syn_ndp_debugfs_fwstate_init(dev);
    if (s) {
       pr_err("Can't initialize fwstate node under %s: %d\n", DEVICE_NAME, s);
       goto error;
    }
    s = syn_ndp_debugfs_devmem_init(dev);
    if (s) {
        pr_err("Can't initialize devmem node under %s: %d\n", DEVICE_NAME, s);
        goto error;
    }
    debugfs_create_u32("mem_range", 0644,  dev->dbgfs, &dev->memrange);
    debugfs_create_u32("dev_address", 0644, dev->dbgfs, &dev->dev_address);

    return s;

error:
    if (dev->dbgfs) {
        debugfs_remove_recursive(dev->dbgfs);
    }
    return s;
}
#else
static int
syn_ndp_setup_debugfs(struct syn_ndp_s *dev) {
    (void)dev;
    return 0;
}
#endif /* CONFIG_DEBUG_FS */

static int __init
syn_ndp_init(void)
{
    struct syn_ndp_s *ndp;
    int s;
    int minor;

    ndp_major = register_chrdev(0, DEVICE_NAME, &fops);
    if (ndp_major < 0) {
        pr_alert("%s: failed to register a major number\n", __func__);
        s = ndp_major;
        goto error;
    }

    pr_debug("%s: major %d\n", __func__, ndp_major);

    ndp_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ndp_class)) {
        pr_alert("%s: failed to register device class\n", __func__);
        s = PTR_ERR(ndp_class);
        goto error;
    }

    memset(ndps, 0, sizeof(ndps));

    /* TODO: multiple devices */
    minor = 0;
    ndp = &ndps[minor];

    s = syn_ndp_spi_init(&ndp->spi_dev);
    if (s) {
        goto error;
    }

    mutex_init(&ndp->ndp_mutex);
    init_waitqueue_head(&ndp->mbwait_waitq);

    mutex_init(&ndp->serial_mutex);

    mutex_init(&ndp->config_mutex);

    mutex_init(&ndp->send_ioctl_mutex);
    spin_lock_init(&ndp->send_ring_lock);
    init_waitqueue_head(&ndp->send_waitq);

    mutex_init(&ndp->extract_ioctl_mutex);
    spin_lock_init(&ndp->extract_ring_lock);
    init_waitqueue_head(&ndp->extract_waitq);

    spin_lock_init(&ndp->result_ring_lock);
    init_waitqueue_head(&ndp->result_waitq);

    spin_lock_init(&ndp->sensor_result_ring_lock);
    init_waitqueue_head(&ndp->sensor_result_waitq);

    mutex_init(&ndp->unbuffered_extract_mutex);

    ndp->extract_scratch_size = 512 * 2;
    ndp->extract_scratch = syn_ndp_malloc(ndp->extract_scratch_size);
    if (!ndp->extract_scratch) {
        s = -ENOMEM;
        goto error;
    }

    s = ring_allocate(&ndp->result_ring, RESULT_RING_SIZE,
                      sizeof(struct ndp_result_s), 1);
    if (s) {
        pr_alert("%s: failed to allocate result ring\n", __func__);
        goto error;
    }

    s = ring_allocate(&ndp->sensor_result_ring, SENSOR_RESULT_RING_SIZE,
                      sizeof(struct ndp_result_s), 1);
    if (s) {
        pr_alert("%s: failed to allocate result ring\n", __func__);
        goto error;
    }

    /* default configuration for driver */
    ndp->extract_buffer_size = 10000;
    ndp->send_buffer_size = 100;
    ndp->sends_max_outstanding = 2;

    ndp->minor = minor;

    ndp->device = device_create(ndp_class, NULL,
                                   MKDEV(ndp_major, minor),
                                   NULL, DEVICE_NAME);
    if (IS_ERR(ndp->device)) {
        pr_alert("%s: failed to create device\n",__func__);
        s = PTR_ERR(ndp->device);
        goto error;
    }

    /* allocate default rings for frame step of 512 */
    s = syn_ndp_pcm_rings_init(ndp, 512 * sizeof(int16_t));
    if (s) {
        goto error;
    }
/* begin device integration code */
//    if (!ndp_get_reset_config(ndp) && ndp->reset_gpio >= 0)
            syn_ndp_reset_chip(ndp, false);
/* end device integration code */

    s = syn_ndp_get_device_type(ndp);
    if (s) {
	    pr_info("%s: Get device type failed\n", __func__);
	    return s;
    }

    s = syn_ndp_setup_debugfs(ndp);
    if (s) {
        pr_err("%s: failed to initialize debugfs node\n", __func__);
        goto error;
    }


 error:
    if (s) {
        ndp_uninit(ndp);
    } else {
        pr_info("%s: driver loaded, version: %s\n",
                DEVICE_NAME, SYNTIANT_NDP_ILIB_VERSION);
    }

    return s;
}

static void
ndp_uninit(struct syn_ndp_s *dev)
{
    int s;

    s = ndp_quiesce(dev);
    if (s) {
        pr_err("%s: quiesce failed: %d\n", __func__, -s);
    }

    s = syntiant_ndp_uninit(dev->ndp, 1, 0);
    if (s) {
        pr_alert("%s: failed to uninit device %d: %s\n", __func__,
                 dev->minor, syntiant_ndp_error_name(s));
    }

    WRITE_ONCE(dev->armed, 0);
    WRITE_ONCE(dev->armed_watch_active, 0);
    dev->ndp = NULL;
    pr_info("syn_ndp%d uninited\n", dev->minor);

}

static int
syn_ndp_spi_remove(struct spi_device *spi)
{
    struct syn_ndp_spi_dev_s *spi_dev = spi_get_drvdata(spi);

    remove_proc_entry("mic_ctl", spi_dev->ndp->procfs_dir_ent);
    remove_proc_entry("info", spi_dev->ndp->procfs_dir_ent);
    remove_proc_entry(DEVICE_NAME, NULL);

    if (spi->irq > 0) {
        free_irq(spi->irq, spi_dev);
        pr_debug("%s: spi irq %d freed\n", __func__, spi->irq);
    }
    spi_set_drvdata(spi_dev->spi, NULL);
    spi_dev->spi = NULL;

    pr_debug("%s: spi driver removed from bus\n", __func__);
    return 0;
}

static void
syn_ndp_spi_exit(struct syn_ndp_spi_dev_s *spi_dev)
{
    if (spi_dev->registered) {
        spi_unregister_driver(&syn_ndp_spi_driver);
        spi_dev->registered = 0;
        pr_debug("%s: spi driver unregistered\n", __func__);
    }
    if (spi_dev->spi_dev && !IS_ERR(spi_dev->spi_dev)) {
        device_destroy(spi_dev->spi_cls, spi_dev->spi_devt);
        spi_dev->spi_dev = NULL;
        pr_debug("%s: spi device destroyed\n", __func__);
    }
    if (spi_dev->cdev.owner) {
        cdev_del(&spi_dev->cdev);
        spi_dev->cdev.owner = NULL;
        pr_debug("%s: spi cdev deleted\n", __func__);
    }
    if (spi_dev->spi_cls && !IS_ERR(spi_dev->spi_cls)) {
        class_destroy(spi_dev->spi_cls);
        spi_dev->spi_cls = NULL;
        pr_debug("%s: spi class destroyed\n", __func__);
    }
    if (spi_dev->spi_devt) {
        unregister_chrdev_region(spi_dev->spi_devt, 1);
        spi_dev->spi_devt = 0;
        pr_debug("%s: spi chrdev region unregistered\n", __func__);
    }
    mutex_destroy(&spi_dev->lock);
    pr_debug("%s: spi exited\n", __func__);
}

static void
syn_ndp_rings_free(struct syn_ndp_s *ndp10x)
{
    spin_lock(&ndp10x->extract_ring_lock);
    ring_reset(&ndp10x->extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);
    ring_free(&ndp10x->extract_ring);
    mutex_destroy(&ndp10x->extract_ioctl_mutex);

    spin_lock(&ndp10x->send_ring_lock);
    ring_reset(&ndp10x->send_ring);
    spin_unlock(&ndp10x->send_ring_lock);
    ring_free(&ndp10x->send_ring);
    mutex_destroy(&ndp10x->send_ioctl_mutex);

    spin_lock(&ndp10x->result_ring_lock);
    ring_reset(&ndp10x->result_ring);
    spin_unlock(&ndp10x->result_ring_lock);
    ring_free(&ndp10x->result_ring);

    spin_lock(&ndp10x->sensor_result_ring_lock);
    ring_reset(&ndp10x->sensor_result_ring);
    spin_unlock(&ndp10x->sensor_result_ring_lock);
    ring_free(&ndp10x->sensor_result_ring);

    mutex_destroy(&ndp10x->unbuffered_extract_mutex);

    pr_debug("%s: rings freed\n", __func__);
}

static void
syn_ndp_uninit(struct syn_ndp_s *dev)
{
    if (dev->ndp) {
        ndp_uninit(dev);
    }

#ifdef CONFIG_DEBUG_FS
    if (dev->dbgfs) {
       debugfs_remove_recursive(dev->dbgfs);
       dev->dbgfs = NULL;
    }
#endif

    syn_ndp_spi_exit(&dev->spi_dev);

    if (dev->device && !IS_ERR(dev->device)) {
        device_destroy(ndp_class, MKDEV(ndp_major, dev->minor));
        dev->device = NULL;
        pr_debug("%s: spi device destroyed\n", __func__);
    }

    syn_ndp_rings_free(dev);

    mutex_destroy(&dev->serial_mutex);

    mutex_destroy(&dev->config_mutex);

    mutex_destroy(&dev->ndp_mutex);

    if (dev->extract_scratch) {
        syn_ndp_free(dev->extract_scratch);
        dev->extract_scratch = NULL;
        pr_debug("%s: extract scratch freed\n", __func__);
    }

    if (ndp_class && !IS_ERR(ndp_class)) {
        class_unregister(ndp_class);
        ndp_class = NULL;
        pr_debug("%s: spi class unregistered and destroyed\n", __func__);
    }

    if (0 <= ndp_major) {
        unregister_chrdev(ndp_major, DEVICE_NAME);
        ndp_major = -1;
        pr_debug("%s: chrdev unregistered\n", __func__);
    }

    if (dev->dsp_vdd_en >= 0) {
	    gpio_free(dev->dsp_vdd_en);
        dev->dsp_vdd_en = -1;
    }

    if (dev->reset_gpio >= 0) {
	    gpio_free(dev->reset_gpio);
        dev->reset_gpio = -1;
    }
}

static void __exit
syn_ndp_exit(void)
{
    struct syn_ndp_s *ndp;
    int minor;

    minor = 0;
    ndp = &ndps[minor];

    syn_ndp_uninit(ndp);

    pr_info("%s: driver unloaded\n", DEVICE_NAME);
}

/* begin device integration code */
late_initcall(syn_ndp_init);
/*module_init(syn_ndp_init);*/
/* end device integration code */
module_exit(syn_ndp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syntiant Corporation");
MODULE_DESCRIPTION("Syntiant NDP driver");
MODULE_VERSION(SYNTIANT_NDP_ILIB_VERSION);
