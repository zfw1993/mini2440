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

//#define PRINTK	printk   /* �����ӡ��Ϣ(���ڵ���: PRINTK()) */
#define PRINTK(...)    /* ����ʾ��ӡ��Ϣ(���ȳɹ��Ͱ����ر�) */


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
	
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */ 
	{	
	    s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->addr << 1; /* �㲻��Ϊʲô�����ƶ��������� */
		
		/* bit[7:6] = 10 : 10: Master receive mode 	 
		 * bit[5]   = 1  : Busy	 
		 * bit[4]   = 1	 : Enable Rx/Tx	 
		 */  
	    s3c2440_i2c_regs->iicstat = 0xb0;         // �������գ�����(һ�����ͻ�����ж�)
	} 
	else  /* д */
	{
	    s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->addr << 1; /* 0x50 => 0xA0 */	

		/* bit[7:6] = 11 : Master transmit mode 	 
	     * bit[5]   = 1  : (write)START signal generation.	 
	     * bit[4]   = 1	 : Enable Rx/Tx	 
	     */    
	    s3c2440_i2c_regs->iicstat = 0xf0;         // �������ͣ�����(һ�������ͻ�����ж�) 
	}
}


static void s3c2440_i2c_stop(int err)  /* zfw -- ��ֹͣ����������(�㲻��) */
{
	s3c2440_i2c_xfer_data.state = STATE_STOP;
	s3c2440_i2c_xfer_data.err 	= err;

	PRINTK("STATE_STOP, err = %d\n", err);

	/* ���������ֹͣ���쳣�����ֹͣ */
	/* ����Stop�ź�  */
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */
	{
		// �������лָ�I2C����������P�ź�                
		/* bit[7:6] = 10 : 0: Master receive mode 				 
		 * bit[5]   = 0  : STOP signal generation				 
		 * bit[4]   = 1	 : Enable Rx/Tx				 
		 */                 
		s3c2440_i2c_regs->iicstat = 0x90;                
		s3c2440_i2c_regs->iiccon  = 0xaf;                
		ndelay(50);  // �ȴ�һ��ʱ���Ա�P�ź��Ѿ�����    
	}
	else  /* д */
	{
		// �������������ָ�I2C����������P�ź�                               
		/* bit[7:6] = 11 : 11: Master transmit mode 				 
		 * bit[5]   = 0  : STOP signal generation				 
		 * bit[4]   = 1	 : Enable Rx/Tx				 
		 */                 
		s3c2440_i2c_regs->iicstat = 0xd0;		
		
		/* bit[7] = 1, ʹ��ACK			     
		 * bit[6] = 0, IICCLK = PCLK/16			     
		 * bit[5] = 1, ʹ���ж�			     
		 * bit[3:0] = 0xf, Tx clock = IICCLK/16			     
		 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz			     
		 */               
		s3c2440_i2c_regs->iiccon = 0xaf;                
		ndelay(50); // �ȴ�һ��ʱ���Ա�P�ź��Ѿ�����
	}

	/* ���� */
	wake_up(&s3c2440_i2c_xfer_data.wait);
	
}


static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	unsigned long timeout;
	
	/* ��num��msgs��i2c���ݷ��ͳ�ȥ/������ */
	s3c2440_i2c_xfer_data.msgs 	   = msgs;
	s3c2440_i2c_xfer_data.msgs_num = num;
	s3c2440_i2c_xfer_data.cur_msgs = 0;
	s3c2440_i2c_xfer_data.cur_ptr  = 0;
	s3c2440_i2c_xfer_data.err  	   = -ENODEV;

	s3c2440_i2c_start();

	/* ����(��Ϊ��֪��ʲôʱ��Ŵ����������������֮����������) */	
	/* �����״̬����ֹͣ��״̬�ͽ������� */
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


/* 1. ����/����adapter */
static struct i2c_adapter s3c2440_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name       = "s3c2440_i2c",
	.algo		= &s3c2440_i2c_algo,
};





static int isLastMsg(void)
{
	return (s3c2440_i2c_xfer_data.cur_msgs == s3c2440_i2c_xfer_data.msgs_num -1);
}

static int isEndData(void)  /* ���һ��(��Ч)���� */
{
	return (s3c2440_i2c_xfer_data.cur_ptr >= s3c2440_i2c_xfer_data.msgs->len);
}

static int isLastData(void)  /* ���һ����Ч���� */
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

	switch (s3c2440_i2c_xfer_data.state)  /* �����ж�ԭ���ж��� */
	{
		case STATE_START: /* ����S�źź��豸��ַ�󣬲����ж� */
		{
			PRINTK("STATE_START\n");
			/* ���û��ACK���򷵻ش��� */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)  /* 1: Last-received bit is 1 (ACK was not received). */
			{
				s3c2440_i2c_stop(-ENODEV);	
				break;
			}

			if (isLastMsg() && isEndData())  /* ���һ����Ϣ && ���һ������ */
			{
				s3c2440_i2c_stop(0);
				break;
			}

			/* ������һ��״̬(��ʱ��ͼ(Random Read)������) */
			if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */
			{
				s3c2440_i2c_xfer_data.state = STATE_READ;
				goto next_read;
			}
			else
			{
				s3c2440_i2c_xfer_data.state = STATE_WRITE;
			}
			// break;  /* ����дbreak����Ϊ�����д�Ļ��������Ͻ�����һ��case�������� */
		}

		case STATE_WRITE:
		{
			PRINTK("STATE_WRITE\n");
			/* ���û��ACK���򷵻ش��� */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)  /* 1: Last-received bit is 1 (ACK was not received). */
			{
				s3c2440_i2c_stop(-ENODEV);	
				break;
			}

			if (!isEndData())  /* ����������һ�����ݣ���ô˵��Ҫ����Ҫ���ͣ��Ǿͷ��� */
			{
				s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr]; 
				s3c2440_i2c_xfer_data.cur_ptr++;   /* s3c2440_i2c_xfer_data.cur_ptr������һ���ֽھͼ�1 */
				
				// ������д��IICDS����Ҫһ��ʱ����ܳ�����SDA����            
				ndelay(50);               
				s3c2440_i2c_regs->iiccon = 0xaf;      // �ָ�I2C����           
				break; 
			}
			else if (!isLastMsg()) /* ��������һ�����ݣ��ж����ǲ������һ����Ϣ */
			{
				/* ����������һ����Ϣ����ô�ʹ�����һ����Ϣ */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msgs++;
				s3c2440_i2c_xfer_data.cur_ptr = 0; /* ����һ����Ϣ��buf[0]��ʼ */
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* ����S�źźͷ����豸��ַ */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* �����һ����Ϣ�����һ������ */
				s3c2440_i2c_stop(0);	
				break;
			}
		}

		case STATE_READ:
		{
			PRINTK("STATE_READ\n");
			/* �������� */
			s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr] = s3c2440_i2c_regs->iicds; 
			s3c2440_i2c_xfer_data.cur_ptr++;   /* s3c2440_i2c_xfer_data.cur_ptr������һ���ֽھͼ�1 */

next_read:
			if (!isEndData())  /* �������û���꣬������������� */
			{
				if (isLastData()) /* ���������ȡ�����������һ�����ݣ���ô����ACK(��ʱ��ͼ) */
				{
					/* bit[7] = 0, ��ʹ��ACK			     
					 * bit[6] = 0, IICCLK = PCLK/16			     
					 * bit[5] = 1, ʹ���ж�			     
					 * bit[3:0] = 0xf, Tx clock = IICCLK/16			     
					 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz			     
					 */
					s3c2440_i2c_regs->iiccon = 0x2f;   // �ָ�I2C���䣬���յ���һ����ʱ��ACK
				}
				else
				{
					s3c2440_i2c_regs->iiccon = 0xaf;   // �ָ�I2C���䣬���յ���һ����ʱ����ACK
				}
				break;
			}
			else if (!isLastMsg())
			{			
				/* ����������һ����Ϣ����ô�ʹ�����һ����Ϣ */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msgs++;
				s3c2440_i2c_xfer_data.cur_ptr = 0; /* ����һ����Ϣ��buf[0]��ʼ */
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* ����S�źźͷ����豸��ַ */
				s3c2440_i2c_start();
				break;
			}			
			else
			{
				/* �����һ����Ϣ�����һ������ */
				s3c2440_i2c_stop(0);	
				break;
			}
			break;
		}
		default: break;
	}

	/* ���ж� */
	s3c2440_i2c_regs->iiccon &= ~(S3C2410_IICCON_IRQPEND); /* 0: Clear pending condition & Resume the operation (when write) */

	return IRQ_HANDLED;
}

static void s3c2440_i2c_init(void)
{   
	struct clk *clk;

	/* linuxΪ��ʡ�磬������ʱ�����ò�����ģ��ص�������Ժ���ʹ�ã��Ͱ���Ӧ��ģ��� */
	clk = clk_get(NULL, "i2c");
	clk_enable(clk);          /* ʹ��I2Cʱ�� */
	
	//GPECON |= 0xa0000000;   // ѡ�����Ź��ܣ�GPE15:IICSDA, GPE14:IICSCL    
	s3c_gpio_cfgpin(S3C2410_GPE(14), S3C2410_GPE14_IICSCL);
	s3c_gpio_cfgpin(S3C2410_GPE(15), S3C2410_GPE15_IICSDA);
	
	// INTMSK &= ~(BIT_IIC);  /* �жϲ��ùܣ�����ͨ��request_irq()���� */  
	
	
	/* bit[7] = 1, ʹ��ACK     
	 * bit[6] = 0, IICCLK = PCLK/16     
	 * bit[5] = 1, ʹ���ж�     
	 * bit[3:0] = 0xf, Tx clock = IICCLK/16     
	 * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz     
	 */   	
	 s3c2440_i2c_regs->iiccon  = (1<<7) | (0<<6) | (1<<5) | (0xf);  // 0xaf    
	 s3c2440_i2c_regs->iicadd  = 0x10;     // S3C24xx slave address = [7:1]    
	 s3c2440_i2c_regs->iicstat = 0x10;     // I2C�������ʹ��(Rx/Tx)
}
static int i2c_bus_s3c2440_init(void)
{
	/* 2. Ӳ����صĲ��� */
	s3c2440_i2c_regs = ioremap(0x54000000, sizeof(struct s3c2440_i2c_regs));

	s3c2440_i2c_init();	

	request_irq(IRQ_IIC, s3c2440_i2c_xfer_irq, 0, "s3c2440-i2c", NULL);

	init_waitqueue_head(&s3c2440_i2c_xfer_data.wait);
	/* 3.  ע��i2c_adapter */

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


