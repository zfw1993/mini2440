#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>



static int __devinit at24cxx_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static int __devexit at24cxx_remove(struct i2c_client *client)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}


static const struct i2c_device_id at24cxx_id_table[] = {
	{ "at24cxx", 0 },
	{}
};


static int at24cxx_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	/* Ӧ��ȥ����Ӳ��ȷ���豸ȷʵ���� */

	/* �����е������ʾ��addr���豸�Ǵ��ڵ�
	 * ������Щ�豸��ƾ��ַ�޷��ֱ�
	 * ����: A��BоƬ�ĵ�ַ����0x50
	 * ����Ҫ��һ����дI2C�豸���ֱ����Ŀ�оƬ
	 * detect����������һ���ֱ����оƬ����һ���������info->type
	 */
	
	printk("at24cxx_detect : addr = 0x%x\n", client->addr);

	/* ��һ���ж�����һ��оƬ */

	strlcpy(info->type, "at24cxx", I2C_NAME_SIZE);
	return 0;
}

static const unsigned short address_list[] = { 0x60, 0x50, I2C_CLIENT_END }; /* 0x60ֻ������ʾ, ʵ�ʲ���������豸 */

/* 1. ����/����i2c_driver�ṹ�� */
static struct i2c_driver at24cxx_driver = {
	.class		= I2C_CLASS_HWMON,  /* ��ʾȥ�����������ϲ����豸 */
	.driver = {
		.name = "mini2440",  /* ������ֲ���Ҫ */
		.owner = THIS_MODULE,
	},
	.probe = at24cxx_probe,
	.remove = __devexit_p(at24cxx_remove),
	.id_table = at24cxx_id_table, 
	.detect		= at24cxx_detect,      /* ���������������ܷ��ҵ��豸 */
	.address_list	= address_list, /* ��Щ�豸�ĵ�ַ */
};


static int at24cxx_drv_init(void)
{
	/* 2. ע��i2c_driver */
	i2c_add_driver(&at24cxx_driver);
	return 0;
}

static void at24cxx_drv_exit(void)
{
	i2c_del_driver(&at24cxx_driver);
}

module_init(at24cxx_drv_init);
module_exit(at24cxx_drv_exit);

MODULE_LICENSE("GPL");
