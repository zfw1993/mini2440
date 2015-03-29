#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/device.h>
#include <mach/gpio.h>
#include <linux/interrupt.h>
#include <linux/poll.h>



volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* 中断事件标志, 中断服务程序将它置1，fifth_drv_read将它清0 */
static volatile int ev_press = 0;

static struct fasync_struct *button_async;

static struct class *fifthdrv_class;                 //定义一个类
static struct device *fifthdrv_class_dev;  //类下面再创建一个设备


static unsigned char g_save_key_val;  //保存键值

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

//下面的值，都是自己设定的，可以随意设定
//  键值 : 按下时，0x01,0x02,0x03,0x04,0x05,0x06
//  键值 : 松开时，0x81,0x82,0x83,0x84,0x85,0x86

struct pin_desc pin_desc[6] =  //结构体数组
{
	{S3C2410_GPG(0),  0x01},
	{S3C2410_GPG(3),  0x02},
	{S3C2410_GPG(5),  0x03},
	{S3C2410_GPG(6),  0x04},
	{S3C2410_GPG(7),  0x05},
	{S3C2410_GPG(11), 0x06},
};

/*
 * 确定按键值
 */
static irqreturn_t buttons_irq(int irq, void *dev_id)  //中断服务函数
{
	//printk("irq = %d\n", irq); //打印出中断号

	struct pin_desc *pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	 
	pinval = s3c2410_gpio_getpin(pindesc->pin);  // 读引脚状态

//	printk("pinval = %d\n", pinval);

	if (pinval) // 不可以这么写 if (1 == pinval) 因为pinval的值为0,8,32,128,64,2048
	{
		//松开
		g_save_key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//按下
		g_save_key_val = pindesc->key_val;
	}

	ev_press = 1;  //置1
	wake_up_interruptible(&button_waitq);  //唤醒休眠的进程，把挂在队列的进程唤醒
	
	kill_fasync(&button_async, SIGIO, POLL_IN); //发送信号，发送的对象肯定在button_async这个结构体当中
	
	return IRQ_RETVAL(IRQ_HANDLED);
}


static int fifth_drv_open(struct inode *inode,struct file *file) //打开设备后，才会调用request_irq
{
	//配置GPG0.3.5.6.7.11为中断引脚
	request_irq(IRQ_EINT8 , buttons_irq, IRQ_TYPE_EDGE_BOTH, "K1", &pin_desc[0]);
	request_irq(IRQ_EINT11, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K2", &pin_desc[1]);
	request_irq(IRQ_EINT13, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K3", &pin_desc[2]);
	request_irq(IRQ_EINT14, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K4", &pin_desc[3]);
	request_irq(IRQ_EINT15, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K5", &pin_desc[4]);
	request_irq(IRQ_EINT19, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K6", &pin_desc[5]);

	return 0;	      		
}          

static int fifth_drv_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT8 , &pin_desc[0]);  //释放中断
	free_irq(IRQ_EINT11, &pin_desc[1]);
	free_irq(IRQ_EINT13, &pin_desc[2]);
	free_irq(IRQ_EINT14, &pin_desc[3]);
	free_irq(IRQ_EINT15, &pin_desc[4]);
	free_irq(IRQ_EINT19, &pin_desc[5]);
	
	return 0;
}                        

static ssize_t fifth_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{	
	if (size != 1) 
		return -EINVAL;

	//如果没有按键动作，休眠
	wait_event_interruptible(button_waitq, ev_press);  //把进程挂在队列当中,此时ev_press为0
	
	//有按键动作，返回键值
	if (copy_to_user(buf, &g_save_key_val, 1))
		return -EFAULT;

	ev_press = 0;  //清零
	
	return 1;
}

static unsigned int fifth_drv_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &button_waitq, wait);  //把进程挂到队列里面去，但不会立刻休眠

	if (ev_press)
		mask |= POLLIN | POLLRDNORM;  

	return mask;   //如果返回为0，那么count++不会执行，那么就会进入休眠
}


static int fifth_drv_fasync(int fd, struct file *filp, int on)
{
	printk("driver: fifth_drv_fasync\n");  //用于调试，看会不会执行这一步
	
	return fasync_helper(fd, filp, on, &button_async); //初始化button_async
}


static struct file_operations fifth_drv_fops = //告诉内核有这个驱动程序
{
	.owner   = THIS_MODULE, //这是一个宏，指向编译模块时，自动创建的_this_modulesy变量
	.open    = fifth_drv_open,
	.read    = fifth_drv_read,
	.release = fifth_drv_close,
	.poll    = fifth_drv_poll,
	.fasync  = fifth_drv_fasync,
};

int major;
static int fifth_drv_init(void)  //entry 
{
	major = register_chrdev(0,"fifth_drv", &fifth_drv_fops);  //注册，告诉内核，挂载驱动程序

	fifthdrv_class = class_create(THIS_MODULE,"fifthdrv");	
	fifthdrv_class_dev = device_create(fifthdrv_class, NULL, MKDEV(major,0), NULL, "buttons");

	return 0;
}

static void fifth_drv_exit(void) 
{
	unregister_chrdev(major,"fifth_drv");  //注册，告诉内核，挂载驱动程序

	device_destroy(fifthdrv_class, MKDEV(major,0));  //卸载设备
	class_destroy(fifthdrv_class);                   //再卸载类
}

module_init(fifth_drv_init);
module_exit(fifth_drv_exit);

MODULE_LICENSE("GPL");


