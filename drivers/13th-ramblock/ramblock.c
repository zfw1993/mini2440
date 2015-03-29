
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gfp.h>

#include <asm/uaccess.h>
#include <asm/dma.h>


static struct gendisk *st_p_gendisk;
static struct request_queue *st_p_request_queue;


static DEFINE_SPINLOCK(ramblock_lock);
static int major;

#define RAMBLOCK_SIZE 	(1024*1024)  
static unsigned char *p_ramblock_buf;

static int ramblock__getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/* 容量 = heads*cylinders*sectors*512  */
	geo->heads     = 2;         /* 磁头，就是表示有多少个面   假设有2面 */
	geo->cylinders = 32;        /* 柱面，就是表示有多少个环   假设有32环 */
	geo->sectors   = RAMBLOCK_SIZE/2/32/512;  /* 扇区， 前面两个参数，可以随便定，但这个不能乱写 */
	
	return 0;
}

static const struct block_device_operations ramblock_fops = {
	.owner	= THIS_MODULE,
//	.ioctl	= ramblock_ioctl,
	.getgeo = ramblock__getgeo,      /* 为了兼容fdisk这个比较老的分区工具 ，于是提供这个函数 */
};

static void do_ramblock_request (struct request_queue * q)
{
//	static int r_cnt = 0;
//	static int w_cnt = 0;
	struct request *req;
	 
//	printk("do_ramblock_request %d\n", cnt++);

	req = blk_fetch_request(q); /*里面运用了电梯调度算法*/
	while (req) 
	{
		/* 数据传输3要素: 源，目的，长度*/
		/* 源/目的 */
		unsigned long offset = blk_rq_pos(req) * 512;

		/* 目的/源 */
		//req->buffer

		/* 长度 */
		unsigned long len  = blk_rq_cur_bytes(req);

		if (rq_data_dir(req) == READ)
		{
		//	printk("do_ramblock_request read %d\n", r_cnt++);
			memcpy(req->buffer, p_ramblock_buf+offset, len);
		}
		else
		{
		//	printk("do_ramblock_request write %d\n", w_cnt++);
			memcpy(p_ramblock_buf+offset, req->buffer, len);
		}
		/* wrap up, 0 = success, -errno = fail */
		if (!__blk_end_request_cur(req, 0))
			req = blk_fetch_request(q);
		else
			printk("__blk_end_request_cur error !\n");
	
	}
	
}

static int ramblock_init(void)
{
	/* 1.分配一个gendisk结构体 */
	st_p_gendisk = alloc_disk(16);  /* 次设备号个数，分区个数+1 */

	/* 2.设置 */
	/* 2.1 分配/设置队列: 提供读写能力*/
	st_p_request_queue = blk_init_queue(do_ramblock_request, &ramblock_lock);
	st_p_gendisk->queue = st_p_request_queue;
	
	/* 2.2 设置其他属性: 比如容量*/
	major = register_blkdev(0, "ramblock"); /* cat /proc/devices */
	
	st_p_gendisk->major       = major;
	st_p_gendisk->first_minor = 0; /* 从0开始到15，总共16个次设备号都对应这个块设备*/
	sprintf(st_p_gendisk->disk_name, "ramblock");
	st_p_gendisk->fops = &ramblock_fops;

	set_capacity(st_p_gendisk, RAMBLOCK_SIZE / 512);  /* 先设置1M大小*/ /* 内核的文件系统的扇区的单位是512字节*/

	/* 3.硬件相关操作*/
	p_ramblock_buf = kzalloc(RAMBLOCK_SIZE,  GFP_KERNEL); 
	//只构造buf，没有对这块内存处理，里面的数据为0，加载驱动时，会提示 unknown partition table 不识别的分区表
	
	/* 4.注册*/
	add_disk(st_p_gendisk);
	return 0;
}

static void ramblock_exit(void)
{
	unregister_blkdev(major, "ramblock");
	del_gendisk(st_p_gendisk);
	put_disk(st_p_gendisk);
	blk_cleanup_queue(st_p_request_queue);

	kfree(p_ramblock_buf);
}

module_init(ramblock_init);
module_exit(ramblock_exit);

MODULE_LICENSE("GPL");




