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

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);  /* 定义一个队列 */

/* 中断事件标志, 中断服务程序将它置1， sixth_drv_read将它清0 */
static volatile int ev_press = 0;

static struct fasync_struct *button_async;
static struct timer_list buttons_timer;


static struct class *sixthdrv_class;       //定义一个类
static struct device *sixthdrv_class_dev;  //类下面再创建一个设备

static unsigned char g_save_key_val;       //保存键值

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

//  下面的值，都是自己设定的，可以随意设定
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

static struct pin_desc *irq_pd;

/*
 * 确定按键值
 */
static irqreturn_t buttons_irq(int irq, void *dev_id)  //中断服务函数
{
	irq_pd = (struct pin_desc *)dev_id;  //用一个全局变量来保存dev_id
	
	// 10ms后启动定时器
	mod_timer(&buttons_timer, jiffies+HZ/100);    // modified timer 修改定时器时间
	
	return IRQ_RETVAL(IRQ_HANDLED);
}

#if 0
atomic_t canopen = ATOMIC_INIT(1); //定义原子变量canopen,并初始化为1
#endif

//static DECLARE_MUTEX(button_lock);          // 2.6.22.6
static DEFINE_SEMAPHORE(button_lock);     //定义互斥锁

static int  sixth_drv_open(struct inode *inode,struct file *file) //打开设备后，才会调用request_irq
{
#if 0
	if (!atomic_dec_and_test(&canopen))  //如果没有人打开过它，那么canopen是为1的，那么不会进入这个结构
	{
		atomic_inc(&canopen);
		return -EBUSY; //如果有人打开了这个设备，那么返回EBUSY
	}
	//如果执行到这里，那么canopen == 0
#endif

	//想实现的效果是:
	//如果是非阻塞，并且无法获得信号量，则返回一个错误
	//如果是阻塞，并且无法获得信号量，则陷入休眠
	if (file->f_flags & O_NONBLOCK) //判断
	{
		//非阻塞
		 if (down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		//获取信号量 
		down(&button_lock); //第一次打开能获取信号，
	}                       //第二次无法获得信号量，而在这里处于无法被中断地休眠，处于一种僵死的状态
						    //只有当第一个应用程序释放信号量时，才能被唤醒
	
	//配置GPG0.3.5.6.7.11为中断引脚
	request_irq(IRQ_EINT8 , buttons_irq, IRQ_TYPE_EDGE_BOTH, "K1", &pin_desc[0]);
	request_irq(IRQ_EINT11, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K2", &pin_desc[1]);
	request_irq(IRQ_EINT13, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K3", &pin_desc[2]);
	request_irq(IRQ_EINT14, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K4", &pin_desc[3]);
	request_irq(IRQ_EINT15, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K5", &pin_desc[4]);
	request_irq(IRQ_EINT19, buttons_irq, IRQ_TYPE_EDGE_BOTH, "K6", &pin_desc[5]);

	return 0;	      
		
}          

static int sixth_drv_close(struct inode *inode, struct file *file)
{
#if 0
	atomic_inc(&canopen);  //关闭设备时，把canopen置1
#endif	

	up(&button_lock);  //释放信号

	free_irq(IRQ_EINT8 , &pin_desc[0]);  //释放中断
	free_irq(IRQ_EINT11, &pin_desc[1]); 
	free_irq(IRQ_EINT13, &pin_desc[2]);
	free_irq(IRQ_EINT14, &pin_desc[3]);
	free_irq(IRQ_EINT15, &pin_desc[4]);
	free_irq(IRQ_EINT19, &pin_desc[5]);
	
	return 0;
}                        

static ssize_t sixth_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (size != 1) 
		return -EINVAL;

	if (file->f_flags & O_NONBLOCK)
	{
		//非阻塞方式
		if (!ev_press)       
			return -EAGAIN;      //如果没有按键按下，则让它再进行一次
	}
	else
	{
		//阻塞方式
		//如果没有按键动作，休眠
		wait_event_interruptible(button_waitq, ev_press);  //把进程挂在队列当中,此时ev_press为0
	}
	
	//有按键动作，返回键值
	if (copy_to_user(buf, &g_save_key_val, 1))
		return -EFAULT;

	ev_press = 0;  //清零
	
	return 1;
}



static unsigned int sixth_drv_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &button_waitq, wait);  //把进程挂到队列里面去，但不会立刻休眠

	if (ev_press)
		mask |= POLLIN | POLLRDNORM;  

	return mask;   //如果返回为0，那么count++不会执行，那么就会进入休眠
}


static int  sixth_drv_fasync(int fd, struct file *filp, int on)
{
	printk("driver:  sixth_drv_fasync\n");  //用于调试，看会不会执行这一步

	return fasync_helper(fd, filp, on, &button_async); //初始化button_async
}


static struct file_operations  sixth_drv_fops = //告诉内核有这个驱动程序
{
	.owner   = THIS_MODULE, //这是一个宏，指向编译模块时，自动创建的_this_modulesy变量
	.open    =  sixth_drv_open,
	.read    =  sixth_drv_read,
	.release =  sixth_drv_close,
	.poll    =  sixth_drv_poll,
	.fasync  =  sixth_drv_fasync,
};


static void buttons_timerout_function(unsigned long data)
{
	//printk("irq = %d\n", irq); //打印出中断号

	struct pin_desc *pindesc = irq_pd;
	unsigned int pinval;

	if (!pindesc)  //如果没有按键按下，让它返回
		return ;
	 
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
}

int major;

static int sixth_drv_init(void)  
{
	init_timer(&buttons_timer);
	//buttons_timer.expires = jiffies + msecs_to_jiffies(CMD_TIMEOUT);   //我们在中断才设置超时时间
	//buttons_timer.data = (unsigned long) urb;                          //所以timer.expires  = 0;  	
	buttons_timer.function = buttons_timerout_function;    // 当超时时间到了之后，这个函数就会被调用
	add_timer(&buttons_timer);  //把定时器告诉内核, 启动定时器
		
	major = register_chrdev(0," sixth_drv", &sixth_drv_fops);  //注册，告诉内核，挂载驱动程序

	sixthdrv_class = class_create(THIS_MODULE, " sixthdrv");	
	sixthdrv_class_dev = device_create(sixthdrv_class, NULL, MKDEV(major,0), NULL, "buttons");

	return 0;
}

static void  sixth_drv_exit(void) 
{
	unregister_chrdev(major, " sixth_drv");  //注册，告诉内核，挂载驱动程序

	device_destroy(sixthdrv_class, MKDEV(major,0));    //卸载设备
	class_destroy(sixthdrv_class);                     //再卸载类
	
}

module_init(sixth_drv_init);
module_exit(sixth_drv_exit);

MODULE_LICENSE("GPL");


