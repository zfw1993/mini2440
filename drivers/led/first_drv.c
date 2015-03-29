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


static struct class *firstdrv_class;       /* ����һ���� */ 
static struct device *firstdrv_class_dev;  /* �������ٴ���һ���豸 */

volatile unsigned long *gpbcon = NULL; 
volatile unsigned long *gpbdat = NULL;


static int first_drv_open(struct inode *inode,struct file *file)
{
	//int minor = MINOR(inode->i_rdev);	//get minor devices number
	
	//printk("first_drv_open\n");
	/* ����GPB5,6,7,8Ϊ��� */
	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));  //����
	*gpbcon |=  ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));  //����Ϊ���
	return 0;
}

//���������Ӧ���û������ write(fd, &val, 4); ���һ�ϲ������ù�    &val <=> buf
static ssize_t first_drv_write(struct file *file,const char __user *buf,size_t count,loff_t *ppos)
{
	int des_val;  /*Ŀ�ص�ַ*/

	//int minor = MINOR(filp->f_dentry->d_inode->i_rdev);  //please to read the "myleds.c" 'file 
	
	//printk("first_drv_wirte\n");

	//copy_from_user(void * to,const void __user * from,unsigned long n)
	//������ʽ : Ŀ�ص�ַ  Դ��ַ  ����  

	if (copy_from_user(&des_val, buf, count)) //���û��ռ䴫������ֵ������des_val //���ں˴�������
			return -EFAULT; /* �����ں˿������ݵ��û��ռ��õĺ�����: copy_to_user(); */

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

static struct file_operations first_drv_fops = //�����ں��������������
{
	.owner = THIS_MODULE, //����һ���ָ꣬�����ģ��ʱ���Զ�������_this_modulesy����
	.open  = first_drv_open,
	.write = first_drv_write,
};

int major; //��ϵͳΪ���Ƿ������豸��
static int first_drv_init(void)  //��ں���
{
	major = register_chrdev(0, "first_drv", &first_drv_fops); /* ע�ᣬ�����ںˣ������������� */
	/* ���豸�ţ����֣��ṹ�� */

	/* ����ϵͳ��Ϣ����ϵͳĿ¼(/sys/)�½���firstdvc����࣬������ᱻ����һ����Ϊzfw����豸 */
	firstdrv_class = class_create(THIS_MODULE,"firstdrv");  /* Ȼ��mdev���Զ�����һ��/dev/zfw ���豸�ڵ� */
	/* �Ƚ���һ����  */                                                        

	firstdrv_class_dev = device_create(firstdrv_class, NULL, MKDEV(major,0), NULL, "zfw");
	/* ���������潨��һ���豸 */                  

	/***********************����ӳ���ϵ**********************************/

	/* ioremap(��ʼ��ַ, ����) */
	gpbcon = (volatile unsigned long *)ioremap(0x56000010, 16); //16���ֽ�
	/* ����GPB�ļĴ�����4����4*4=16��*/
	gpbdat = gpbcon + 1;  /*ָ��+1 : ��ʾ���ݼ�4���ֽ� = 0x56000014*/
	
	return 0;          
}

static void first_drv_exit(void)  /* ��ں��� */
{
	unregister_chrdev(major, "first_drv"); /* ж���������� */
	//���豸�ţ�����
	device_destroy(firstdrv_class, MKDEV(major,0));  //ж���豸
	class_destroy(firstdrv_class);                //��ж����

	/************************ȥ��ӳ���ϵ**********************************/

	iounmap(gpbcon);  //�����ַ
}


module_init(first_drv_init); //������ں����������ں��������ں���
module_exit(first_drv_exit); //���γ��ں���


MODULE_LICENSE("GPL");


