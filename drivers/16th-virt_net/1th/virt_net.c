
/* 参考
 * drivers\net\ethernet\cirrus\Cs89x0.c
 */
	 
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gfp.h>
	 
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/atomic.h>



static struct net_device *st_p_vnet_dev;


static int virt_net_init(void)
{
	/* 1. 分配一个net_device结构体 */
	st_p_vnet_dev = alloc_netdev_mqs(0, "vnet%d", ether_setup, 1, 1);  /* alloc_etherdev */ 
	/* sizeof_priv 私有数据个数为0, 不需要私有数据 */

	/* 2. 设置 */

	/* 3. 注册 */
	//register_netdevice(st_p_vnet_dev);
	register_netdev(st_p_vnet_dev);
	
	return 0;
}

static void virt_net_exit(void)
{
	unregister_netdev(st_p_vnet_dev);
	free_netdev(st_p_vnet_dev);
}


module_init(virt_net_init);
module_exit(virt_net_exit);

//MODULE_AUTHOR("ZhengFuWen ForARM 2014/9/24 16:18");
MODULE_LICENSE("GPL");


