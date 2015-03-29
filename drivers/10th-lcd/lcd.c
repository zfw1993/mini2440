
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <mach/regs-lcd.h>
#include <mach/regs-gpio.h>
#include <mach/fb.h>
#include <linux/console.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/uaccess.h>


static struct lcd_regs 
{
	unsigned long lcdcon1;
	unsigned long lcdcon2;
	unsigned long lcdcon3;
	unsigned long lcdcon4;
	unsigned long lcdcon5;
	unsigned long lcdsaddr1;
	unsigned long lcdsaddr2;
	unsigned long lcdsaddr3;
	unsigned long redlut;
	unsigned long greenlut;
	unsigned long bluelut;
	unsigned long reserved[9];
    unsigned long dithmode;
	unsigned long tpal;
	unsigned long lcdintpnd;
	unsigned long lcdsrcpnd;
	unsigned long lcdintmsk;
	unsigned long tconsel;
};


/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int s3c_lcdfb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	unsigned int val;
	
	if (regno > 16)
		return 1;	/* unknown type */

	//用red,green,blue三原色构造出val
	val  = chan_to_field(red,   &info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,  &info->var.blue);

	((u32 *)(info->pseudo_palette))[regno] = val;		
	//pseudo_pal[regno] = val; //或者可以这么写
	
	return 0;
}


static struct fb_ops s3c_lcdfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= s3c_lcdfb_setcolreg,  //设置假的调色板
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static volatile unsigned long *gpccon;
static volatile unsigned long *gpgcon;
static volatile unsigned long *gpdcon;
//static volatile unsigned long *gpddat;

static volatile struct lcd_regs *lcd_regs;
static struct fb_info *s3c_lcd;             //定义一个结构体指针
//static volatile struct fb_info *s3c_lcd;  //用volatile优化之后，出现warning

static u32	pseudo_pal[16];

static int lcd_init(void)
{
	int ret;
	
	//1.分配一个fbinfo
	s3c_lcd = framebuffer_alloc(0, NULL); //不需要额外的空间，需要判断返回值，万一内存不够，分配不成功呢
	if (!s3c_lcd)
	return -ENOMEM;   
	
	//2.设置            
	//2.1设置固定的参数
	strcpy(s3c_lcd->fix.id, "mylcd");
	s3c_lcd->fix.smem_len    = 320*240*32/8; //mini2440的LCD位宽是24，但是2440里会分配4字节即32位(浪费1字节)
	s3c_lcd->fix.type        = FB_TYPE_PACKED_PIXELS;  //用排除法设置这一项
	s3c_lcd->fix.visual      = FB_VISUAL_TRUECOLOR;  //TFT 真彩色
	s3c_lcd->fix.line_length = 320*4;           //其他使用默认值
		
	//2.2设置可变的参数
	s3c_lcd->var.xres           = 320;
	s3c_lcd->var.yres           = 240;
	s3c_lcd->var.xres_virtual   = 320;
	s3c_lcd->var.yres_virtual   = 240;
	s3c_lcd->var.bits_per_pixel = 32;  //BPP: 每个色素使用多少位来表示其颜色

	//RGB : 5:6:5
	s3c_lcd->var.red.offset     = 16;
	s3c_lcd->var.red.length     = 8;

	s3c_lcd->var.green.offset   = 8;
	s3c_lcd->var.green.length   = 8;

	s3c_lcd->var.blue.offset    = 0;
	s3c_lcd->var.blue.length    = 8;

	s3c_lcd->var.activate       = FB_ACTIVATE_NOW;  //其他使用默认值，或不用设置
		
	//2.3设置可操作函数
	s3c_lcd->fbops              = &s3c_lcdfb_ops;
	//s3c_lcd->screen_base =       //显存的虚拟地址
	s3c_lcd->screen_size        = 320*240*32/8;
	
	//2.4其他的设置
	s3c_lcd->pseudo_palette = pseudo_pal;    //为了兼容以前的程序，还要写假的调色板，因为对于8bpp，才需要调色板

	//3.硬件相关的设置
	//3.1配置GPIO，用于LCD
	gpccon = ioremap(0x56000020, 4);  //不会只映射4个字节，而是映射一页
	gpdcon = ioremap(0x56000030, 4);
	gpgcon = ioremap(0x56000060, 4);

	*gpccon = 0xaaaaaaaa; //PIO管脚用于LEDN,VCLK,VFRAME,VM,USE_EN,LCDVF1,LCDVF2,LCD_PWR,VD[7:0]
	*gpdcon = 0xaaaaaaaa; //GPIO管脚用于VD[23:8] 

	*gpgcon |= (3<<(4*2));  //GPG4用作LCD_PWEREN,电源使能
		 
	//3.2根据LCD手册控制LCD控制器，比如VCLK的频率
	lcd_regs = ioremap(0x4d000000, sizeof(struct lcd_regs));

	/* 
	 * MINI2440 LCD 3.5英寸 ZQ3506_V0 SPEC.pdf 第11、12页
	 * 
	 * LCD手册11,12页和2440手册"Figure 15-6. TFT LCD Timing Example"一对比就知道参数含义了
	 */
	/* bit[17:8]: VCLK = HCLK / [(VLKVAL+1) * 2],LCD手册11，(DCLK=614MHZ~11MHz)
	 *             7.1MHz = 100MHz / [(CLKVAL+1) * 2]
	 *             CLKVAL = 6
	 * bit[6:5]: 0b11  TFT LCD
	 * bit[4:1]: 0b1100 16bpp for TFT
     * bit[0]  : 0 = Disable the video output and the LCD control signal         
     * 
	 */
	 lcd_regs->lcdcon1 = (6<<8) | (3<<5) | (0xd<<1) | (0<<0);

	/* 垂直方向的时间参数
	 * 根据数据手册
	 * bit[31:24]: VBPD, VSYNC之后再过多长时间才能发出第1行数据
	 *             LCD手册，tvb = 18
	 *             VBPD = 17;
	 * bit[23:14]: 多少行,240,所以LINEVAL = 240-1 = 239
	 * bit[13:6]: VFPD,  发出最后一行数据之后，再过多长时间才发出VSYNC
	 *            LCD手册, tvf = 4, 所以VFPD = 4-1 = 3
	 * bit[5:0]: VSPW, VSYNC信号的脉冲宽度, 手册tvp=1, VSPW = 1-1 = 0
	 */
	 
	/* 使用这些数值, 图像有下移的现象, 应该是数据手册过时了
	 * 自己微调一下, 上下移动调VBPD和VFPD
	 * 保持(VBPD+VFPD)不变, 减小VBPD图像上移, 取VBPD=11, VFPD=9
	 */
	 //lcd_regs->lcdcon2 = (17<<24) | (239<<14) | (3<<6) | (0<<0);
	lcd_regs->lcdcon2 = (11<<24) | (239<<14) | (9<<6) | (0<<0);

   /* 水平方向的时间参数
	* bit[25:19]: HBPD, HSYNC之后再过多长时间才能发出第1个像素的数据
	*			  LCD手册，tv = 18
	*			  VBPD = 17;
	* bit[18:8]: 多少列,320,所以HOZVAL = 320-1 = 319
	* bit[7:0]: HFPD,	发出最后一个像素之后，再过多长时间才发出HSYNC
	*			 LCD手册, thf>=2, th=408  th=thp+thb+320+thf, thf=49, HFPD=49-1=48
	*/

//  lcd_regs->lcdcon3 = (37<<19) | (319<<8) | (48<<0);
    lcd_regs->lcdcon3 = (69<<19) | (319<<8) | (16<<0);


	/* 水平方向同步信号
	 * bit[7:0]: HSPW, HSYNC信号的脉冲宽度，lcd手册thp=1, 所以HSPW=1-1=0
	 */
	 lcd_regs->lcdcon4 = 0;

	/* 信号的极性
	 * bit[11]: 1 = 565 format
	 * bit[10]: 0 = The video data is fetched at VCLK falling edge 
	 * bit[9] : 1 = HSYNC信号要反转，即低电平有效
	 * bit[8] : 1 = YSYNC信号要反转，即低电平有效
	 * bit[7:4]: 不需要反转
	 * bit[3] : 0 = PWREN输出0，先不使能
	 * bit[1] : 0 = BSWP
	 * bit[0] : 1 = HWSWP 2440手册P413
	 */ 
	 lcd_regs->lcdcon5 = (0<<10) | (1<<9) | (1<<8) | (0<<12) | (0<<1) | (0<<0);
	//lcd_regs->lcdcon5 = (1<<11) | (1<<9) | (1<<8) | (1<<0);
	
	//3.3分配显存framebuffer，并把地址告诉LCD控制器
	s3c_lcd->screen_base = dma_alloc_writecombine(NULL, s3c_lcd->fix.smem_len, &s3c_lcd->fix.smem_start, GFP_KERNEL);

	/* LCDSADDR1
	 * LCDBANK  bit[29:21] 用来保存帧内存起始地址A[30:22],帧内存的起始地址必须为4MB对齐
	 * LCDBASEU bit[20:0]  对于TFT LCD，用来保存视口所对应的内存起始地址A[21:1]
	 */
	//lcd_regs->lcdsddr1 = (s3c_lcd->fix.smem_start << 1) & ~(3<<30);
	//这里面的意思是,保存A[30:1],那么bit31, bit0位是不要的,右移后,bit0去掉了,~(3<<30)就把原来的bit31去掉
	lcd_regs->lcdsaddr1 = (s3c_lcd->fix.smem_start >> 1) & ~(3<<30);
	
	/* LCDSADDR2
	 * LCDBASEL = ((the frame end address)>>1) + 1
     *	       = LCDBASEL + (PAGEWIDTH+OFFSIZE)*(LINEVAL+1)
     */
    lcd_regs->lcdsaddr2 = ((s3c_lcd->fix.smem_start+s3c_lcd->fix.smem_len)>>1) & 0x1fffff;

	/* OFFSIZE   bit[21:11]  差值为0
	 * PAGEWIDTH bit[10:0]   视品(view point)的宽度，以半字为单位
	 */
	lcd_regs->lcdsaddr3 = 320*32/16; //一行的长度(单位: 2字节)
 
	//s3c_lcd->fix.smem_start = xxx//显存的物理地址

	//启动LCD
	lcd_regs->lcdcon1 |= (1<<0); //使能LCD控制器
	lcd_regs->lcdcon5 |= (1<<3); //使能LCD本身，LCD_PWREN
	                             //MINI2440的背光电路是通过LCD_PWREN来控制的，不需要单独的背光引脚

	//4.注册
	ret = register_framebuffer(s3c_lcd);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n",
			ret);
		return -EINVAL;
	}
	
	return 0;
}

static void lcd_exit(void)
{
	unregister_framebuffer(s3c_lcd);
	lcd_regs->lcdcon1 &= ~(1<<0);  //关闭LCD控制器
	lcd_regs->lcdcon5 &= ~(1<<3);  //关闭LCD本身

	dma_free_writecombine(NULL, s3c_lcd->fix.smem_len, s3c_lcd->screen_base, s3c_lcd->fix.smem_start);
		                                              //   虚拟地址               物理地址
	iounmap(lcd_regs);
	iounmap(gpgcon);
	iounmap(gpccon);
	iounmap(gpdcon);
	
	framebuffer_release(s3c_lcd);
}


module_init(lcd_init);
module_exit(lcd_exit);

MODULE_LICENSE("GPL");

