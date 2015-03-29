
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


//static struct s3c_nand_regs    /* �ṹ�岻�ü�static,  ֻ�Ƕ�����һ������ */
struct s3c_nand_regs
{
	unsigned long	nfconf;               
	unsigned long	nfcont;               
	unsigned long	nfcmd;                
	unsigned long	nfaddr;               
	unsigned long	nfdata;               
	unsigned long	nfmeccd0;
	unsigned long 	nfmeccd1;
	unsigned long 	nfseccd;
	unsigned long	nfstat;
	unsigned long	nfestat0;
	unsigned long	nfestat1;
	unsigned long	nfmecc0;
	unsigned long	nfmecc1;
	unsigned long	nfsecc;
	unsigned long	nfsblk;
	unsigned long	nfeblk;
};

static struct nand_chip *st_p_nand_chip;
static struct mtd_info *st_p_mtd;
static struct s3c_nand_regs *st_p_s3c_nand_regs;

static void s3c2440_select_chip(struct mtd_info *mtd, int chipnr)
{
	if (chipnr == -1)
	{
		/* ȡ��ѡ��: NFCONT �� bit[1]����Ϊ1 */
		st_p_s3c_nand_regs->nfcont |= (1<<1);
		
	}
	else
	{
		/* ѡ��: NFCONT �� bit[1]����Ϊ0 */
		st_p_s3c_nand_regs->nfcont &= ~(1<<1); 
	}
}

static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl & NAND_CLE)
	{
		/* �����NFCMMD = dat */
		st_p_s3c_nand_regs->nfcmd = dat;
	}
	else
	{
		/* ����ַ��NFADDR = dat */
		st_p_s3c_nand_regs->nfaddr = dat;
	}
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
//	return "NFSTAT��bit[0]"
	return (st_p_s3c_nand_regs->nfstat & (1<<0));
}


static int s3c_nand_init(void)
{
	 struct clk *p_clk;
	
	/* 1. ����һ��nand_chip�ṹ�� */
	st_p_nand_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);

	st_p_s3c_nand_regs = ioremap(0x4E000000, sizeof(struct s3c_nand_regs));
	
	/* 2. ����nand_chip           
	 * ����nand_chip�Ǹ�nand_scan_identʹ�õģ������֪�����ʹ�ã��ȿ�nand_scan_ident��ôʹ�� 
	 * ��Ӧ���ṩ: ѡ�У����������ַ�������ݣ������ݣ��ж�״̬�Ĺ��� 
	 */
	st_p_nand_chip->select_chip = s3c2440_select_chip;
	st_p_nand_chip->cmd_ctrl    = s3c2440_cmd_ctrl;
	st_p_nand_chip->IO_ADDR_R   = &st_p_s3c_nand_regs->nfdata; //"NFDATA �������ַ";
	st_p_nand_chip->IO_ADDR_W   = &st_p_s3c_nand_regs->nfdata; //"NFDATA �������ַ";
	st_p_nand_chip->dev_ready   = s3c2440_dev_ready; 
    st_p_nand_chip->ecc.mode    = NAND_ECC_SOFT;   /* �������ECC�� */
	/* 3. Ӳ����ص�����: ����nand flash���ֲ�����ʱ����� */
	p_clk = clk_get(NULL, "nand"); 
	clk_enable(p_clk);     /* ʵ���Ͼ�������CLKCON�Ĵ�����bit[4]Ϊ1  ���ܿ��� ��������ļĴ����޷����� */
	
	/* HCLK = 100MHz
	 * TACLS : ����CLE/ALE֮��೤ʱ��ŷ���nWE�źţ���nand�ֲ��֪CLE/ALE��nWE����ͬʱ����������TACLS = 0
	 * TWRPH0: nWE�������ȣ�HCLK*(TWRPH0+1), ��nand�ֲ��֪��Ҫ>=12ns������TWRPH0 >= 1
	 * TWRPH0: nWE��Ϊ�ߵ�ƽ��೤ʱ��CLE/ALE���ܱ�Ϊ�͵�ƽ����nand �ֲ��֪��Ҫ>5ns������TWRPH1 >= 0
	 */
#define TACLS   0
#define TWRPH0  1
#define TWRPH1  0

	st_p_s3c_nand_regs->nfconf = (TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4);

	/* NFCONT
	 * bit1 - ��Ϊ1��ȡ��Ƭѡ
	 * bit0 = ��Ϊ1��ʹ��nand flash������
	 */
	st_p_s3c_nand_regs->nfcont = (1<<1) | (1<<0);
	
	/* 4. ʹ��: nand_scan         */
	st_p_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	st_p_mtd->owner = THIS_MODULE;
	st_p_mtd->priv  = st_p_nand_chip;  /* ��nand_chip�ṹ����mtd�ṹ��ҹ����� */

	nand_scan_ident(st_p_mtd, 1, NULL);  /* ʶ��nand flash, ����mtd_info */
	
	/* 5. add_mtd_partitions      */  /* ���ϳ�����ʶ���nand flash�� */
	                                  /* ��add_mtd_partitons��֪ͨ�ַ��豸����豸���ֱ�����Ӧ�Ĳ����� */
	
	
	return 0; 
}

static void s3c_nand_exit(void)
{
	kfree(st_p_mtd);
	iounmap(st_p_s3c_nand_regs);
	kfree(st_p_nand_chip);
	
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");

