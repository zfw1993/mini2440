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

static struct class *seconddrv_class;       //����һ����
static struct device *seconddrv_class_dev;  //�������ٴ���һ���豸


ssize_t second_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	/* ����6�����ŵĵ�ƽ */
	unsigned char key_vals[6];
	int regval; // ��arm�int �� long��һ����

	if (size != sizeof(key_vals))
		return -EINVAL;    

	//��GPG����	
	regval = *gpgdat;  //�ǵ���*gpgdat

	key_vals[0] = (regval & (1<<0)) ? 1 : 0;	
	key_vals[1] = (regval & (1<<3)) ? 1 : 0;
	key_vals[2] = (regval & (1<<5)) ? 1 : 0;
	key_vals[3] = (regval & (1<<6)) ? 1 : 0;
	key_vals[4] = (regval & (1<<7)) ? 1 : 0;
	key_vals[5] = (regval & (1<<11)) ? 1 : 0;

	//��ֵ���ظ��û�����
	if (copy_to_user(buf, key_vals, sizeof(key_vals)))
			return -EFAULT;
	
	return sizeof(key_vals);
}


static int second_drv_open(struct inode *inode,struct file *file)
{
	//����GPG0.3.5.6.7.11Ϊ��������
	*gpgcon &= ~( (0x3<<(0*2)) | (0x3<<(3*2)) | (0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(11*2)) );  //����
	*gpgcon |=  ( (0x0<<(0*2)) | (0x0<<(3*2)) | (0x0<<(5*2)) | (0x0<<(6*2)) | (0x0<<(7*2)) | (0x0<<(11*2)) );  //����Ϊ����
	return 0;	
}

static struct file_operations second_drv_fops = //�����ں��������������
{
	.owner = THIS_MODULE, //����һ���ָ꣬�����ģ��ʱ���Զ�������_this_modulesy����
	.open  = second_drv_open,
	.read  = second_drv_read,
};

int major;
static int second_drv_init(void)  //entry 
{
	major = register_chrdev(0,"second_drv", &second_drv_fops);  //ע�ᣬ�����ںˣ�������������

	seconddrv_class = class_create(THIS_MODULE, "seconddrv");	
	seconddrv_class_dev = device_create(seconddrv_class, NULL, MKDEV(major,0), NULL, "buttons");
		
	gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16); //16���ֽ�
	gpgdat = gpgcon + 1;  /*ָ��+1 : ��ʾ���ݼ�4���ֽ� = 0x56000014*/

	return 0;
}

static void second_drv_exit(void) 
{
	unregister_chrdev(major,"second_drv");  //ע�ᣬ�����ںˣ�������������
	//���豸�ţ����֣��ṹ��
	device_destroy(seconddrv_class, MKDEV(major,0));         //ж���豸
	class_destroy(seconddrv_class);                //��ж����

	iounmap(gpgcon);  //�����ַ
}

module_init(second_drv_init);
module_exit(second_drv_exit);

MODULE_LICENSE("GPL");


