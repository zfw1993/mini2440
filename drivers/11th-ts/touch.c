#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <plat/adc.h>
#include <plat/regs-adc.h>
#include <plat/ts.h>


struct s3c2440_ts_regs
{
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
	
};

static volatile struct s3c2440_ts_regs *vl_st_p_s3c2440_ts_regs;
static struct input_dev *st_p_s3c_ts_dev;

static struct timer_list st_ts_timer;

static void enter_wait_pen_down_mode(void)
{
	/* bit[8] : Detect Stylus Up or Down status
	 *            0 = Detect Stylus Down Interrupt Signal.
	 *            1 = Detect Stylus Up Interrupt Signal
	 * bit[7] : YM Switch Enable
	 * 
	 */
	
	vl_st_p_s3c2440_ts_regs->adctsc = 0xd3;  //waiting for interrupt mode
}

static void enter_wait_pen_up_mode(void)
{
	vl_st_p_s3c2440_ts_regs->adctsc = 0x1d3;  //waiting for interrupt mode	
}

static void enter_measure_xy_mode(void)
{
	//测量x,y坐标时，上拉电阻都是断开的
	vl_st_p_s3c2440_ts_regs->adctsc = (1<<2) | (1<<3);
}

static void start_adc(void)
{
	vl_st_p_s3c2440_ts_regs->adccon |= (1<<0); //启动ADC 
}

static irqreturn_t pen_down_up_irq(int irq, void *dev_id)
{
	/* ADCDAT0 
	 * bit[15]: Up or Down state of stylus at waiting for interrupt mode
	 *          0 = Stylus down state.
     *          1 = Stylus up state. 
	 */

	if (vl_st_p_s3c2440_ts_regs->adcdat0 & (1<<15))
	{
		//printk("pen up\n");

		input_report_abs(st_p_s3c_ts_dev, ABS_PRESSURE, 0);   /* 1-表示按下，0-表示松开 */											
		input_report_key(st_p_s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(st_p_s3c_ts_dev);  //表示事件上报完毕
		
		enter_wait_pen_down_mode();			
	}
	else
	{
//		printk("pen down\n");
//		enter_wait_pen_up_mode();
		enter_measure_xy_mode();    //测量xy坐标模式
		start_adc();                //启动ADC
	}

	return IRQ_HANDLED;       //如果没有写这个返回值，会出现"irq event 79: bogus return value a" 这样的错误
}  

static int s3c_filter_ts(int x[], int y[])
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0]+x[1]) / 2;
	avr_y = (y[0]+y[1]) / 2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	avr_x = (x[1]+x[2]) / 2;
	avr_y = (y[1]+y[2]) / 2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	return 1;
	
}


static irqreturn_t adc_irq(int irq, void *dev_id)
{
	static int cnt = 0;
	int adcdat0,adcdat1;
	static int x[4],y[4];
	 
	//优化措施2: 如果ADC完成时，发现触摸笔已经松开，则丢弃此次结果
	adcdat0 = vl_st_p_s3c2440_ts_regs->adcdat0;
	adcdat1 = vl_st_p_s3c2440_ts_regs->adcdat1;
	
	if (vl_st_p_s3c2440_ts_regs->adcdat0 & (1<<15))
	{
		//如果已经松开
		cnt = 0;
		input_report_abs(st_p_s3c_ts_dev, ABS_PRESSURE, 0);   /* 1-表示按下，0-表示松开 */											
		input_report_key(st_p_s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(st_p_s3c_ts_dev);
		
		enter_wait_pen_down_mode();  //那就进入按下模式，不打印值，丢弃此结果
	}
	else
	{		
		//printk("adc_irq cnt = %d, x = %d, y = %d\n", cnt++, adcdat0 & 0x3ff, adcdat1 & 0x3ff);
		//enter_wait_pen_up_mode();  
		/* 优化措施3: 多次测量求平均值*/	
		x[cnt] = adcdat0 & 0x3ff;
		y[cnt] = adcdat1 & 0x3ff;
		cnt++;
		if (4 == cnt)
		{
			/* 优化措施4: 软件过滤 */
			if (s3c_filter_ts(x, y))
			{
				//如果过滤成功
				//printk("x = %d, y = %d\n", (x[0]+x[1]+x[2]+x[3])/4, (y[0]+y[1]+y[2]+y[3])/4);

				input_report_abs(st_p_s3c_ts_dev, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);
				input_report_abs(st_p_s3c_ts_dev, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
				input_report_abs(st_p_s3c_ts_dev, ABS_PRESSURE, 1);   /* 1-表示按下，0-表示松开 */                                          
				input_report_key(st_p_s3c_ts_dev, BTN_TOUCH, 1);
				input_sync(st_p_s3c_ts_dev);  //表示事件上报完毕
			}
			cnt = 0;
            enter_wait_pen_up_mode();          //如果没有这个函数，则一直处于按下状态
			//启动定时器处理长按/滑动的情况
			mod_timer(&st_ts_timer, jiffies + HZ/100);
		}
		else
		{
			enter_measure_xy_mode();    //测量xy坐标模式
			start_adc();                //启动ADC
		}
	}
	return IRQ_HANDLED;
}


static void s3c_ts_timer_function(unsigned long dat)
{
	if (vl_st_p_s3c2440_ts_regs->adcdat0 & (1<<15))
	{
		//如果已经松开	
		input_report_abs(st_p_s3c_ts_dev, ABS_PRESSURE, 0);   /* 1-表示按下，0-表示松开 */											
		input_report_key(st_p_s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(st_p_s3c_ts_dev);
		enter_wait_pen_down_mode();	
	}
	else
	{
		enter_measure_xy_mode();    //测量xy坐标模式
		start_adc();                //启动ADC	
	}
}

static int s3c_ts_init(void)
{
	struct clk *st_p_clk;
	//1.分配一个input_dev结构体
	st_p_s3c_ts_dev = input_allocate_device();
	
	//2.设置
	//2.1 能产生哪类事件
	set_bit(EV_KEY, st_p_s3c_ts_dev->evbit);
	set_bit(EV_ABS, st_p_s3c_ts_dev->evbit);

	//2.2 能产生这类事件里的哪些事件
	set_bit(BTN_TOUCH, st_p_s3c_ts_dev->keybit);

	input_set_abs_params(st_p_s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(st_p_s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(st_p_s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0); 

	//3.注册
	input_register_device(st_p_s3c_ts_dev);

	//4.硬件相关的操作
	//4.1 使能时钟(CLKCON[15])
	st_p_clk = clk_get(NULL, "adc");  //参考s3c2410_ts.c的s3c2410ts_probe函数
	clk_enable(st_p_clk);
	//4.2 设置s3c2440的ADC/TS寄存器
	vl_st_p_s3c2440_ts_regs = ioremap(0x58000000, sizeof(struct s3c2440_ts_regs));

	/* bit[14]   : A/D converter prescaler enable : 1 = Enable
	 * bit[13:6] : A/D converter prescaler value  : Data = 49
	 *             ADCLK = PCLK / (49+1) = 50MHz / (49+1) = 1MHz
	 * bit[2]    : Standby mode select            : 0 = Normal operation mode
	 * bit[1]    : A/D conversion start by read   : 0 = Disable start by read operation
	 * bit[0]    : A/D conversion starts by enable: 先不设置这一位
	 *             0 = No operation
	 *             1 = A/D conversion starts and this bit is cleared after the start-up.
	 */ 
	vl_st_p_s3c2440_ts_regs->adccon = (1<<14) | (49<<6) | (0<<0);

   request_irq(IRQ_TC, pen_down_up_irq, 0, "ts_pen", NULL);      	
   //request_irq(IRQ_TC, pen_down_up_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);  
   request_irq(IRQ_ADC, adc_irq, 0, "adc", NULL); 	   

   //优化措施1:
   //设置ADCDLY为最大值，这使得电压稳定后，再发出IRQ中断
   vl_st_p_s3c2440_ts_regs->adcdly = 0xffff;
	
	/* 优化措施5: 使用定时器处理长按，滑动的情况
	 *
	 */
	init_timer(&st_ts_timer);
	st_ts_timer.function = s3c_ts_timer_function;
	add_timer(&st_ts_timer);

	enter_wait_pen_down_mode();  //等待按下 
	
	return 0;
}

static void s3c_ts_exit(void)          
{
	del_timer(&st_ts_timer);
	free_irq(IRQ_TC, NULL);
	free_irq(IRQ_ADC, NULL);      //记得释放中断
	iounmap(vl_st_p_s3c2440_ts_regs );
	
	input_unregister_device(st_p_s3c_ts_dev);
	input_free_device(st_p_s3c_ts_dev);		
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);

MODULE_LICENSE("GPL");
