
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h> 
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
//#include <linux/rtpm_prio.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "tpd.h"
//#include <cust_eint.h>


//#include <mach/mt_pm_ldo.h>
//#include <mach/mt_typedefs.h>
//#include <mach/mt_boot.h>
//#include <mt-plat/mt_boot_common.h>

#include "mtk_gslX680.h"

#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
#define GSL_MONITOR
#define GSL9XX_CHIP		//??им3?D????ид?IC
#define GSLX680_NAME	"gslX680"
#define GSLX680_ADDR	0x40
#define MAX_FINGERS	  	10
#define MAX_CONTACTS	10
#define DMA_TRANS_LEN	0x20
#define SMBUS_TRANS_LEN	0x01
#define GSL_PAGE_REG		0xf0
#define ADD_I2C_DEVICE_ANDROID_4_0
//#define HIGH_SPEED_I2C
//#define FILTER_POINT
#ifdef FILTER_POINT
#define FILTER_MAX	9
#endif

#define TPD_PROC_DEBUG
#ifdef TPD_PROC_DEBUG
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>  //lzk
#include <linux/uaccess.h>
//typedef unsigned char           u8;
//typedef unsigned short          u16;
//typedef unsigned int            u32;
//typedef unsigned long long      u64;
//typedef signed char             s8;
//typedef short                   s16;
//typedef int                     s32;
//typedef long long               s64;

//static struct proc_dir_entry *gsl_config_proc = NULL;

#define EM_TP		"[tpd-gslx680] "
#ifdef CONFIG_RECORD_KLOG
void printc(const char *fmt, ...);
#define PRINT_TP(fmt, args...)   printc(EM_TP fmt,##args)
#else
#define PRINT_TP(fmt, args...)   printk(EM_TP fmt,##args)
#endif



#define GSL_CONFIG_PROC_FILE "gsl_config"
#define CONFIG_LEN 31
static char gsl_read[CONFIG_LEN];
static u8 gsl_data_proc[8] = {0};
static u8 gsl_proc_flag = 0;
#endif

#ifdef GSL_IDENTY_TP
static int gsl_tp_type = 0;
static int gsl_identify_tp(struct i2c_client *client);
#endif


enum {	
	RST_GPIO = 0,
	IRQ_GPIO = 1,
};

static int tpd_flag = 0;
static int tpd_halt=0;
static char eint_flag = 0;
extern struct tpd_device *tpd;
static struct i2c_client *i2c_client = NULL;
static struct task_struct *thread = NULL;
#ifdef GSL_MONITOR
static struct delayed_work gsl_monitor_work;
static struct workqueue_struct *gsl_monitor_workqueue = NULL;
static u8 int_1st[4] = {0};
static u8 int_2nd[4] = {0};
//static char dac_counter = 0;
static char b0_counter = 0;
static char bc_counter = 0;
static char i2c_lock_flag = 0;
#endif

static u32 id_sign[MAX_CONTACTS+1] = {0};
static u8 id_state_flag[MAX_CONTACTS+1] = {0};
static u8 id_state_old_flag[MAX_CONTACTS+1] = {0};
static u16 x_old[MAX_CONTACTS+1] = {0};
static u16 y_old[MAX_CONTACTS+1] = {0};
static u16 x_new = 0;
static u16 y_new = 0;

static unsigned int *g_config_data_id = NULL;
static const struct fw_data *g_gsl_fw = NULL;
static unsigned int g_fw_size = 0;
static unsigned int g_config_data_id_size = 0;


#define TPD_HAVE_BUTTON
#define TPD_KEY_COUNT	4
#define TPD_KEYS		{KEY_MENU, KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH}
/* {button_center_x, button_center_y, button_width, button_height*/
#define TPD_KEYS_DIM	{{70, 2048, 60, 50},{210, 2048, 60, 50},{340, 2048, 60, 50},{470, 2048, 60, 50}}


static DECLARE_WAIT_QUEUE_HEAD(waiter);
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);

unsigned int touch_irq = 0;

static const struct i2c_device_id gslx68x_tpd_id[] = {{"gslx68x", 0}, {} };
static const struct of_device_id gslx68x_dt_match[] = {
	{.compatible = "mediatek,touch-gslx680"},
	{},
};
MODULE_DEVICE_TABLE(of, gslx68x_dt_match);

//extern void mt_eint_unmask(unsigned int line);
//extern void mt_eint_mask(unsigned int line);
//extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
//extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
//extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
//extern void mt_eint_registration(kal_uint8 eintno,
//		kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
//		kal_bool auto_umask);
//extern void mt_eint_registration(unsigned int eint_num, unsigned int flag,
//              void (EINT_FUNC_PTR) (void), unsigned int is_auto_umask);
//#define GSL_DEBUG
#ifdef GSL_DEBUG 
#define print_info(fmt, args...)   \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_info(fmt, args...)
#endif

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;

#endif
#if defined(CONFIG_T809_TPD_WAKE) 
static u8 tpd_gsl_suspend=0;
#define KEY_NUM_CODE  KEY_POWER
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif




static void startup_chip(struct i2c_client *client)
{
	u8 write_buf = 0x00;

	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &write_buf); 	
#ifdef GSL_NOID_VERSION
	gsl_DataInit(g_config_data_id);
#endif

	msleep(10);		
}

#ifdef GSL9XX_CHIP
static void gsl_io_control(struct i2c_client *client)
{
	u8 buf[4] = {0};
	int i;

	for(i=0;i<5;i++){
		buf[0] = 0;
		buf[1] = 0;
		buf[2] = 0xfe;
		buf[3] = 0x1;
		i2c_smbus_write_i2c_block_data(client, 0xf0, 4, buf);
		buf[0] = 0x5;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0x80;
		i2c_smbus_write_i2c_block_data(client, 0x78, 4, buf);
		msleep(5);
	}
	msleep(50);

}
#endif

static void reset_chip(struct i2c_client *client)
{
	u8 write_buf[4]	= {0};

	write_buf[0] = 0x88;
	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &write_buf[0]); 	
	msleep(20);

	write_buf[0] = 0x04;
	i2c_smbus_write_i2c_block_data(client, 0xe4, 1, &write_buf[0]); 	
	msleep(10);

	write_buf[0] = 0x00;
	write_buf[1] = 0x00;
	write_buf[2] = 0x00;
	write_buf[3] = 0x00;
	i2c_smbus_write_i2c_block_data(client, 0xbc, 4, write_buf); 	
	msleep(10);
	#ifdef GSL9XX_CHIP
	gsl_io_control(client);
	#endif

}

static void clr_reg(struct i2c_client *client)
{
	u8 write_buf[4]	= {0};

	write_buf[0] = 0x88;
	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &write_buf[0]); 	
	msleep(20);

	write_buf[0] = 0x03;
	i2c_smbus_write_i2c_block_data(client, 0x80, 1, &write_buf[0]); 	
	msleep(5);
	
	write_buf[0] = 0x04;
	i2c_smbus_write_i2c_block_data(client, 0xe4, 1, &write_buf[0]); 	
	msleep(5);

	write_buf[0] = 0x00;
	i2c_smbus_write_i2c_block_data(client, 0xe0, 1, &write_buf[0]); 	
	msleep(20);
}

#ifdef HIGH_SPEED_I2C
static u32 gsl_read_interface(struct i2c_client *client, u8 reg, u8 *buf, u32 num)
{
	struct i2c_msg xfer_msg[2];

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = 1;
	xfer_msg[0].flags = client->flags & I2C_M_TEN;
	xfer_msg[0].buf = &reg;
	xfer_msg[0].timing = 400;

	xfer_msg[1].addr = client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags |= I2C_M_RD;
	xfer_msg[1].buf = buf;
	xfer_msg[1].timing = 400;

	if (reg < 0x80) {
		i2c_transfer(client->adapter, xfer_msg, ARRAY_SIZE(xfer_msg));
		msleep(5);
	}

	return i2c_transfer(client->adapter, xfer_msg, ARRAY_SIZE(xfer_msg)) == ARRAY_SIZE(xfer_msg) ? 0 : -EFAULT;
}

static u32 gsl_write_interface(struct i2c_client *client, const u8 reg, u8 *buf, u32 num)
{
	struct i2c_msg xfer_msg[1];

	buf[0] = reg;

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num + 1;
	xfer_msg[0].flags = client->flags & I2C_M_TEN;
	xfer_msg[0].buf = buf;
	xfer_msg[0].timing = 400;

	return i2c_transfer(client->adapter, xfer_msg, 1) == 1 ? 0 : -EFAULT;
}

static __inline__ void fw2buf(u8 *buf, const u32 *fw)
{
	u32 *u32_buf = (int *)buf;
	*u32_buf = *fw;
}

static void gsl_load_fw(struct i2c_client *client)
{
	u8 buf[DMA_TRANS_LEN*4 + 1] = {0};
	u8 send_flag = 1;
	u8 *cur = buf + 1;
	u32 source_line = 0;
	u32 source_len;
	struct fw_data *ptr_fw;

	PRINT_TP("=============gsl_load_fw start==============\n");

	ptr_fw = g_gsl_fw;
	source_len = g_fw_size;
	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
		if (GSL_PAGE_REG == ptr_fw[source_line].offset)
		{
			fw2buf(cur, &ptr_fw[source_line].val);
			gsl_write_interface(client, GSL_PAGE_REG, buf, 4);
			send_flag = 1;
		}
		else 
		{
			if (1 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20))
	    			buf[0] = (u8)ptr_fw[source_line].offset;

			fw2buf(cur, &ptr_fw[source_line].val);
			cur += 4;

			if (0 == send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20)) 
			{
	    			gsl_write_interface(client, buf[0], buf, cur - buf - 1);
	    			cur = buf + 1;
			}

			send_flag++;
		}
	}

	PRINT_TP("=============gsl_load_fw end==============\n");

}
#else
#ifdef GSL_IDENTY_TP
static void gsl_load_fw(struct i2c_client *client,const struct fw_data *GSL_DOWNLOAD_DATA,int data_len)
#else
static void gsl_load_fw(struct i2c_client *client)
#endif
{
	u8 buf[SMBUS_TRANS_LEN*4] = {0};
	u8 reg = 0, send_flag = 1, cur = 0;
	
	
	unsigned int source_line = 0;
	unsigned int source_len = g_fw_size;
	

#ifdef GSL_IDENTY_TP
	{
		g_gsl_fw = GSL_DOWNLOAD_DATA;
		source_len = data_len;
	}
	
#endif
	PRINT_TP("=============gsl_load_fw start==============\n");
	
    
	for (source_line = 0; source_line < source_len; source_line++) 
	{
		if(1 == SMBUS_TRANS_LEN)
		{
			reg = g_gsl_fw[source_line].offset;
			memcpy(&buf[0], &g_gsl_fw[source_line].val, 4);
			i2c_smbus_write_i2c_block_data(client, reg, 4, buf);
         				
		}
		else
		{
			/* init page trans, set the page val */
			if (GSL_PAGE_REG == g_gsl_fw[source_line].offset)
			{
				buf[0] = (u8)(g_gsl_fw[source_line].val & 0x000000ff);
				i2c_smbus_write_i2c_block_data(client, GSL_PAGE_REG, 1, &buf[0]); 	
				send_flag = 1;
			}
			else 
			{
				if (1 == send_flag % (SMBUS_TRANS_LEN < 0x08 ? SMBUS_TRANS_LEN : 0x08))
					reg = g_gsl_fw[source_line].offset;

				memcpy(&buf[cur], &g_gsl_fw[source_line].val, 4);
				cur += 4;

				if (0 == send_flag % (SMBUS_TRANS_LEN < 0x08 ? SMBUS_TRANS_LEN : 0x08)) 
				{
					i2c_smbus_write_i2c_block_data(client, reg, SMBUS_TRANS_LEN*4, buf); 	
					cur = 0;
				}

				send_flag++;

			}
		}
	}

	PRINT_TP("=============gsl_load_fw end==============\n");

}
#endif

static int test_i2c(struct i2c_client *client)
{
	u8 read_buf = 0;
	u8 write_buf = 0x12;
	int ret, rc = 1;
	
	ret = i2c_smbus_read_i2c_block_data( client, 0xf0, 1, &read_buf );
	if  (ret  < 0)  
    		rc --;
	else
		PRINT_TP("I read reg 0xf0 is %x\n", read_buf);

	msleep(2);
	ret = i2c_smbus_write_i2c_block_data( client, 0xf0, 1, &write_buf );
	if(ret  >=  0 )
		PRINT_TP("I write reg 0xf0 0x12\n");
	
	msleep(2);
	ret = i2c_smbus_read_i2c_block_data( client, 0xf0, 1, &read_buf );
	if(ret <  0 )
		rc --;
	else
		PRINT_TP("I read reg 0xf0 is 0x%x\n", read_buf);

	return rc;
}

static void init_chip(struct i2c_client *client)
{
	
	clr_reg(client);
	reset_chip(client);
		
	
#ifdef GSL_IDENTY_TP
if(0==gsl_tp_type)
		gsl_identify_tp(client);

	if(1==gsl_tp_type){
		g_config_data_id = gsl_config_data_id_PB;
		g_gsl_fw = GSLX680_FW_PB;
		g_fw_size = ARRAY_SIZE(GSLX680_FW_PB);
		g_config_data_id_size = ARRAY_SIZE(gsl_config_data_id_PB);
	}
	else if(2==gsl_tp_type)
		{
		g_config_data_id = gsl_config_data_id_WJ;
		g_gsl_fw = GSLX680_FW_WJ;
		g_fw_size = ARRAY_SIZE(GSLX680_FW_WJ);
		g_config_data_id_size = ARRAY_SIZE(gsl_config_data_id_WJ);
		}
	else if(3==gsl_tp_type)
		{
		g_config_data_id = gsl_config_data_id_MJK;
		g_gsl_fw = GSLX680_FW_MJK;
		g_fw_size = ARRAY_SIZE(GSLX680_FW_MJK);
		g_config_data_id_size = ARRAY_SIZE(gsl_config_data_id_MJK);
		}	
	else
	{
	  PRINT_TP("GSL_IDENTY_TP not match type!!!!!\n");
	}
gsl_load_fw(client,g_gsl_fw,g_fw_size);	
#else
	gsl_load_fw(client);
#endif		
	startup_chip(client);
	reset_chip(client);
	startup_chip(client);		
}

static void check_mem_data(struct i2c_client *client)
{
	u8 read_buf[4]  = {0};
	
	msleep(30);
	i2c_smbus_read_i2c_block_data(client,0xb0, sizeof(read_buf), read_buf);
	
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
	{
		PRINT_TP("#########check mem read 0xb0 = %x %x %x %x #########\n", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip(client);
	}
}

#ifdef TPD_PROC_DEBUG
static int char_to_int(char ch)
{
    if(ch>='0' && ch<='9')
        return (ch-'0');
    else
        return (ch-'a'+10);
}

//static int gsl_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
static int gsl_config_read_proc(struct seq_file *m,void *v)
{
	//char *ptr = page;
	char temp_data[5] = {0};
	unsigned int tmp=0;
	//unsigned int *ptr_fw;
	
	if('v'==gsl_read[0]&&'s'==gsl_read[1])
	{
#ifdef GSL_NOID_VERSION
		tmp=gsl_version_id();
#else 
		tmp=0x20121215;
#endif
		//ptr += sprintf(ptr,"version:%x\n",tmp);
		seq_printf(m,"version:%x\n",tmp);
	}
	else if('r'==gsl_read[0]&&'e'==gsl_read[1])
	{
		if('i'==gsl_read[3])
		{
#ifdef GSL_NOID_VERSION 
	        
			tmp=(gsl_data_proc[5]<<8) | gsl_data_proc[4];
			//ptr +=sprintf(ptr,"gsl_config_data_id[%d] = ",tmp);
			seq_printf(m,"%d\n",g_config_data_id[tmp]);
#if 1//def GSL_IDENTY_TP

			if(tmp>=0&&tmp<g_config_data_id_size)
			{
			//ptr +=sprintf(ptr,"%d\n",gsl_config_data_id[tmp]); 
					seq_printf(m,"%d\n",g_config_data_id[tmp]); 
			}

#else		
			if(tmp>=0&&tmp<ARRAY_SIZE(gsl_config_data_id))
			{
			//ptr +=sprintf(ptr,"%d\n",gsl_config_data_id[tmp]); 
					seq_printf(m,"%d\n",gsl_config_data_id[tmp]); 
			}
#endif
#endif
		}
		else 
		{
			i2c_smbus_write_i2c_block_data(i2c_client,0xf0,4,&gsl_data_proc[4]);
			if(gsl_data_proc[0] < 0x80)
				i2c_smbus_read_i2c_block_data(i2c_client,gsl_data_proc[0],4,temp_data);
			i2c_smbus_read_i2c_block_data(i2c_client,gsl_data_proc[0],4,temp_data);

			//ptr +=sprintf(ptr,"offset : {0x%02x,0x",gsl_data_proc[0]);
			//ptr +=sprintf(ptr,"%02x",temp_data[3]);
			//ptr +=sprintf(ptr,"%02x",temp_data[2]);
			//ptr +=sprintf(ptr,"%02x",temp_data[1]);
			//ptr +=sprintf(ptr,"%02x};\n",temp_data[0]);
			seq_printf(m,"offset : {0x%02x,0x",gsl_data_proc[0]);
			seq_printf(m,"%02x",temp_data[3]);
			seq_printf(m,"%02x",temp_data[2]);
			seq_printf(m,"%02x",temp_data[1]);
			seq_printf(m,"%02x};\n",temp_data[0]);
		}
	}
	//*eof = 1;
	//return (ptr - page);
	return 0;
}
	//ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
//static ssize_t gsl_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
static ssize_t gsl_config_write_proc(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	u8 buf[8] = {0};
	char temp_buf[CONFIG_LEN];
	char *path_buf;
	int tmp = 0;
	int tmp1 = 0;
	print_info("[tp-gsl][%s] \n",__func__);
	if(count > 512)
	{
		print_info("size not match [%d:%ld]\n", CONFIG_LEN, count);
        return -EFAULT;
	}
	path_buf=kzalloc(count,GFP_KERNEL);
	if(!path_buf)
	{
		PRINT_TP("alloc path_buf memory error \n");
		return -1;
	}	
	//if(copy_from_user(path_buf, buffer, (count<CONFIG_LEN?count:CONFIG_LEN)))
	if(copy_from_user(path_buf, buffer, count))
	{
		print_info("copy from user fail\n");
		goto exit_write_proc_out;
	}
	memcpy(temp_buf,path_buf,(count<CONFIG_LEN?count:CONFIG_LEN));
	print_info("[tp-gsl][%s][%s]\n",__func__,temp_buf);
	
	buf[3]=char_to_int(temp_buf[14])<<4 | char_to_int(temp_buf[15]);	
	buf[2]=char_to_int(temp_buf[16])<<4 | char_to_int(temp_buf[17]);
	buf[1]=char_to_int(temp_buf[18])<<4 | char_to_int(temp_buf[19]);
	buf[0]=char_to_int(temp_buf[20])<<4 | char_to_int(temp_buf[21]);
	
	buf[7]=char_to_int(temp_buf[5])<<4 | char_to_int(temp_buf[6]);
	buf[6]=char_to_int(temp_buf[7])<<4 | char_to_int(temp_buf[8]);
	buf[5]=char_to_int(temp_buf[9])<<4 | char_to_int(temp_buf[10]);
	buf[4]=char_to_int(temp_buf[11])<<4 | char_to_int(temp_buf[12]);
	if('v'==temp_buf[0]&& 's'==temp_buf[1])//version //vs
	{
		memcpy(gsl_read,temp_buf,4);
		PRINT_TP("gsl version\n");
	}
	else if('s'==temp_buf[0]&& 't'==temp_buf[1])//start //st
	{
	#ifdef GSL_MONITOR
		cancel_delayed_work_sync(&gsl_monitor_work);
	#endif
		gsl_proc_flag = 1;
		reset_chip(i2c_client);
	}
	else if('e'==temp_buf[0]&&'n'==temp_buf[1])//end //en
	{
		msleep(20);
		reset_chip(i2c_client);
		startup_chip(i2c_client);
		gsl_proc_flag = 0;
	}
	else if('r'==temp_buf[0]&&'e'==temp_buf[1])//read buf //
	{
		memcpy(gsl_read,temp_buf,4);
		memcpy(gsl_data_proc,buf,8);
	}
	else if('w'==temp_buf[0]&&'r'==temp_buf[1])//write buf
	{
		i2c_smbus_write_i2c_block_data(i2c_client,buf[4],4,buf);
	}
#ifdef GSL_NOID_VERSION
	else if('i'==temp_buf[0]&&'d'==temp_buf[1])//write id config //
	{
		tmp1=(buf[7]<<24)|(buf[6]<<16)|(buf[5]<<8)|buf[4];
		tmp=(buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
#if  1//def GSL_IDENTY_TP  

			if(tmp1>=0&&tmp1<g_config_data_id_size)
			{
				g_config_data_id[tmp1] = tmp;
			}

	
#else	
		if(tmp1>=0 && tmp1<ARRAY_SIZE(gsl_config_data_id))
		{
			gsl_config_data_id[tmp1] = tmp;
		}
#endif
	}
#endif
exit_write_proc_out:
	kfree(path_buf);
	return count;
}

static int gsl_server_list_open(struct inode *inode,struct file *file)
{
	return single_open(file,gsl_config_read_proc,NULL);
}
static const struct file_operations gsl_seq_fops = {
	.open = gsl_server_list_open,
	.read = seq_read,
	.release = single_release,
	.write = gsl_config_write_proc,
	.owner = THIS_MODULE,
};
#endif


#ifdef FILTER_POINT
static void filter_point(u16 x, u16 y , u8 id)
{
	u16 x_err =0;
	u16 y_err =0;
	u16 filter_step_x = 0, filter_step_y = 0;
	
	id_sign[id] = id_sign[id] + 1;
	if(id_sign[id] == 1)
	{
		x_old[id] = x;
		y_old[id] = y;
	}
	
	x_err = x > x_old[id] ? (x -x_old[id]) : (x_old[id] - x);
	y_err = y > y_old[id] ? (y -y_old[id]) : (y_old[id] - y);

	if( (x_err > FILTER_MAX && y_err > FILTER_MAX/3) || (x_err > FILTER_MAX/3 && y_err > FILTER_MAX) )
	{
		filter_step_x = x_err;
		filter_step_y = y_err;
	}
	else
	{
		if(x_err > FILTER_MAX)
			filter_step_x = x_err; 
		if(y_err> FILTER_MAX)
			filter_step_y = y_err;
	}

	if(x_err <= 2*FILTER_MAX && y_err <= 2*FILTER_MAX)
	{
		filter_step_x >>= 2; 
		filter_step_y >>= 2;
	}
	else if(x_err <= 3*FILTER_MAX && y_err <= 3*FILTER_MAX)
	{
		filter_step_x >>= 1; 
		filter_step_y >>= 1;
	}	
	else if(x_err <= 4*FILTER_MAX && y_err <= 4*FILTER_MAX)
	{
		filter_step_x = filter_step_x*3/4; 
		filter_step_y = filter_step_y*3/4;
	}	
	
	x_new = x > x_old[id] ? (x_old[id] + filter_step_x) : (x_old[id] - filter_step_x);
	y_new = y > y_old[id] ? (y_old[id] + filter_step_y) : (y_old[id] - filter_step_y);

	x_old[id] = x_new;
	y_old[id] = y_new;
}
#else

static void record_point(u16 x, u16 y , u8 id)
{
	u16 x_err =0;
	u16 y_err =0;

	id_sign[id]=id_sign[id]+1;
	
	if(id_sign[id]==1){
		x_old[id]=x;
		y_old[id]=y;
	}

	x = (x_old[id] + x)/2;
	y = (y_old[id] + y)/2;
		
	if(x>x_old[id]){
		x_err=x -x_old[id];
	}
	else{
		x_err=x_old[id]-x;
	}

	if(y>y_old[id]){
		y_err=y -y_old[id];
	}
	else{
		y_err=y_old[id]-y;
	}

	if( (x_err > 3 && y_err > 1) || (x_err > 1 && y_err > 3) ){
		x_new = x;     x_old[id] = x;
		y_new = y;     y_old[id] = y;
	}
	else{
		if(x_err > 3){
			x_new = x;     x_old[id] = x;
		}
		else
			x_new = x_old[id];
		if(y_err> 3){
			y_new = y;     y_old[id] = y;
		}
		else
			y_new = y_old[id];
	}

	if(id_sign[id]==1){
		x_new= x_old[id];
		y_new= y_old[id];
	}
	
}
#endif

#ifdef GSL_IDENTY_TP	
#define GSL_C		100
#define GSL_CHIP_1	0xfc000000  //PB TP
#define GSL_CHIP_2	0xfc400000  //WJ TP
#define GSL_CHIP_3	0xfc800000  //MJKTP

static unsigned int gsl_count_one(unsigned int flag)
{
	unsigned int tmp=0; 
	int i =0;

	for (i=0 ; i<32 ; i++) {
		if (flag & (0x1 << i)) {
			tmp++;
		}
	}
	return tmp;
}

static int gsl_identify_tp(struct i2c_client *client)
{
	u8 buf[4];
	int err=1;
	int flag=0;
	unsigned int tmp,tmp0;
	unsigned int tmp1,tmp2,tmp3;
	u32 num;

identify_tp_repeat:
	clr_reg(client);
	reset_chip(client);
	num = ARRAY_SIZE(GSL_TP_CHECK);
	gsl_load_fw(client,GSL_TP_CHECK,num);
	startup_chip(client);
	msleep(200);
	i2c_smbus_read_i2c_block_data(client,0xb4,4,buf);
	print_info("the test 0xb4 = {0x%02x%02x%02x%02x}\n",buf[3],buf[2],buf[1],buf[0]);

	if (((buf[3] << 8) | buf[2]) > 1) {
		print_info("[TP-GSL][%s] is start ok\n",__func__);
		msleep(20);
		i2c_smbus_read_i2c_block_data(client,0xb8,4,buf);
		tmp = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
		print_info("the test 0xb8 = {0x%02x%02x%02x%02x}\n",buf[3],buf[2],buf[1],buf[0]);





		tmp1 = gsl_count_one(GSL_CHIP_1^tmp);
		tmp0 = gsl_count_one((tmp&GSL_CHIP_1)^GSL_CHIP_1); 
		tmp1 += tmp0*GSL_C;
		print_info("[TP-GSL] tmp1 = %d\n",tmp1);
		
		tmp2 = gsl_count_one(GSL_CHIP_2^tmp); 
		tmp0 = gsl_count_one((tmp&GSL_CHIP_2)^GSL_CHIP_2);
		tmp2 += tmp0*GSL_C;
		print_info("[TP-GSL] tmp2 = %d\n",tmp2);
		
		tmp3 = gsl_count_one(GSL_CHIP_3^tmp); 
		tmp0 = gsl_count_one((tmp&GSL_CHIP_3)^GSL_CHIP_3);
		tmp3 += tmp0*GSL_C;
		print_info("[TP-GSL] tmp3 = %d\n",tmp3);
		
		if (0xffffffff == GSL_CHIP_1) {
			tmp1=0xffff;
		}
		if (0xffffffff == GSL_CHIP_2) {
			tmp2=0xffff;
		}		
		if (0xffffffff == GSL_CHIP_3) {
			tmp3=0xffff;
		}

		print_info("[TP-GSL] tmp1 = %d\n",tmp1);
		print_info("[TP-GSL] tmp2 = %d\n",tmp2);
		print_info("[TP-GSL] tmp3 = %d\n",tmp3);

		tmp = tmp1;
		if (tmp1 > tmp2) {
			tmp = tmp2; 
		}		
		if (tmp > tmp3) {
			tmp = tmp3; 
		}

		if(tmp == tmp1)
		{
			
			gsl_tp_type = 1;
		} 
		else if(tmp == tmp2) 
		{
		
			gsl_tp_type = 2;
		}
		else if(tmp == tmp3) 
		{
		
			gsl_tp_type = 3;
		}
		else
		{
			
			gsl_tp_type = 1;	
		}
	
		err = 1;
	} else {
		flag++;
		if(flag < 1) {
			goto identify_tp_repeat;
		}
		err = 0;
	}
	return err; 
}
#endif

u8 rs_value1=0;
#ifdef TOUCHSWAP
int touchswap = 0; //TOUCHSWAP;
#endif

#ifdef TPD_ROTATION_SUPPORT
static void tpd_swap_xy(int *x, int *y)
{
        int temp = 0;

        temp = *x;
        *x = *y;
        *y = temp;
}

static void tpd_rotate_90(int *x, int *y)
{
        *x = SCREEN_MAX_X + 1 - *x;

        *x = (*x * SCREEN_MAX_Y) / SCREEN_MAX_X;
        *y = (*y * SCREEN_MAX_X) / SCREEN_MAX_Y;

        tpd_swap_xy(x, y);
}
static void tpd_rotate_180(int *x, int *y)
{
        *y = SCREEN_MAX_Y + 1 - *y;
        *x = SCREEN_MAX_X + 1 - *x;
}
static void tpd_rotate_270(int *x, int *y)
{
        *y = SCREEN_MAX_Y + 1 - *y;

        *x = (*x * SCREEN_MAX_Y) / SCREEN_MAX_X;
        *y = (*y * SCREEN_MAX_X) / SCREEN_MAX_Y;

        tpd_swap_xy(x, y);
}
#endif

void tpd_down( int id, int x, int y, int p) 
{
	//PRINT_TP("------tpd_down id: %d, x:%d, y:%d------[w:%d,h:%d] \n", id, x, y, SCREEN_MAX_X, SCREEN_MAX_Y);
#ifdef CONFIG_MTK_BOOT
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) 
#endif
		{
#if defined(GSL_CONVERTE_FACTORY)
        GSL_CONVERTE_FACTORY(x,y);
#endif
    } 
#ifdef CONFIG_MTK_BOOT
	else if (get_boot_mode() == NORMAL_BOOT || get_boot_mode() == ALARM_BOOT) 
#endif
{
#if 0
		//PRINT_TP("=====>In normal mode touchswap = %d=====>\n", touchswap);
		if (touchswap & 0x1) {
			int t = x;
			x = y*(SCREEN_MAX_X)/(SCREEN_MAX_Y);
			y = t*(SCREEN_MAX_Y)/(SCREEN_MAX_X);
		}
		
		if (touchswap & 0x2) {
			x = x*(SCREEN_MAX_X)/(SCREEN_MAX_Y);
			y = y*(SCREEN_MAX_Y)/(SCREEN_MAX_X);
		}
	
		if (touchswap & 0x4) {
			x = (SCREEN_MAX_X) - x;
		}
	
		if (touchswap & 0x8) {
			y = (SCREEN_MAX_Y) - y;
		}
#endif
#if defined(GSL_CONVERTE)
                GSL_CONVERTE(x,y);
              //  PRINT_TP("------tpd_down GSL_CONVERTE---->x:%d, y:%d\n", x, y);
#endif
        }
#ifdef TPD_ROTATION_SUPPORT
        switch (tpd_rotation_type) {
        case TPD_ROTATION_90:
                        tpd_rotate_90(&x, &y);
                        break;
        case TPD_ROTATION_270:
                        tpd_rotate_270(&x, &y);
                        break;
        case TPD_ROTATION_180:
                        tpd_rotate_180(&x, &y);
                        break;
        default:
                        break;
        }
#endif
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id); 	
	input_mt_sync(tpd->dev);
}

void tpd_up(void) 
{
	print_info("------tpd_up------ \n");

	input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_mt_sync(tpd->dev);
	
}

static void report_data_handle(void)
{
	u8 touch_data[MAX_FINGERS * 4 + 4] = {0};
	u8 buf[4] = {0};
	unsigned char id, point_num = 0;
	unsigned int x, y, temp_a, temp_b, i;

#ifdef GSL_NOID_VERSION
	struct gsl_touch_info cinfo={{0},{0},{0},0};
	int tmp1 = 0;
#endif
#ifdef GSL_MONITOR
	if(i2c_lock_flag != 0)
		return;
	else
		i2c_lock_flag = 1;
#endif

	
#ifdef TPD_PROC_DEBUG
    if(gsl_proc_flag == 1)
        return;
#endif

	i2c_smbus_read_i2c_block_data(i2c_client, 0x80, 4, &touch_data[0]);
	point_num = touch_data[0];
	if(point_num > 0)
		i2c_smbus_read_i2c_block_data(i2c_client, 0x84, 8, &touch_data[4]);
	if(point_num > 2)
		i2c_smbus_read_i2c_block_data(i2c_client, 0x8c, 8, &touch_data[12]);
	if(point_num > 4)
		i2c_smbus_read_i2c_block_data(i2c_client, 0x94, 8, &touch_data[20]);
	if(point_num > 6)
		i2c_smbus_read_i2c_block_data(i2c_client, 0x9c, 8, &touch_data[28]);
	if(point_num > 8)
		i2c_smbus_read_i2c_block_data(i2c_client, 0xa4, 8, &touch_data[36]);
	
#ifdef GSL_NOID_VERSION
	cinfo.finger_num = point_num;
	print_info("tp-gsl  finger_num = %d\n",cinfo.finger_num);
	for(i = 0; i < (point_num < MAX_CONTACTS ? point_num : MAX_CONTACTS); i ++)
	{
		temp_a = touch_data[(i + 1) * 4 + 3] & 0x0f;
		temp_b = touch_data[(i + 1) * 4 + 2];
		cinfo.x[i] = temp_a << 8 |temp_b;
		temp_a = touch_data[(i + 1) * 4 + 1];
		temp_b = touch_data[(i + 1) * 4 + 0];
		cinfo.y[i] = temp_a << 8 |temp_b;		
		cinfo.id[i] = ((touch_data[(i + 1) * 4 + 3] & 0xf0)>>4);
		print_info("tp-gsl  before: x[%d] = %d, y[%d] = %d, id[%d] = %d \n",i,cinfo.x[i],i,cinfo.y[i],i,cinfo.id[i]);
	}
	cinfo.finger_num = (touch_data[3]<<24)|(touch_data[2]<<16)|
		(touch_data[1]<<8)|touch_data[0];
	gsl_alg_id_main(&cinfo);
	tmp1=gsl_mask_tiaoping();
	print_info("[tp-gsl] tmp1=%x\n",tmp1);
	if(tmp1>0&&tmp1<0xffffffff)
	{
		buf[0]=0xa;buf[1]=0;buf[2]=0;buf[3]=0;
		i2c_smbus_write_i2c_block_data(i2c_client,0xf0,4,buf);
		buf[0]=(u8)(tmp1 & 0xff);
		buf[1]=(u8)((tmp1>>8) & 0xff);
		buf[2]=(u8)((tmp1>>16) & 0xff);
		buf[3]=(u8)((tmp1>>24) & 0xff);
		print_info("tmp1=%08x,buf[0]=%02x,buf[1]=%02x,buf[2]=%02x,buf[3]=%02x\n",
			tmp1,buf[0],buf[1],buf[2],buf[3]);
		i2c_smbus_write_i2c_block_data(i2c_client,0x8,4,buf);
	}
	point_num = cinfo.finger_num;
#endif

	for(i = 1 ;i <= MAX_CONTACTS; i ++)
	{
		if(point_num == 0)
			id_sign[i] = 0;	
		id_state_flag[i] = 0;
	}
	for(i = 0; i < (point_num < MAX_FINGERS ? point_num : MAX_FINGERS); i ++)
	{
	#ifdef GSL_NOID_VERSION
		id = cinfo.id[i];
		x =  cinfo.x[i];
		y =  cinfo.y[i];
	#else
		id = touch_data[(i + 1) * 4 + 3] >> 4;
		temp_a = touch_data[(i + 1) * 4 + 3] & 0x0f;
		temp_b = touch_data[(i + 1) * 4 + 2];
		x = temp_a << 8 |temp_b;
		temp_a = touch_data[(i + 1) * 4 + 1];
		temp_b = touch_data[(i + 1) * 4 + 0];
		y = temp_a << 8 |temp_b;	
	#endif
	
		if(1 <= id && id <= MAX_CONTACTS)
		{
		#ifdef FILTER_POINT
			filter_point(x, y ,id);
		#else
			record_point(x, y , id);
		#endif
			tpd_down(id, x_new, y_new, 10);
			id_state_flag[id] = 1;
		}
	}
	for(i = 1; i <= MAX_CONTACTS; i ++)
	{	
		if( (0 == point_num) || ((0 != id_state_old_flag[i]) && (0 == id_state_flag[i])) )
		{
			id_sign[i]=0;
		}
		id_state_old_flag[i] = id_state_flag[i];
	}			
	if(0 == point_num)
	{
		tpd_up();
	}
	input_sync(tpd->dev);
#ifdef GSL_MONITOR
	i2c_lock_flag = 0;
#endif
}

#ifdef GSL_MONITOR
static void gsl_monitor_worker(struct work_struct *data)
{
	//u8 write_buf[4] = {0};
	u8 read_buf[4]  = {0};
	char init_chip_flag = 0;
	
	print_info("----------------gsl_monitor_worker-----------------\n");	

	if(i2c_lock_flag != 0)
		goto queue_monitor_work;
	else
		i2c_lock_flag = 1;
	
	i2c_smbus_read_i2c_block_data(i2c_client, 0xb0, 4, read_buf);
	if(read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
		b0_counter ++;
	else
		b0_counter = 0;

	if(b0_counter > 1)
	{
		PRINT_TP("======read 0xb0: %x %x %x %x ======\n",read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip_flag = 1;
		b0_counter = 0;
		goto queue_monitor_init_chip;
	}

	i2c_smbus_read_i2c_block_data(i2c_client, 0xb4, 4, read_buf);	
	
	int_2nd[3] = int_1st[3];
	int_2nd[2] = int_1st[2];
	int_2nd[1] = int_1st[1];
	int_2nd[0] = int_1st[0];
	int_1st[3] = read_buf[3];
	int_1st[2] = read_buf[2];
	int_1st[1] = read_buf[1];
	int_1st[0] = read_buf[0];

	if (int_1st[3] == int_2nd[3] && int_1st[2] == int_2nd[2] &&int_1st[1] == int_2nd[1] && int_1st[0] == int_2nd[0]) 
	{
		PRINT_TP("======int_1st: %x %x %x %x , int_2nd: %x %x %x %x ======\n",int_1st[3], int_1st[2], int_1st[1], int_1st[0], int_2nd[3], int_2nd[2],int_2nd[1],int_2nd[0]);
		init_chip_flag = 1;
		goto queue_monitor_init_chip;
	}
#if 1 //version 1.4.0 or later than 1.4.0 read 0xbc for esd checking
	i2c_smbus_read_i2c_block_data(i2c_client, 0xbc, 4, read_buf);
	if(read_buf[3] != 0 || read_buf[2] != 0 || read_buf[1] != 0 || read_buf[0] != 0)
		bc_counter++;
	else
		bc_counter = 0;
	if(bc_counter > 1)
	{
		PRINT_TP("======read 0xbc: %x %x %x %x======\n",read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip_flag = 1;
		bc_counter = 0;
	}
#else
	write_buf[3] = 0x01;
	write_buf[2] = 0xfe;
	write_buf[1] = 0x10;
	write_buf[0] = 0x00;
	i2c_smbus_write_i2c_block_data(i2c_client, 0xf0, 4, write_buf);
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 4, read_buf);
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 4, read_buf);
	
	if(read_buf[3] < 10 && read_buf[2] < 10 && read_buf[1] < 10 && read_buf[0] < 10)
		dac_counter ++;
	else
		dac_counter = 0;

	if(dac_counter > 1) 
	{
		PRINT_TP("======read DAC1_0: %x %x %x %x ======\n",read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip_flag = 1;
		dac_counter = 0;
	}
#endif
queue_monitor_init_chip:
	if(init_chip_flag)
		init_chip(i2c_client);
	
	i2c_lock_flag = 0;
	
queue_monitor_work:	
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 100);
}
#endif


int tp_check_flag;
#if defined(ATA_TP_ADDR)
#define RAWDATA_ADDR	ATA_TP_ADDR
#else
#define RAWDATA_ADDR	0x5f80
#endif

#if defined(ATA_TP_DRV_NUM)
#define DRV_NUM			ATA_TP_DRV_NUM
#else
#define DRV_NUM			15
#endif

#if defined(ATA_TP_RAWDATA_THRESHOLD)
#define RAWDATA_THRESHOLD			ATA_TP_RAWDATA_THRESHOLD
#else
#define RAWDATA_THRESHOLD			6000
#endif
#if defined(ATA_TP_DAC_THRESHOLD)
#define DAC_THRESHOLD			ATA_TP_DAC_THRESHOLD
#else
#define DAC_THRESHOLD			20
#endif

#if defined(ATA_TP_SEN_NUM)
#define SEN_NUM			ATA_TP_SEN_NUM
#else
#define SEN_NUM			10
#endif

#if defined(ATA_TP_SEN_SENSOR)
#define SEN_ORDER			ATA_TP_SEN_SENSOR
#else
#define SEN_ORDER			{0,1,2,3,4,5,6,7,8,9}
#endif

#if defined(ATA_DRV_FAIL_MAX)
#define DRV_FAIL_MAX	ATA_DRV_FAIL_MAX
#else
#define DRV_FAIL_MAX	(2)
#endif

#if defined(ATA_SEN_FAIL_MAX)
#define SEN_FAIL_MAX 	ATA_SEN_FAIL_MAX
#else
#define SEN_FAIL_MAX	(2)
#endif

#ifdef CONFIG_GSL5680_ATA_TEST

#if defined(GSL5680_RAWDATA_THRESHOLD_MIN)
#define RAWDATA_THRESHOLD_MIN GSL5680_RAWDATA_THRESHOLD_MIN
#else
#define RAWDATA_THRESHOLD_MIN (1000)
#endif

#if defined(GSL5680_RAWDATA_THRESHOLD_MAX)
#define RAWDATA_THRESHOLD_MAX GSL5680_RAWDATA_THRESHOLD_MAX
#else
#define RAWDATA_THRESHOLD_MAX (7000)
#endif

#endif

static const u8 sen_order[SEN_NUM] = SEN_ORDER;

static int ctp_factory_test (void)
{
	u8 buf[4];
        int i, offset;
	u32 rawdata_value, dac_value;
	int drv_fail = 0;
	int sen_fail = 0;
	

	struct i2c_client *client = i2c_client;
	
	PRINT_TP(KERN_EMERG "=====>RAWDATA_ADDR=0x%x=====>\n",RAWDATA_ADDR);
	PRINT_TP(KERN_EMERG "=====>DRV_NUM=%d=====>\n",DRV_NUM);
	PRINT_TP(KERN_EMERG "=====>RAWDATA_THRESHOLD=%d=====>\n",RAWDATA_THRESHOLD);
	PRINT_TP(KERN_EMERG "=====>DAC_THRESHOLD=%d=====>\n",DAC_THRESHOLD);
	PRINT_TP(KERN_EMERG "=====>SEN_NUM=%d=====>\n",SEN_NUM);
#ifdef CONFIG_GSL5680_ATA_TEST
	PRINT_TP(KERN_EMERG "=====>RAWDATA_THRESHOLD_MIN=%d=====>\n",RAWDATA_THRESHOLD_MIN);
	PRINT_TP(KERN_EMERG "=====>RAWDATA_THRESHOLD_MAX=%d=====>\n",RAWDATA_THRESHOLD_MAX);
#endif
	
        if(!client)
	{
		PRINT_TP("err ,client is NULL,ctp_factory_test\n");
        return -1;
	}
	
	msleep(800);	
	for(i = 0; i < DRV_NUM; i ++)
	{
		buf[3] = 0;
		buf[2] = 0;
		buf[1] = 0;
		buf[0] = (RAWDATA_ADDR + SEN_NUM*2*i)/0x80;
		offset = (RAWDATA_ADDR + SEN_NUM*2*i)%0x80;
		i2c_smbus_write_i2c_block_data(client, 0xf0, 4, buf);
		i2c_smbus_read_i2c_block_data(client, offset, 4, buf);
		i2c_smbus_read_i2c_block_data(client, offset, 4, buf);
		rawdata_value = (buf[1]<<8) + buf[0];
		PRINT_TP(KERN_EMERG "(buf[1]<<8) + buf[0] %s,rawdata_value = %d\n",__func__,rawdata_value);
#ifdef CONFIG_GSL5680_ATA_TEST
		if ((rawdata_value < RAWDATA_THRESHOLD_MIN) || (rawdata_value > RAWDATA_THRESHOLD_MAX))
		{
				if(drv_fail++ <= DRV_FAIL_MAX)
				         continue;
				return -1; //fail
		}
#else

		if(rawdata_value > RAWDATA_THRESHOLD)
		{
			rawdata_value = (buf[3]<<8) + buf[2];
		        PRINT_TP(KERN_EMERG "(buf[3]<<8) + buf[2] %s,===>rawdata_value = %d\n",__func__,rawdata_value);
				
			if(rawdata_value > RAWDATA_THRESHOLD)
			{
		                PRINT_TP(KERN_EMERG "(buf[3]<<8) + buf[2] fail %s, ############>rawdata_value = %d\n",__func__,rawdata_value);
				if(drv_fail++ <= DRV_FAIL_MAX)
				         continue;

				return -1; //fail
			}
		}
#endif
	}
#ifdef CONFIG_GSL5680_ATA_TEST
	return 0;
#endif
	
	for(i = 0; i < SEN_NUM; i ++)
	{
		buf[3] = 0x01;
		buf[2] = 0xfe;
		buf[1] = 0x10;
		buf[0] = 0x00;
		offset = 0x10 + (sen_order[i]/4)*4;
		i2c_smbus_write_i2c_block_data(client, 0xf0, 4, buf);
		i2c_smbus_read_i2c_block_data(client, offset, 4, buf);
		i2c_smbus_read_i2c_block_data(client, offset, 4, buf);

		dac_value = buf[sen_order[i]%4];
		PRINT_TP(KERN_EMERG "================dac_value = %d DAC_THRESHOLD = %d===================\n",dac_value,DAC_THRESHOLD);
		if(dac_value < DAC_THRESHOLD) {
			if (sen_fail++ <= SEN_FAIL_MAX)
				continue;
		
			return -1; //fail
		}
	}
	
	return 0; //pass
}


static int touch_event_handler(void *unused)
{
	//struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	struct sched_param param = { .sched_priority = 4 };
	sched_setscheduler(current, SCHED_RR, &param);
	
	do
	{
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		TPD_DEBUG_SET_TIME;
		

		set_current_state(TASK_RUNNING);
		print_info("===touch_event_handler, task running===\n");

		eint_flag = 0;
		report_data_handle();
		 #if defined(CONFIG_T809_TPD_WAKE)
			
		 if(tpd_gsl_suspend==3)		
		 	{	
		 	PRINT_TP("x_new=%d,y_new=%d\n",x_new,y_new);
		          tpd_direct_key(KEY_NUM_CODE);
			  tpd_gsl_suspend=0;		 
			  }		
		 #endif
	} while (!kthread_should_stop());
	
	return 0;
}

 static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	print_info("===tpd irq interrupt===\n");

	eint_flag = 1;
	tpd_flag=1;
#if defined(CONFIG_T809_TPD_WAKE)
	if(  tpd_gsl_suspend==1)
		  tpd_gsl_suspend=2;
	else if(tpd_gsl_suspend==2)
		tpd_gsl_suspend=3;
	else
		;
#endif
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) {
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	node = of_find_matching_node(NULL, touch_of_match);

	if (node) {
		tpd_gpio_as_int(IRQ_GPIO);
		touch_irq = irq_of_parse_and_map(node, 0);
		if (!touch_irq) {
			PRINT_TP("touch_irq get fail!!\n");
			return -EINVAL;
		}
		ret = request_irq(touch_irq, tpd_eint_interrupt_handler,
					IRQF_TRIGGER_FALLING, TPD_DEVICE, NULL);
			if (ret > 0)
				PRINT_TP("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	} else {
		PRINT_TP("[%s] tpd request_irq can not find touch eint device node!.", __func__);
	}
	enable_irq(touch_irq);
	return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	int err = 0;
	int ret;

	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		PRINT_TP("Failed to enable reg-vgp2: %d\n", ret);

	tpd_gpio_output(RST_GPIO, 1);
	msleep(100);

	
	ret = test_i2c(client);
	if(ret < 0)
	{
		PRINT_TP("------gslX680 test_i2c error------\n");	
		return -1;
	}	

	i2c_client = client;	
	init_chip(i2c_client);
	check_mem_data(i2c_client);

	ret = tpd_irq_registration();
	if(ret < 0)
	{
		PRINT_TP("------gslX680 irq register error------\n");	
		return -1;
	}	
#ifdef GSL_IDENTY_TP
	
#else 
	g_config_data_id = gsl_config_data_id;
	g_gsl_fw = GSLX680_FW;
	g_fw_size = ARRAY_SIZE(GSLX680_FW);
	g_config_data_id_size = ARRAY_SIZE(gsl_config_data_id);
#endif
	//msleep(100);
	tpd_load_status = 1;
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		PRINT_TP(" failed to create kernel thread: %d\n", err);
	}

#ifdef GSL_MONITOR
	PRINT_TP( "tpd_i2c_probe () : queue gsl_monitor_workqueue\n");

	INIT_DELAYED_WORK(&gsl_monitor_work, gsl_monitor_worker);
	gsl_monitor_workqueue = create_singlethread_workqueue("gsl_monitor_workqueue");
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 1000);
#endif

#ifdef TPD_PROC_DEBUG
#if 0
    gsl_config_proc = create_proc_entry(GSL_CONFIG_PROC_FILE, 0666, NULL);
    //PRINT_TP("[tp-gsl] [%s] gsl_config_proc = %x \n",__func__,gsl_config_proc);
	if (gsl_config_proc == NULL)
	{
		print_info("create_proc_entry %s failed\n", GSL_CONFIG_PROC_FILE);
	}
	else
	{
		gsl_config_proc->read_proc = gsl_config_read_proc;
		gsl_config_proc->write_proc = gsl_config_write_proc;
	}
#else
    proc_create(GSL_CONFIG_PROC_FILE,0666,NULL,&gsl_seq_fops);
#endif
    gsl_proc_flag = 0;
#endif

	PRINT_TP("==tpd_i2c_probe end==\n");
		
	return 0;
}

static int tpd_i2c_remove(struct i2c_client *client)
{
	PRINT_TP("==tpd_i2c_remove==\n");
	
	return 0;
}


static const struct i2c_device_id tpd_i2c_id[] = {{TPD_DEVICE,0},{}};
#ifdef ADD_I2C_DEVICE_ANDROID_4_0
//static struct i2c_board_info __initdata gslX680_i2c_tpd={ I2C_BOARD_INFO(TPD_DEVICE, (GSLX680_ADDR))};
#else
static unsigned short force[] = {0, (GSLX680_ADDR << 1), I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = { .forces = forces,};
#endif

struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(gslx68x_dt_match),
		.name = "gslx680",
	#ifndef ADD_I2C_DEVICE_ANDROID_4_0	 
		.owner = THIS_MODULE,
	#endif
	},
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.id_table = tpd_i2c_id,
	.detect = tpd_i2c_detect,
//	#ifndef ADD_I2C_DEVICE_ANDROID_4_0
//	.address_data = &addr_data,
//	#endif
};

int tpd_local_init(void)
{
	int retval;
	PRINT_TP("gslx68x:tpd_local_init\n");
	
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);
	if (retval != 0) {
		PRINT_TP("Failed to set vtouch voltage: %d\n", retval);
		return -1;
	}

	if(i2c_add_driver(&tpd_i2c_driver)!=0) {
		PRINT_TP("unable to add i2c driver.\n");
		return -1;
	}
	
	if(tpd_load_status == 0)
	{
		PRINT_TP("add error touch panel driver.\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (MAX_CONTACTS+1), 0, 0);//

	
#ifdef TPD_HAVE_BUTTON
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy(tpd_calmat, tpd_calmat_local, 8*4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);
#endif
	tpd_type_cap = 1;

	PRINT_TP("gslx68x:tpd_local_init end.\n");
	return 0;
}

/* Function to manage low power suspend */
//void tpd_suspend(struct early_suspend *h)
static void tpd_suspend(struct device *h)
{
	PRINT_TP("==tpd_suspend==\n");
#ifdef TPD_PROC_DEBUG
	if(gsl_proc_flag == 1){
		return;
	}
#endif
	tpd_halt = 1;
     
       #if defined(CONFIG_T809_TPD_WAKE)      
         tpd_gsl_suspend=1;		 
	#else	
	disable_irq(touch_irq);
	 #endif
#ifdef GSL_MONITOR
	PRINT_TP( "gsl_ts_suspend () : cancel gsl_monitor_work\n");
	cancel_delayed_work_sync(&gsl_monitor_work);
#endif
#if !defined(CONFIG_T809_TPD_WAKE)

	tpd_gpio_output(RST_GPIO, 0);
#endif
}

/* Function to manage power-on resume */
//void tpd_resume(struct early_suspend *h)
static void tpd_resume(struct device *h)
{
	PRINT_TP("==tpd_resume==\n");
	
#ifdef TPD_PROC_DEBUG
    if(gsl_proc_flag == 1){
        return;
    }
#endif	

	tpd_gpio_output(RST_GPIO, 1);
	msleep(20);	
	reset_chip(i2c_client);
	startup_chip(i2c_client);
	check_mem_data(i2c_client);	
#ifdef GSL_MONITOR
	PRINT_TP( "gsl_ts_resume () : queue gsl_monitor_work\n");
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 300);
#endif	
#if !(defined(CONFIG_T809_TPD_WAKE))

	enable_irq(touch_irq);
#endif
	tpd_halt = 0;
#if defined(CONFIG_T809_TPD_WAKE) 
	tpd_gsl_suspend=0;
#endif

}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gslx680",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
	.factory_test = ctp_factory_test,
};
/////////////////////////////////////////////////////////////////////////////
//u8 rs_value1=0;
#if 0
static ssize_t db_value_store(struct class *class, 
			struct class_attribute *attr, const char *buf, size_t count)
{	
	int rs_tmp;
	
	rs_tmp = simple_strtoul(buf, NULL, 10);
	
	rs_value1=rs_tmp;
	
	//sprintf(buf,"rs_value1 = %d \r\n",rs_value1);
	return count;
}

static ssize_t db_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	return sprintf(buf,"rs_value1 = %d \r\n" ,rs_value1 );
}

static struct class_attribute db_class_attrs[] = { 
	__ATTR(db,0644,db_value_show,db_value_store),
    __ATTR_NULL
};

static struct class db_interface_class = {
        .name = "db_interface",
        //.class_attrs = db_class_attrs,
    };

#endif 


#ifdef TOUCHSWAP
static ssize_t touchswap_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	int rs_tmp;
	
	rs_tmp = simple_strtoul(buf, NULL, 10);
	
	touchswap=rs_tmp;
	
	sprintf(buf,"touchswap = %d \r\n",touchswap);
	return count;
}

static ssize_t touchswap_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	return sprintf(buf,"touchswap = %d \r\n" ,touchswap);
}

#endif

#ifdef TOUCHSWAP_FACTORY_MODE
static ssize_t touchswap_factory_mode_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	int rs_tmp;
	
	rs_tmp = simple_strtoul(buf, NULL, 10);
	
	touchswap_factory_mode=rs_tmp;
	
	sprintf(buf,"touchswap_factory_mode = %d \r\n",touchswap_factory_mode);
	return count;
}

static ssize_t touchswap_factory_mode_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	return sprintf(buf,"touchswap_factory_mode = %d \r\n" ,touchswap_factory_mode);
}
#endif


#if defined(TOUCHSWAP) || defined(TOUCHSWAP_FACTORY_MODE)
static struct class_attribute touchswap_class_attrs[] = { 
#ifdef TOUCHSWAP
	__ATTR(touchswap,0644,touchswap_value_show,touchswap_value_store),
#endif
#ifdef TOUCHSWAP_FACTORY_MODE
	__ATTR(touchswap_factory_mode,0644,touchswap_factory_mode_value_show,touchswap_factory_mode_value_store),
#endif
    __ATTR_NULL
};
#endif

#if defined(TOUCHSWAP) || defined(TOUCHSWAP_FACTORY_MODE)
static struct class touchswap_interface_class = {
        .name = "touchswap_interface",
        .class_attrs = touchswap_class_attrs,
    };
#endif

#ifdef CONFIG_T8270A_JIANRONG_V6_V6L

extern int gsl_8270a_tpd_i2c_num();
#endif

/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	PRINT_TP("gslX680 touch panel driver init\n");
	tpd_get_dts_info();
    //ret = class_register(&db_interface_class);	
#if defined(TOUCHSWAP) || defined(TOUCHSWAP_FACTORY_MODE)
    ret = class_register(&touchswap_interface_class);	
#endif
	if(tpd_driver_add(&tpd_device_driver) < 0)
		PRINT_TP("add gslX680 driver failed\n");
	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
	PRINT_TP("Sileadinc gslX680 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);


