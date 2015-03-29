
/* 参考
 * drivers\mtd\nand\S3c2410.c
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
 
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
 
#include <asm/io.h>
 
#include <plat/regs-nand.h>
#include <plat/nand.h>

static struct nand_chip *st_p_nand_chip;
static struct mtd_info *st_p_mtd;



static void s3c2440_select_chip(struct mtd_info *mtd, int chipnr)
{
	if (chipnr == -1)
	{
		/* 取消选中: NFCONT 的 bit[1]设置为0 */
		
	}
	else
	{
		/* 选中: NFCONT 的 bit[1]设置为1 */
	}
}

static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl);
{
	if (ctrl & NAND_CLE)
	{
		/* 发命令，NFCMMD = dat */
	}
	else
	{
		/* 发地址，NFADDR = dat */
	}
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	return "NFSTAT的bit[0]"
}


static int s3c_nand_init(void)
{
	/* 1. 分配一个nand_chip结构体 */
	st_p_nand_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	
	/* 2. 设置nand_chip           
	 * 设置nand_chip是给nand_scan_ident使用的，如果不知道如何使用，先看nand_scan_ident怎么使用 
	 * 它应该提供: 选中，发命令，发地址，发数据，读数据，判断状态的功能 
	 */
	st_p_nand_chip->select_chip = s3c2440_select_chip;
	st_p_nand_chip->cmd_ctrl    = s3c2440_cmd_ctrl;
	st_p_nand_chip->IO_ADDR_R   = "NFDATA 的虚拟地址";
	st_p_nand_chip->IO_ADDR_W   = "NFDATA 的虚拟地址";
	st_p_nand_chip->dev_ready   = s3c2440_dev_ready; 
	
	/* 3. 硬件相关的设置          */
	/* 4. 使用: nand_scan         */
	st_p_mtd = kzalloc(sizeof(struct st_p_mtd), GFP_KERNEL);
	st_p_mtd->owner = THIS_MODULE;
	st_p_mtd->priv  = st_p_nand_chip;  /* 将nand_chip结构体与mtd结构体挂钩起来 */

	nand_scan_ident(st_p_mtd, 1, NULL);  /* 识别nand flash, 构造mtd_info */
	
	/* 5. add_mtd_partitions      */
	
	return 0;
}

static void s3c_nand_exit(void)
{
	
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");

