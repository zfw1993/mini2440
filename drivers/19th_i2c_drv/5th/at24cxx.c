#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <asm/uaccess.h>





static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x50, I2C_CLIENT_END };
							           /* ��Ϊ0x60�Ļ������ڲ������豸��ַΪ0x60���豸������at24cxx_detect���ᱻ����  */

static unsigned short force_addr[] = { ANY_I2C_BUS, 0x60, I2C_CLIENT_END}; /* ǿ���Ե���Ϊ���ڸ��豸��ַ(ʵ���ϲ�����) */
static unsigned short * forces[] = {force_addr, NULL};

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_addr, /* Ҫ����S�źź��豸��ַ���õ�ACK�źţ�����ȷ����������豸 */
	.probe = ignore,
	.ignore = ignore,
	//.force = forces, /* ǿ����Ϊ��������豸 */
};

static struct i2c_driver at24cxx_driver;
static struct class *cls;
struct i2c_client *at24cxx_client;


static int major;

static ssize_t at24cxx_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	unsigned char addr;
	unsigned char data;
	struct i2c_msg msgs[2];
	int iRet;

	if (size != 1)
		return -1;

	copy_from_user(&addr, buf, 1);

	/* ������Ϣ */
	/* ���ݴ�����Ҫ��: Դ, Ŀ��, ���� */
	/* ��AT24CXXʱ,Ҫ�Ȱ�Ҫ���Ĵ洢�ռ�ĵ�ַ������ */
	msgs[0].addr = at24cxx_client->addr;  /* Ŀ�� */
	msgs[0].buf  = &addr;                 /* Դ */
	msgs[0].len  = 1;                     /* ���� (��ַ=1 byte) */
	msgs[0].flags = 0;                    /* 0��ʾд */ 

	/* Ȼ������������ */
	msgs[1].addr = at24cxx_client->addr;  /* Ŀ�� */
	msgs[1].buf  = &data;                 /* Դ */
	msgs[1].len  = 1;                     /* ���� (����=1 byte) */
	msgs[1].flags = I2C_M_RD;             /* ��ʾ�� */ 

	iRet = i2c_transfer(at24cxx_client->adapter, msgs, 2);
	if (iRet == 2)
	{
		copy_to_user(buf, &data, 1);
		return 1;
	}
	else
		return -1;
}

static ssize_t at24cxx_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	unsigned char val[2];
	struct i2c_msg msgs[1]; 
	int iRet;

	/* addr = buf[0]
	 * data = buf[1]
	 */

	if (size != 2)
		return -1;
	
	
	copy_from_user(val, buf, 2);

	/* ������Ϣ */
	/* ���ݴ�����Ҫ��: Դ, Ŀ��, ���� */
	msgs[0].addr = at24cxx_client->addr;  /* Ŀ�� */
	msgs[0].buf  = val;                   /* Դ */
	msgs[0].len  = 2;                     /* ���� (��ַ+����=2 byte) */
	msgs[0].flags = 0;                    /* 0��ʾд */ 

	
	
	iRet = i2c_transfer(at24cxx_client->adapter, msgs, 1);
	if (iRet == 1)
		return 2;
	else
		return -1;
	
}

static struct file_operations at24cxx_fops = {
	.owner 	= THIS_MODULE,
	.read   = at24cxx_read,
	.write  = at24cxx_write,
};


static int at24cxx_detect(struct i2c_adapter *adapter, int address, int kind)
{
	
	printk("at24cxx_detect\n");

	at24cxx_client  = kzalloc(sizeof(struct i2c_client), GFP_KERNEL); /* �и�����: ���free�� */
	at24cxx_client->addr = address; /*  �豸��ַ */
	at24cxx_client->adapter = adapter;
	at24cxx_client->driver = &at24cxx_driver;
	strcpy(at24cxx_client->name, "at24cxx");
	
	i2c_attach_client(at24cxx_client);

	major = register_chrdev(0, "at24cxx", &at24cxx_fops);

	cls = class_create(THIS_MODULE, "at24cxx");
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "at24cxx");

	return 0;
}

static int at24cxx_attach(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, at24cxx_detect);
}

static int at24cxx_detach(struct i2c_client *client)
{
	printk("at24cxx_detach\n"); /* detach: ���� */
	class_device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(major, "at24cxx");
		
	i2c_detach_client(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/* 1. ����һ��i2c_driver�ṹ�� */
/* 2. ����i2c_driver�ṹ�� */
static struct i2c_driver at24cxx_driver = {
	.driver = {
		   .name = "at24cxx",
		   },
	.attach_adapter = at24cxx_attach,
	.detach_client = at24cxx_detach,
};

static int at24cxx_init(void)
{
	i2c_add_driver(&at24cxx_driver);
	return 0;
}

static void at24cxx_exit(void)
{
	i2c_del_driver(&at24cxx_driver);
}

module_init(at24cxx_init);
module_exit(at24cxx_exit);

MODULE_LICENSE("GPL");

