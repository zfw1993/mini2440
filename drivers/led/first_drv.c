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
#include <linux/mutex.h>


static struct class *firstdrv_class;       /* 定义一个类 */ 
static struct device *firstdrv_class_dev;  /* 类下面再创建一个设备 */

volatile unsigned long *gpbcon = NULL; 
volatile unsigned long *gpbdat = NULL;


static int first_drv_open(struct inode *inode,struct file *file)
{
	//int minor = MINOR(inode->i_rdev);	//get minor devices number
	
	//printk("first_drv_open\n");
	/* 配置GPB5,6,7,8为输出 */
	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));  //清零
	*gpbcon |=  ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));  //配置为输出
	return 0;
}

//这个函数对应于用户程序的 write(fd, &val, 4); 最后一上参数不用管    &val <=> buf
static ssize_t first_drv_write(struct file *file,const char __user *buf,size_t count,loff_t *ppos)
{
	int des_val;  /*目地地址*/

	//int minor = MINOR(filp->f_dentry->d_inode->i_rdev);  //please to read the "myleds.c" 'file 
	
	//printk("first_drv_wirte\n");

	//copy_from_user(void * to,const void __user * from,unsigned long n)
	//参数格式 : 目地地址  源地址  长度  

	if (copy_from_user(&des_val, buf, count)) //把用户空间传进来的值拷贝到des_val //向内核传递数据
			return -EFAULT; /* 而从内核拷贝数据到用户空间用的函数是: copy_to_user(); */

	if (1 == des_val)
	{
		/*led on*/
		*gpbdat &= ~((1<<5) | (1<<6) | (1<<7) | (1<<8));
	}
	else
	{   
		/*led off*/
		*gpbdat |= ((1<<5) | (1<<6) | (1<<7) | (1<<8));
	}
	
	return 0;
}

static struct file_operations first_drv_fops = //告诉内核有这个驱动程序
{
	.owner = THIS_MODULE, //这是一个宏，指向编译模块时，自动创建的_this_modulesy变量
	.open  = first_drv_open,
	.write = first_drv_write,
};

int major; //让系统为我们分配主设备号
static int first_drv_init(void)  //入口函数
{
	major = register_chrdev(0, "first_drv", &first_drv_fops); /* 注册，告诉内核，挂载驱动程序 */
	/* 主设备号，名字，结构体 */

	/* 生成系统信息，在系统目录(/sys/)下建立firstdvc这个类，类下面会被创建一个名为zfw这个设备 */
	firstdrv_class = class_create(THIS_MODULE,"firstdrv");  /* 然后mdev会自动创建一个/dev/zfw 的设备节点 */
	/* 先建立一个类  */                                                        

	firstdrv_class_dev = device_create(firstdrv_class, NULL, MKDEV(major,0), NULL, "zfw");
	/* 再在类下面建立一个设备 */                  

	/***********************建立映射关系**********************************/

	/* ioremap(开始地址, 长度) */
	gpbcon = (volatile unsigned long *)ioremap(0x56000010, 16); //16个字节
	/* 关于GPB的寄存器有4个（4*4=16）*/
	gpbdat = gpbcon + 1;  /*指针+1 : 表示内容加4个字节 = 0x56000014*/
	
	return 0;          
}

static void first_drv_exit(void)  /* 入口函数 */
{
	unregister_chrdev(major, "first_drv"); /* 卸载驱动程序 */
	//主设备号，名字
	device_destroy(firstdrv_class, MKDEV(major,0));  //卸载设备
	class_destroy(firstdrv_class);                //再卸载类

	/************************去除映射关系**********************************/

	iounmap(gpbcon);  //物理地址
}


module_init(first_drv_init); //修饰入口函数，告诉内核有这个入口函数
module_exit(first_drv_exit); //修饰出口函数


MODULE_LICENSE("GPL");


