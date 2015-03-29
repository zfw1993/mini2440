
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
	/* ���� = heads*cylinders*sectors*512  */
	geo->heads     = 2;         /* ��ͷ�����Ǳ�ʾ�ж��ٸ���   ������2�� */
	geo->cylinders = 32;        /* ���棬���Ǳ�ʾ�ж��ٸ���   ������32�� */
	geo->sectors   = RAMBLOCK_SIZE/2/32/512;  /* ������ ǰ������������������㶨�������������д */
	
	return 0;
}

static const struct block_device_operations ramblock_fops = {
	.owner	= THIS_MODULE,
//	.ioctl	= ramblock_ioctl,
	.getgeo = ramblock__getgeo,      /* Ϊ�˼���fdisk����Ƚ��ϵķ������� �������ṩ������� */
};

static void do_ramblock_request (struct request_queue * q)
{
//	static int r_cnt = 0;
//	static int w_cnt = 0;
	struct request *req;
	 
//	printk("do_ramblock_request %d\n", cnt++);

	req = blk_fetch_request(q); /*���������˵��ݵ����㷨*/
	while (req) 
	{
		/* ���ݴ���3Ҫ��: Դ��Ŀ�ģ�����*/
		/* Դ/Ŀ�� */
		unsigned long offset = blk_rq_pos(req) * 512;

		/* Ŀ��/Դ */
		//req->buffer

		/* ���� */
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
	/* 1.����һ��gendisk�ṹ�� */
	st_p_gendisk = alloc_disk(16);  /* ���豸�Ÿ�������������+1 */

	/* 2.���� */
	/* 2.1 ����/���ö���: �ṩ��д����*/
	st_p_request_queue = blk_init_queue(do_ramblock_request, &ramblock_lock);
	st_p_gendisk->queue = st_p_request_queue;
	
	/* 2.2 ������������: ��������*/
	major = register_blkdev(0, "ramblock"); /* cat /proc/devices */
	
	st_p_gendisk->major       = major;
	st_p_gendisk->first_minor = 0; /* ��0��ʼ��15���ܹ�16�����豸�Ŷ���Ӧ������豸*/
	sprintf(st_p_gendisk->disk_name, "ramblock");
	st_p_gendisk->fops = &ramblock_fops;

	set_capacity(st_p_gendisk, RAMBLOCK_SIZE / 512);  /* ������1M��С*/ /* �ں˵��ļ�ϵͳ�������ĵ�λ��512�ֽ�*/

	/* 3.Ӳ����ز���*/
	p_ramblock_buf = kzalloc(RAMBLOCK_SIZE,  GFP_KERNEL); 
	//ֻ����buf��û�ж�����ڴ洦�����������Ϊ0����������ʱ������ʾ unknown partition table ��ʶ��ķ�����
	
	/* 4.ע��*/
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




