
//参考 drivers\input\keyboard\gpio_keys.c


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>

struct pin_desc
{
	int          irq;
	unsigned int pin;
	char        *name;	
	unsigned int key_val;
};


struct pin_desc pin_desc[6] =  //结构体数组
{
	{IRQ_EINT8,  S3C2410_GPG(0), "K1", KEY_L},
	{IRQ_EINT11, S3C2410_GPG(3), "K2", KEY_S},
	{IRQ_EINT13, S3C2410_GPG(5), "K3", KEY_ENTER},
	{IRQ_EINT14, S3C2410_GPG(6), "K4", KEY_LEFTSHIFT},
	{IRQ_EINT15, S3C2410_GPG(7), "K5", KEY_P},
	{IRQ_EINT19, S3C2410_GPG(11),"K6", KEY_S},
};


static struct input_dev *buttons_dev;
static struct pin_desc *irq_pd;
static struct timer_list buttons_timer;
	

static irqreturn_t buttons_irq(int irq, void *dev_id)  //中断服务函数
{
	irq_pd = (struct pin_desc *)dev_id;  //用一个全局变量来保存dev_id
	
	// 10ms 后启动定时器
	mod_timer(&buttons_timer, jiffies+HZ/100);    // modified timer 修改定时器时间
	
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void buttons_timerout_function(unsigned long data)
{
	struct pin_desc *pindesc = irq_pd;
	unsigned int pinval;
	
	if (!pindesc)
		return;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		// 松开:  最后一个参数: 0-松开  1-按下
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 0);
		input_sync(buttons_dev);  //表示事件已经上报完毕
	}
	else
	{
		// 按下
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 1);
		input_sync(buttons_dev);
	}
}


static int input_sub_init(void)
{
	int i;
	
	// 1. 分配一个input_dev结构体
	buttons_dev = input_allocate_device();
	if (!buttons_dev)
		return -EAGAIN;

	// 2.  设置
	// 2.1 能产生哪类操作
	set_bit(EV_KEY, buttons_dev->evbit);
	set_bit(EV_REP, buttons_dev->evbit);  //产生重复类操作
 	
	// 2.2 能产生这类操作里的哪些事件: L,S.ENTER,LEFTSHIFT
	set_bit(KEY_L, buttons_dev->keybit);
	set_bit(KEY_S, buttons_dev->keybit);
	set_bit(KEY_ENTER, buttons_dev->keybit);
	set_bit(KEY_LEFTSHIFT, buttons_dev->keybit);   //大写	
	set_bit(KEY_P, buttons_dev->keybit);
	set_bit(KEY_S, buttons_dev->keybit);

	// 3. 注册
	input_register_device(buttons_dev);
	
	// 4. 硬件相关的操作
	init_timer(&buttons_timer);
	buttons_timer.function = buttons_timerout_function;
	add_timer(&buttons_timer); //把定时器告诉内核
	
	for (i=0; i<6; i++)      
		request_irq(pin_desc[i].irq, buttons_irq, IRQ_TYPE_EDGE_BOTH, pin_desc[i].name, &pin_desc[i]);
		
	return 0;
}

static void input_sub_exit(void)
{
	int i;

	for (i=0; i<6; i++)
	{
		free_irq(pin_desc[i].irq, &pin_desc[i]);
	}
	del_timer(&buttons_timer);
	input_unregister_device(buttons_dev);
	input_free_device(buttons_dev);
}


module_init(input_sub_init);
module_exit(input_sub_exit);

MODULE_LICENSE("GPL");
