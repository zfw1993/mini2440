#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <linux/notifier.h>

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>

volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;

static struct class *seconddrv_class;       //定义一个类
static struct device *seconddrv_class_dev;  //类下面再创建一个设备


ssize_t second_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	/* 返回6个引脚的电平 */
	unsigned char key_vals[6];
	int regval; // 在arm里，int 与 long是一样的

	if (size != sizeof(key_vals))
		return -EINVAL;    

	//读GPG引脚	
	regval = *gpgdat;  //记得是*gpgdat

	key_vals[0] = (regval & (1<<0)) ? 1 : 0;	
	key_vals[1] = (regval & (1<<3)) ? 1 : 0;
	key_vals[2] = (regval & (1<<5)) ? 1 : 0;
	key_vals[3] = (regval & (1<<6)) ? 1 : 0;
	key_vals[4] = (regval & (1<<7)) ? 1 : 0;
	key_vals[5] = (regval & (1<<11)) ? 1 : 0;

	//把值返回给用户程序
	if (copy_to_user(buf, key_vals, sizeof(key_vals)))
			return -EFAULT;
	
	return sizeof(key_vals);
}


static int second_drv_open(struct inode *inode,struct file *file)
{
	//配置GPG0.3.5.6.7.11为输入引脚
	*gpgcon &= ~( (0x3<<(0*2)) | (0x3<<(3*2)) | (0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(11*2)) );  //清零
	*gpgcon |=  ( (0x0<<(0*2)) | (0x0<<(3*2)) | (0x0<<(5*2)) | (0x0<<(6*2)) | (0x0<<(7*2)) | (0x0<<(11*2)) );  //配置为输入
	return 0;	
}

static struct file_operations second_drv_fops = //告诉内核有这个驱动程序
{
	.owner = THIS_MODULE, //这是一个宏，指向编译模块时，自动创建的_this_modulesy变量
	.open  = second_drv_open,
	.read  = second_drv_read,
};

int major;
static int second_drv_init(void)  //entry 
{
	major = register_chrdev(0,"second_drv", &second_drv_fops);  //注册，告诉内核，挂载驱动程序

	seconddrv_class = class_create(THIS_MODULE, "seconddrv");	
	seconddrv_class_dev = device_create(seconddrv_class, NULL, MKDEV(major,0), NULL, "buttons");
		
	gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16); //16个字节
	gpgdat = gpgcon + 1;  /*指针+1 : 表示内容加4个字节 = 0x56000014*/

	return 0;
}

static void second_drv_exit(void) 
{
	unregister_chrdev(major,"second_drv");  //注册，告诉内核，挂载驱动程序
	//主设备号，名字，结构体
	device_destroy(seconddrv_class, MKDEV(major,0));         //卸载设备
	class_destroy(seconddrv_class);                //再卸载类

	iounmap(gpgcon);  //物理地址
}

module_init(second_drv_init);
module_exit(second_drv_exit);

MODULE_LICENSE("GPL");


