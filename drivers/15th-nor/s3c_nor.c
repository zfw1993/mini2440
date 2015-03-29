
/* 参考: 
 *	drivers\mtd\maps\Physmap.c
 */
	 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/concat.h>
#include <linux/io.h>


static struct map_info *st_p_s3c_nor_map;
static struct mtd_info *st_p_s3c_nor_mtd;

static struct mtd_partition s3c_nor_parts[] = {
	[0] = {
		.name	= "bootloader_nor",
		.size	= SZ_256K,
		.offset	= 0,
	},
	[1] = {
		.name	= "root_nor",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	}
};

static int s3c_nor_init(void)
{
	/* 1. 分配map_info结构体 */
	//st_p_s3c_nor_map = devm_kzalloc(NULL, sizeof(struct map_info), GFP_KERNEL); /* 有问题 */
	st_p_s3c_nor_map = kzalloc(sizeof(struct map_info), GFP_KERNEL);
	
	/* 2. 设置物理基地址(phys), 大小(size), 位宽(bankwidth), 虚拟基地址(virt) */
	st_p_s3c_nor_map->name = "s3c_nor";
	st_p_s3c_nor_map->phys = 0;         /* nor启动时，起始地址为0 */
	st_p_s3c_nor_map->size = 0x1000000; /* 随便写，只要大于2M即可 */
 	st_p_s3c_nor_map->bankwidth = 2;    /* 2*8 = 16位 */
	//st_p_s3c_nor_map->virt = devm_ioremap(NULL, st_p_s3c_nor_map->phys, st_p_s3c_nor_map->size);
	st_p_s3c_nor_map->virt = ioremap(st_p_s3c_nor_map->phys, st_p_s3c_nor_map->size);
	
	simple_map_init(st_p_s3c_nor_map);
	
	/* 3. 使用: 调用nor flash协议层提供的函数来识别 */
	printk("use cfi_probe\n");
	st_p_s3c_nor_mtd = do_map_probe("cfi_probe", st_p_s3c_nor_map);
	if (!st_p_s3c_nor_mtd)  /* 如果为空 */
	{
		printk("use jedec_probe\n");
		st_p_s3c_nor_mtd = do_map_probe("jedec_probe", st_p_s3c_nor_map);
	}

	if (!st_p_s3c_nor_mtd)
	{
//		devm_iounmap(NULL, st_p_s3c_nor_map->virt);
	//	devm_kfree(NULL, st_p_s3c_nor_map);
		iounmap(st_p_s3c_nor_map->virt);
		kfree(st_p_s3c_nor_map);
		return -EIO;
	}
	
	/* 4. add_mtd_partitions */
	mtd_device_parse_register(st_p_s3c_nor_mtd, NULL, NULL, s3c_nor_parts, 2); /* 分区 */
	
	return 0;
}

static void s3c_nor_exit(void)
{
	//devm_iounmap(NULL, st_p_s3c_nor_map->virt);
	//devm_kfree(NULL, st_p_s3c_nor_map);
	mtd_device_unregister(st_p_s3c_nor_mtd);
	iounmap(st_p_s3c_nor_map->virt);
	kfree(st_p_s3c_nor_map);
}



module_init(s3c_nor_init);
module_exit(s3c_nor_exit);

MODULE_LICENSE("GPL");

