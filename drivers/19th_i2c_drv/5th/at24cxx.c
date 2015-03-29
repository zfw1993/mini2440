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
							           /* 改为0x60的话，由于不存在设备地址为0x60的设备，所以at24cxx_detect不会被调用  */

static unsigned short force_addr[] = { ANY_I2C_BUS, 0x60, I2C_CLIENT_END}; /* 强制性的认为存在该设备地址(实际上不存在) */
static unsigned short * forces[] = {force_addr, NULL};

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_addr, /* 要发出S信号和设备地址并得到ACK信号，才能确定存在这个设备 */
	.probe = ignore,
	.ignore = ignore,
	//.force = forces, /* 强制认为存在这个设备 */
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

	/* 构造消息 */
	/* 数据传输三要素: 源, 目的, 长度 */
	/* 读AT24CXX时,要先把要读的存储空间的地址发给它 */
	msgs[0].addr = at24cxx_client->addr;  /* 目的 */
	msgs[0].buf  = &addr;                 /* 源 */
	msgs[0].len  = 1;                     /* 长度 (地址=1 byte) */
	msgs[0].flags = 0;                    /* 0表示写 */ 

	/* 然后启动读操作 */
	msgs[1].addr = at24cxx_client->addr;  /* 目的 */
	msgs[1].buf  = &data;                 /* 源 */
	msgs[1].len  = 1;                     /* 长度 (数据=1 byte) */
	msgs[1].flags = I2C_M_RD;             /* 表示读 */ 

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

	/* 构造消息 */
	/* 数据传输三要素: 源, 目的, 长度 */
	msgs[0].addr = at24cxx_client->addr;  /* 目的 */
	msgs[0].buf  = val;                   /* 源 */
	msgs[0].len  = 2;                     /* 长度 (地址+数据=2 byte) */
	msgs[0].flags = 0;                    /* 0表示写 */ 

	
	
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

	at24cxx_client  = kzalloc(sizeof(struct i2c_client), GFP_KERNEL); /* 有个问题: 如何free呢 */
	at24cxx_client->addr = address; /*  设备地址 */
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
	printk("at24cxx_detach\n"); /* detach: 分离 */
	class_device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(major, "at24cxx");
		
	i2c_detach_client(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/* 1. 分配一个i2c_driver结构体 */
/* 2. 设置i2c_driver结构体 */
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

