
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


//static struct s3c_nand_regs    /* 结构体不用加static,  只是定义了一个类型 */
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
		/* 取消选中: NFCONT 的 bit[1]设置为1 */
		st_p_s3c_nand_regs->nfcont |= (1<<1);
		
	}
	else
	{
		/* 选中: NFCONT 的 bit[1]设置为0 */
		st_p_s3c_nand_regs->nfcont &= ~(1<<1); 
	}
}

static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl & NAND_CLE)
	{
		/* 发命令，NFCMMD = dat */
		st_p_s3c_nand_regs->nfcmd = dat;
	}
	else
	{
		/* 发地址，NFADDR = dat */
		st_p_s3c_nand_regs->nfaddr = dat;
	}
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
//	return "NFSTAT的bit[0]"
	return (st_p_s3c_nand_regs->nfstat & (1<<0));
}


static int s3c_nand_init(void)
{
	 struct clk *p_clk;
	
	/* 1. 分配一个nand_chip结构体 */
	st_p_nand_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);

	st_p_s3c_nand_regs = ioremap(0x4E000000, sizeof(struct s3c_nand_regs));
	
	/* 2. 设置nand_chip           
	 * 设置nand_chip是给nand_scan_ident使用的，如果不知道如何使用，先看nand_scan_ident怎么使用 
	 * 它应该提供: 选中，发命令，发地址，发数据，读数据，判断状态的功能 
	 */
	st_p_nand_chip->select_chip = s3c2440_select_chip;
	st_p_nand_chip->cmd_ctrl    = s3c2440_cmd_ctrl;
	st_p_nand_chip->IO_ADDR_R   = &st_p_s3c_nand_regs->nfdata; //"NFDATA 的虚拟地址";
	st_p_nand_chip->IO_ADDR_W   = &st_p_s3c_nand_regs->nfdata; //"NFDATA 的虚拟地址";
	st_p_nand_chip->dev_ready   = s3c2440_dev_ready; 
    st_p_nand_chip->ecc.mode    = NAND_ECC_SOFT;   /* 软件生成ECC码 */
	/* 3. 硬件相关的设置: 根据nand flash的手册设置时间参数 */
	p_clk = clk_get(NULL, "nand"); 
	clk_enable(p_clk);     /* 实际上就是设置CLKCON寄存器的bit[4]为1  开总开关 否则下面的寄存器无法操作 */
	
	/* HCLK = 100MHz
	 * TACLS : 发出CLE/ALE之后多长时间才发出nWE信号，从nand手册可知CLE/ALE与nWE可以同时发出，所以TACLS = 0
	 * TWRPH0: nWE的脉冲宽度，HCLK*(TWRPH0+1), 从nand手册可知它要>=12ns，所以TWRPH0 >= 1
	 * TWRPH0: nWE变为高电平后多长时间CLE/ALE才能变为低电平，从nand 手册可知它要>5ns，所以TWRPH1 >= 0
	 */
#define TACLS   0
#define TWRPH0  1
#define TWRPH1  0

	st_p_s3c_nand_regs->nfconf = (TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4);

	/* NFCONT
	 * bit1 - 设为1，取消片选
	 * bit0 = 设为1，使能nand flash控制器
	 */
	st_p_s3c_nand_regs->nfcont = (1<<1) | (1<<0);
	
	/* 4. 使用: nand_scan         */
	st_p_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	st_p_mtd->owner = THIS_MODULE;
	st_p_mtd->priv  = st_p_nand_chip;  /* 将nand_chip结构体与mtd结构体挂钩起来 */

	nand_scan_ident(st_p_mtd, 1, NULL);  /* 识别nand flash, 构造mtd_info */
	
	/* 5. add_mtd_partitions      */  /* 以上程序是识别出nand flash的 */
	                                  /* 而add_mtd_partitons是通知字符设备或块设备，分别做相应的操作的 */
	
	
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

