
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

static void emulator_rx_packet(struct sk_buff *skb, struct net_device *dev)
{
	/* 参考LDD3 */
	
}


static netdev_tx_t	virt_net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	static int cnt = 0;
	printk("virt_net_send_packet cnt = %d\n", cnt++);

	/* 对于真实的网卡是把skb里的数据通过网卡发送出去 */
	netif_stop_queue(dev);   /* 停止该网卡的队列 */
	/* ........ */           /* 把skb的数据写入网卡 */

	/* 构造一个假的sk_buff，上报 */
	emulator_rx_packet(skb, dev);
	
	dev_kfree_skb (skb);	 /* 释放skb */
	netif_wake_queue(dev);   /* 数据全部发送出去后，唤醒网卡的队列 */
	
	/* 而我们的是虚拟的网卡 */

	/* 更新统计信息 */
	st_p_vnet_dev->stats.tx_packets++;
	st_p_vnet_dev->stats.tx_bytes += skb->len;

	
	
	
	return NETDEV_TX_OK;	
}


static const struct net_device_ops virt_net_ops = {
//	.ndo_open		= virt_net_open,
//  .ndo_stop		= net_close,
//	.ndo_tx_timeout		= net_timeout,
	.ndo_start_xmit 	= virt_net_send_packet,
//	.ndo_get_stats		= net_get_stats,
//	.ndo_set_rx_mode	= set_multicast_list,
//	.ndo_set_mac_address 	= set_mac_address,
//	.ndo_change_mtu		= eth_change_mtu,
//	.ndo_validate_addr	= eth_validate_addr,
};



static int virt_net_init(void)
{
	/* 1. 分配一个net_device结构体 */
//	st_p_vnet_dev = alloc_netdev_mqs(0, "vnet%d", ether_setup, 1, 1);  /* alloc_etherdev */ 
	st_p_vnet_dev = alloc_netdev(0, "vnet%d", ether_setup);  /* alloc_etherdev */ 
	/* sizeof_priv 私有数据个数为0, 不需要私有数据*/

	if (!st_p_vnet_dev)    
	{
		printk("Could not allocate st_p_vnet_dev\n");
		return -1;
	}
		
	/* 2. 设置 */            
	//st_p_vnet_dev->netdev_ops->ndo_start_xmit = virt_net_send_packet; /* 这样设置有错 */
	st_p_vnet_dev->netdev_ops = &virt_net_ops; 

	/* 设置MAC地址 */
	st_p_vnet_dev->dev_addr[0] = 0x08;   /* 随便设置 */
	st_p_vnet_dev->dev_addr[1] = 0x89;
	st_p_vnet_dev->dev_addr[2] = 0x89;
	st_p_vnet_dev->dev_addr[3] = 0x89;
	st_p_vnet_dev->dev_addr[4] = 0x89;
	st_p_vnet_dev->dev_addr[5] = 0x11;
	

	/* 3. 注册 */
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

MODULE_AUTHOR("ZhengFuWen ForARM 2014/9/24 16:18");
MODULE_LICENSE("GPL");


