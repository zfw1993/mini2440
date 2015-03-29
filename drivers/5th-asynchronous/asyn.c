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

/* �ж��¼���־, �жϷ����������1��fifth_drv_read������0 */
static volatile int ev_press = 0;

static struct fasync_struct *button_async;

static struct class *fifthdrv_class;                 //����һ����
static struct device *fifthdrv_class_dev;  //�������ٴ���һ���豸


static unsigned char g_save_key_val;  //�����ֵ

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

//�����ֵ�������Լ��趨�ģ����������趨
//  ��ֵ : ����ʱ��0x01,0x02,0x03,0x04,0x05,0x06
//  ��ֵ : �ɿ�ʱ��0x81,0x82,0x83,0x84,0x85,0x86

struct pin_desc pin_desc[6] =  //�ṹ������
{
	{S3C2410_GPG(0),  0x01},
	{S3C2410_GPG(3),  0x02},
	{S3C2410_GPG(5),  0x03},
	{S3C2410_GPG(6),  0x04},
	{S3C2410_GPG(7),  0x05},
	{S3C2410_GPG(11), 0x06},
};

/*
 * ȷ������ֵ
 */
static irqreturn_t buttons_irq(int irq, void *dev_id)  //�жϷ�����
{
	//printk("irq = %d\n", irq); //��ӡ���жϺ�

	struct pin_desc *pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	 
	pinval = s3c2410_gpio_getpin(pindesc->pin);  // ������״̬

//	printk("pinval = %d\n", pinval);

	if (pinval) // ��������ôд if (1 == pinval) ��Ϊpinval��ֵΪ0,8,32,128,64,2048
	{
		//�ɿ�
		g_save_key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//����
		g_save_key_val = pindesc->key_val;
	}

	ev_press = 1;  //��1
	wake_up_interruptible(&button_waitq);  //�������ߵĽ��̣��ѹ��ڶ��еĽ��̻���
	
	kill_fasync(&button_async, SIGIO, POLL_IN); //�����źţ����͵Ķ���϶���button_async����ṹ�嵱��
	
	return IRQ_RETVAL(IRQ_HANDLED);
}


static int fifth_drv_open(struct inode *inode,struct file *file) //���豸�󣬲Ż����request_irq
{
	//����GPG0.3.5.6.7.11Ϊ�ж�����
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
	free_irq(IRQ_EINT8 , &pin_desc[0]);  //�ͷ��ж�
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

	//���û�а�������������
	wait_event_interruptible(button_waitq, ev_press);  //�ѽ��̹��ڶ��е���,��ʱev_pressΪ0
	
	//�а������������ؼ�ֵ
	if (copy_to_user(buf, &g_save_key_val, 1))
		return -EFAULT;

	ev_press = 0;  //����
	
	return 1;
}

static unsigned int fifth_drv_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &button_waitq, wait);  //�ѽ��̹ҵ���������ȥ����������������

	if (ev_press)
		mask |= POLLIN | POLLRDNORM;  

	return mask;   //�������Ϊ0����ôcount++����ִ�У���ô�ͻ��������
}


static int fifth_drv_fasync(int fd, struct file *filp, int on)
{
	printk("driver: fifth_drv_fasync\n");  //���ڵ��ԣ����᲻��ִ����һ��
	
	return fasync_helper(fd, filp, on, &button_async); //��ʼ��button_async
}


static struct file_operations fifth_drv_fops = //�����ں��������������
{
	.owner   = THIS_MODULE, //����һ���ָ꣬�����ģ��ʱ���Զ�������_this_modulesy����
	.open    = fifth_drv_open,
	.read    = fifth_drv_read,
	.release = fifth_drv_close,
	.poll    = fifth_drv_poll,
	.fasync  = fifth_drv_fasync,
};

int major;
static int fifth_drv_init(void)  //entry 
{
	major = register_chrdev(0,"fifth_drv", &fifth_drv_fops);  //ע�ᣬ�����ںˣ�������������

	fifthdrv_class = class_create(THIS_MODULE,"fifthdrv");	
	fifthdrv_class_dev = device_create(fifthdrv_class, NULL, MKDEV(major,0), NULL, "buttons");

	return 0;
}

static void fifth_drv_exit(void) 
{
	unregister_chrdev(major,"fifth_drv");  //ע�ᣬ�����ںˣ�������������

	device_destroy(fifthdrv_class, MKDEV(major,0));  //ж���豸
	class_destroy(fifthdrv_class);                   //��ж����
}

module_init(fifth_drv_init);
module_exit(fifth_drv_exit);

MODULE_LICENSE("GPL");


