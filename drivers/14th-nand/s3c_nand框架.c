
/* �ο�
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
		/* ȡ��ѡ��: NFCONT �� bit[1]����Ϊ0 */
		
	}
	else
	{
		/* ѡ��: NFCONT �� bit[1]����Ϊ1 */
	}
}

static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl);
{
	if (ctrl & NAND_CLE)
	{
		/* �����NFCMMD = dat */
	}
	else
	{
		/* ����ַ��NFADDR = dat */
	}
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	return "NFSTAT��bit[0]"
}


static int s3c_nand_init(void)
{
	/* 1. ����һ��nand_chip�ṹ�� */
	st_p_nand_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	
	/* 2. ����nand_chip           
	 * ����nand_chip�Ǹ�nand_scan_identʹ�õģ������֪�����ʹ�ã��ȿ�nand_scan_ident��ôʹ�� 
	 * ��Ӧ���ṩ: ѡ�У����������ַ�������ݣ������ݣ��ж�״̬�Ĺ��� 
	 */
	st_p_nand_chip->select_chip = s3c2440_select_chip;
	st_p_nand_chip->cmd_ctrl    = s3c2440_cmd_ctrl;
	st_p_nand_chip->IO_ADDR_R   = "NFDATA �������ַ";
	st_p_nand_chip->IO_ADDR_W   = "NFDATA �������ַ";
	st_p_nand_chip->dev_ready   = s3c2440_dev_ready; 
	
	/* 3. Ӳ����ص�����          */
	/* 4. ʹ��: nand_scan         */
	st_p_mtd = kzalloc(sizeof(struct st_p_mtd), GFP_KERNEL);
	st_p_mtd->owner = THIS_MODULE;
	st_p_mtd->priv  = st_p_nand_chip;  /* ��nand_chip�ṹ����mtd�ṹ��ҹ����� */

	nand_scan_ident(st_p_mtd, 1, NULL);  /* ʶ��nand flash, ����mtd_info */
	
	/* 5. add_mtd_partitions      */
	
	return 0;
}

static void s3c_nand_exit(void)
{
	
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");

