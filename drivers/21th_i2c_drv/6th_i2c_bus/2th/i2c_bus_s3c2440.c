#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>



#include <asm/irq.h>

#include <plat/regs-iic.h>
#include <plat/iic.h>

//#define PRINTK	printk   /* 输出打印信息(用于调试: PRINTK()) */
#define PRINTK(...)    /* 不显示打印信息(调度成功就把它关闭) */


/* i2c controller state */

enum s3c24xx_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};


struct s3c2440_i2c_regs {
	unsigned int iiccon;
	unsigned int iicstat;
	unsigned int iicadd;
	unsigned int iicds;
	unsigned int iiclc;
};

static struct s3c2440_i2c_regs *s3c2440_i2c_regs;

struct s3c2440_i2c_xfer_data {
	struct i2c_msg *msgs;
	int msgs_num;
	int cur_msgs;
	int cur_ptr;
	int state;
	int err;
	wait_queue_head_t	wait;
};

static struct s3c2440_i2c_xfer_data s3c2440_i2c_xfer_data;


static void s3c2440_i2c_start(void)
{
	s3c2440_i2c_xfer_data.state = STATE_START;
	
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */ 
	{	
	    s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->addr << 1; /* 搞不懂为什么是左移而不是右移 */
		
		/* bit[7:6] = 10 : 10: Master receive mode 	 
		 * bit[5]   = 1  : Busy	 
		 * bit[4]   = 1	 : Enable Rx/Tx	 
		 */  
	    s3c2440_i2c_regs->iicstat = 0xb0;         // 主机接收，启动(一启动就会产生中断)
	} 
	else  /* 写 */
	{
	    s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->addr << 1; /* 0x50 => 0xA0 */	

		/* bit[7:6] = 11 : Master transmit mode 	 
	     * bit[5]   = 1  : (write)START signal generation.	 
	     * bit[4]   = 1	 : Enable Rx/Tx	 
	     */    
	    s3c2440_i2c_regs->iicstat = 0xf0;         // 主机发送，启动(一启动，就会产生中断) 
	}
}


static void s3c2440_i2c_stop(int err)  /* zfw -- 在停止函数会休眠(搞不懂) */
{
	s3c2440_i2c_xfer_data.state = STATE_STOP;
	s3c2440_i2c_xfer_data.err 	= err;

	PRINTK("STATE_STOP, err = %d\n", err);

	/* 正常情况的停止与异常情况的停止 */
	/* 发出Stop信号  */
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */
	{
		// 下面两行恢复I2C操作，发出P信号                
		/* bit[7:6] = 10 : 0: Master receive mode 				 
		 * bit[5]   = 0  : STOP signal generation				 
		 * bit[4]   = 1	 : Enable Rx/Tx				 
		 */                 
		s3c2440_i2c_regs->iicstat = 0x90;                
		s3c2440_i2c_regs->iiccon  = 0xaf;                
		ndelay(50);  // 等待一段时间以便P信号已经发出    
	}
	else  /* 写 */
	{
		// 下面两行用来恢复I2C操作，发出P信号                               
		/* bit[7:6] = 11 : 11: Master transmit mode 				 
		 * bit[5]   = 0  : STOP signal generation				 
		 * bit[4]   = 1	 : Enable Rx/Tx				 
		 */                 
		s3c2440_i2c_regs->iicstat = 0xd0;		
		
		/* bit[7] = 1, 使能ACK			     
		 * bit[6] = 0, IICCLK = PCLK/16			     
		 * bit[5] = 1, 使能中断			     
		 * bit[3:0] = 0xf, Tx clock = IICCLK/16			     
		 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz			     
		 */               
		s3c2440_i2c_regs->iiccon = 0xaf;                
		ndelay(50); // 等待一段时间以便P信号已经发出
	}

	/* 唤醒 */
	wake_up(&s3c2440_i2c_xfer_data.wait);
	
}


static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	unsigned long timeout;
	
	/* 把num个msgs的i2c数据发送出去/读进来 */
	s3c2440_i2c_xfer_data.msgs 	   = msgs;
	s3c2440_i2c_xfer_data.msgs_num = num;
	s3c2440_i2c_xfer_data.cur_msgs = 0;
	s3c2440_i2c_xfer_data.cur_ptr  = 0;
	s3c2440_i2c_xfer_data.err  	   = -ENODEV;

	s3c2440_i2c_start();

	/* 休眠(因为不知道什么时候才传输结束，所以启动之后，在这休眠) */	
	/* 如果不状态不是停止的状态就进入休眠 */
	timeout = wait_event_timeout(s3c2440_i2c_xfer_data.wait, s3c2440_i2c_xfer_data.state == STATE_STOP, HZ * 5);
	if (timeout == 0)
	{
		printk("s3c2440_i2c_xfer time out\n");
		return -ETIMEDOUT;
	}
	else
	{
		return s3c2440_i2c_xfer_data.err;
	}
}


/* declare our i2c functionality */
static u32 s3c2440_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}


static const struct i2c_algorithm s3c2440_i2c_algo = {
//	.smbuf_xfer			= ,
	.master_xfer		= s3c2440_i2c_xfer,
	.functionality		= s3c2440_i2c_func,
};


/* 1. 分配/设置adapter */
static struct i2c_adapter s3c2440_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name       = "s3c2440_i2c",
	.algo		= &s3c2440_i2c_algo,
};





static int isLastMsg(void)
{
	return (s3c2440_i2c_xfer_data.cur_msgs == s3c2440_i2c_xfer_data.msgs_num -1);
}

static int isEndData(void)  /* 最后一个(无效)数据 */
{
	return (s3c2440_i2c_xfer_data.cur_ptr >= s3c2440_i2c_xfer_data.msgs->len);
}

static int isLastData(void)  /* 最后一个有效数据 */
{
	return (s3c2440_i2c_xfer_data.cur_ptr == s3c2440_i2c_xfer_data.msgs->len - 1);	
}

static irqreturn_t s3c2440_i2c_xfer_irq(int irq, void *dev_id)
{
	unsigned int iicSt;   /* IIC State */

	iicSt = s3c2440_i2c_regs->iicstat;

	if(iicSt & 0x8) /* IIC-bus arbitration procedure status flag bit. */
	{ 
		printk("Bus arbitration failed\n\r"); 
	}  

	switch (s3c2440_i2c_xfer_data.state)  /* 产生中断原因有多种 */
	{
		case STATE_START: /* 发出S信号和设备地址后，产生中断 */
		{
			PRINTK("STATE_START\n");
			/* 如果没有ACK，则返回错误 */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)  /* 1: Last-received bit is 1 (ACK was not received). */
			{
				s3c2440_i2c_stop(-ENODEV);	
				break;
			}

			if (isLastMsg() && isEndData())  /* 最后一个消息 && 最后一个数据 */
			{
				s3c2440_i2c_stop(0);
				break;
			}

			/* 进入下一个状态(看时序图(Random Read)就明白) */
			if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */
			{
				s3c2440_i2c_xfer_data.state = STATE_READ;
				goto next_read;
			}
			else
			{
				s3c2440_i2c_xfer_data.state = STATE_WRITE;
			}
			// break;  /* 不用写break，因为如果是写的话，就马上进行下一个case发数据了 */
		}

		case STATE_WRITE:
		{
			PRINTK("STATE_WRITE\n");
			/* 如果没有ACK，则返回错误 */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)  /* 1: Last-received bit is 1 (ACK was not received). */
			{
				s3c2440_i2c_stop(-ENODEV);	
				break;
			}

			if (!isEndData())  /* 如果不是最后一个数据，那么说还要数据要发送，那就发送 */
			{
				s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr]; 
				s3c2440_i2c_xfer_data.cur_ptr++;   /* s3c2440_i2c_xfer_data.cur_ptr传输完一个字节就加1 */
				
				// 将数据写入IICDS后，需要一段时间才能出现在SDA线上            
				ndelay(50);               
				s3c2440_i2c_regs->iiccon = 0xaf;      // 恢复I2C传输           
				break; 
			}
			else if (!isLastMsg()) /* 如果是最后一个数据，判断它是不是最后一个消息 */
			{
				/* 如果不是最后一个消息，那么就处理下一个消息 */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msgs++;
				s3c2440_i2c_xfer_data.cur_ptr = 0; /* 从下一个消息的buf[0]开始 */
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* 发出S信号和发出设备地址 */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* 是最后一个消息的最后一个数据 */
				s3c2440_i2c_stop(0);	
				break;
			}
		}

		case STATE_READ:
		{
			PRINTK("STATE_READ\n");
			/* 读出数据 */
			s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr] = s3c2440_i2c_regs->iicds; 
			s3c2440_i2c_xfer_data.cur_ptr++;   /* s3c2440_i2c_xfer_data.cur_ptr传输完一个字节就加1 */

next_read:
			if (!isEndData())  /* 如果数据没读完，继续发起读操作 */
			{
				if (isLastData()) /* 如果即将读取的数据是最后一个数据，那么不发ACK(看时序图) */
				{
					/* bit[7] = 0, 不使能ACK			     
					 * bit[6] = 0, IICCLK = PCLK/16			     
					 * bit[5] = 1, 使能中断			     
					 * bit[3:0] = 0xf, Tx clock = IICCLK/16			     
					 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz			     
					 */
					s3c2440_i2c_regs->iiccon = 0x2f;   // 恢复I2C传输，接收到下一数据时无ACK
				}
				else
				{
					s3c2440_i2c_regs->iiccon = 0xaf;   // 恢复I2C传输，接收到下一数据时发出ACK
				}
				break;
			}
			else if (!isLastMsg())
			{			
				/* 如果不是最后一个消息，那么就处理下一个消息 */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msgs++;
				s3c2440_i2c_xfer_data.cur_ptr = 0; /* 从下一个消息的buf[0]开始 */
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* 发出S信号和发出设备地址 */
				s3c2440_i2c_start();
				break;
			}			
			else
			{
				/* 是最后一个消息的最后一个数据 */
				s3c2440_i2c_stop(0);	
				break;
			}
			break;
		}
		default: break;
	}

	/* 清中断 */
	s3c2440_i2c_regs->iiccon &= ~(S3C2410_IICCON_IRQPEND); /* 0: Clear pending condition & Resume the operation (when write) */

	return IRQ_HANDLED;
}

static void s3c2440_i2c_init(void)
{   
	struct clk *clk;

	/* linux为了省电，启动的时候会把用不到的模块关掉，如果以后想使用，就把相应的模块打开 */
	clk = clk_get(NULL, "i2c");
	clk_enable(clk);          /* 使能I2C时钟 */
	
	//GPECON |= 0xa0000000;   // 选择引脚功能：GPE15:IICSDA, GPE14:IICSCL    
	s3c_gpio_cfgpin(S3C2410_GPE(14), S3C2410_GPE14_IICSCL);
	s3c_gpio_cfgpin(S3C2410_GPE(15), S3C2410_GPE15_IICSDA);
	
	// INTMSK &= ~(BIT_IIC);  /* 中断不用管，可以通过request_irq()设置 */  
	
	
	/* bit[7] = 1, 使能ACK     
	 * bit[6] = 0, IICCLK = PCLK/16     
	 * bit[5] = 1, 使能中断     
	 * bit[3:0] = 0xf, Tx clock = IICCLK/16     
	 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz     
	 */   	
	 s3c2440_i2c_regs->iiccon  = (1<<7) | (0<<6) | (1<<5) | (0xf);  // 0xaf    
	 s3c2440_i2c_regs->iicadd  = 0x10;     // S3C24xx slave address = [7:1]    
	 s3c2440_i2c_regs->iicstat = 0x10;     // I2C串行输出使能(Rx/Tx)
}
static int i2c_bus_s3c2440_init(void)
{
	/* 2. 硬件相关的操作 */
	s3c2440_i2c_regs = ioremap(0x54000000, sizeof(struct s3c2440_i2c_regs));

	s3c2440_i2c_init();	

	request_irq(IRQ_IIC, s3c2440_i2c_xfer_irq, 0, "s3c2440-i2c", NULL);

	init_waitqueue_head(&s3c2440_i2c_xfer_data.wait);
	/* 3.  注册i2c_adapter */

	i2c_add_adapter(&s3c2440_i2c_adapter);
	return 0;
}

static void i2c_bus_s3c2440_exit(void)
{
	i2c_del_adapter(&s3c2440_i2c_adapter);
	free_irq(IRQ_IIC, NULL);
	iounmap(s3c2440_i2c_regs);
}


module_init(i2c_bus_s3c2440_init);
module_exit(i2c_bus_s3c2440_exit);

MODULE_LICENSE("GPL");


