
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

	//��red,green,blue��ԭɫ�����val
	val  = chan_to_field(red,   &info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,  &info->var.blue);

	((u32 *)(info->pseudo_palette))[regno] = val;		
	//pseudo_pal[regno] = val; //���߿�����ôд
	
	return 0;
}


static struct fb_ops s3c_lcdfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= s3c_lcdfb_setcolreg,  //���üٵĵ�ɫ��
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static volatile unsigned long *gpccon;
static volatile unsigned long *gpgcon;
static volatile unsigned long *gpdcon;
//static volatile unsigned long *gpddat;

static volatile struct lcd_regs *lcd_regs;
static struct fb_info *s3c_lcd;             //����һ���ṹ��ָ��
//static volatile struct fb_info *s3c_lcd;  //��volatile�Ż�֮�󣬳���warning

static u32	pseudo_pal[16];

static int lcd_init(void)
{
	int ret;
	
	//1.����һ��fbinfo
	s3c_lcd = framebuffer_alloc(0, NULL); //����Ҫ����Ŀռ䣬��Ҫ�жϷ���ֵ����һ�ڴ治�������䲻�ɹ���
	if (!s3c_lcd)
	return -ENOMEM;   
	
	//2.����            
	//2.1���ù̶��Ĳ���
	strcpy(s3c_lcd->fix.id, "mylcd");
	s3c_lcd->fix.smem_len    = 320*240*32/8; //mini2440��LCDλ����24������2440������4�ֽڼ�32λ(�˷�1�ֽ�)
	s3c_lcd->fix.type        = FB_TYPE_PACKED_PIXELS;  //���ų���������һ��
	s3c_lcd->fix.visual      = FB_VISUAL_TRUECOLOR;  //TFT ���ɫ
	s3c_lcd->fix.line_length = 320*4;           //����ʹ��Ĭ��ֵ
		
	//2.2���ÿɱ�Ĳ���
	s3c_lcd->var.xres           = 320;
	s3c_lcd->var.yres           = 240;
	s3c_lcd->var.xres_virtual   = 320;
	s3c_lcd->var.yres_virtual   = 240;
	s3c_lcd->var.bits_per_pixel = 32;  //BPP: ÿ��ɫ��ʹ�ö���λ����ʾ����ɫ

	//RGB : 5:6:5
	s3c_lcd->var.red.offset     = 16;
	s3c_lcd->var.red.length     = 8;

	s3c_lcd->var.green.offset   = 8;
	s3c_lcd->var.green.length   = 8;

	s3c_lcd->var.blue.offset    = 0;
	s3c_lcd->var.blue.length    = 8;

	s3c_lcd->var.activate       = FB_ACTIVATE_NOW;  //����ʹ��Ĭ��ֵ����������
		
	//2.3���ÿɲ�������
	s3c_lcd->fbops              = &s3c_lcdfb_ops;
	//s3c_lcd->screen_base =       //�Դ�������ַ
	s3c_lcd->screen_size        = 320*240*32/8;
	
	//2.4����������
	s3c_lcd->pseudo_palette = pseudo_pal;    //Ϊ�˼�����ǰ�ĳ��򣬻�Ҫд�ٵĵ�ɫ�壬��Ϊ����8bpp������Ҫ��ɫ��

	//3.Ӳ����ص�����
	//3.1����GPIO������LCD
	gpccon = ioremap(0x56000020, 4);  //����ֻӳ��4���ֽڣ�����ӳ��һҳ
	gpdcon = ioremap(0x56000030, 4);
	gpgcon = ioremap(0x56000060, 4);

	*gpccon = 0xaaaaaaaa; //PIO�ܽ�����LEDN,VCLK,VFRAME,VM,USE_EN,LCDVF1,LCDVF2,LCD_PWR,VD[7:0]
	*gpdcon = 0xaaaaaaaa; //GPIO�ܽ�����VD[23:8] 

	*gpgcon |= (3<<(4*2));  //GPG4����LCD_PWEREN,��Դʹ��
		 
	//3.2����LCD�ֲ����LCD������������VCLK��Ƶ��
	lcd_regs = ioremap(0x4d000000, sizeof(struct lcd_regs));

	/* 
	 * MINI2440 LCD 3.5Ӣ�� ZQ3506_V0 SPEC.pdf ��11��12ҳ
	 * 
	 * LCD�ֲ�11,12ҳ��2440�ֲ�"Figure 15-6. TFT LCD Timing Example"һ�ԱȾ�֪������������
	 */
	/* bit[17:8]: VCLK = HCLK / [(VLKVAL+1) * 2],LCD�ֲ�11��(DCLK=614MHZ~11MHz)
	 *             7.1MHz = 100MHz / [(CLKVAL+1) * 2]
	 *             CLKVAL = 6
	 * bit[6:5]: 0b11  TFT LCD
	 * bit[4:1]: 0b1100 16bpp for TFT
     * bit[0]  : 0 = Disable the video output and the LCD control signal         
     * 
	 */
	 lcd_regs->lcdcon1 = (6<<8) | (3<<5) | (0xd<<1) | (0<<0);

	/* ��ֱ�����ʱ�����
	 * ���������ֲ�
	 * bit[31:24]: VBPD, VSYNC֮���ٹ��೤ʱ����ܷ�����1������
	 *             LCD�ֲᣬtvb = 18
	 *             VBPD = 17;
	 * bit[23:14]: ������,240,����LINEVAL = 240-1 = 239
	 * bit[13:6]: VFPD,  �������һ������֮���ٹ��೤ʱ��ŷ���VSYNC
	 *            LCD�ֲ�, tvf = 4, ����VFPD = 4-1 = 3
	 * bit[5:0]: VSPW, VSYNC�źŵ�������, �ֲ�tvp=1, VSPW = 1-1 = 0
	 */
	 
	/* ʹ����Щ��ֵ, ͼ�������Ƶ�����, Ӧ���������ֲ��ʱ��
	 * �Լ�΢��һ��, �����ƶ���VBPD��VFPD
	 * ����(VBPD+VFPD)����, ��СVBPDͼ������, ȡVBPD=11, VFPD=9
	 */
	 //lcd_regs->lcdcon2 = (17<<24) | (239<<14) | (3<<6) | (0<<0);
	lcd_regs->lcdcon2 = (11<<24) | (239<<14) | (9<<6) | (0<<0);

   /* ˮƽ�����ʱ�����
	* bit[25:19]: HBPD, HSYNC֮���ٹ��೤ʱ����ܷ�����1�����ص�����
	*			  LCD�ֲᣬtv = 18
	*			  VBPD = 17;
	* bit[18:8]: ������,320,����HOZVAL = 320-1 = 319
	* bit[7:0]: HFPD,	�������һ������֮���ٹ��೤ʱ��ŷ���HSYNC
	*			 LCD�ֲ�, thf>=2, th=408  th=thp+thb+320+thf, thf=49, HFPD=49-1=48
	*/

//  lcd_regs->lcdcon3 = (37<<19) | (319<<8) | (48<<0);
    lcd_regs->lcdcon3 = (69<<19) | (319<<8) | (16<<0);


	/* ˮƽ����ͬ���ź�
	 * bit[7:0]: HSPW, HSYNC�źŵ������ȣ�lcd�ֲ�thp=1, ����HSPW=1-1=0
	 */
	 lcd_regs->lcdcon4 = 0;

	/* �źŵļ���
	 * bit[11]: 1 = 565 format
	 * bit[10]: 0 = The video data is fetched at VCLK falling edge 
	 * bit[9] : 1 = HSYNC�ź�Ҫ��ת�����͵�ƽ��Ч
	 * bit[8] : 1 = YSYNC�ź�Ҫ��ת�����͵�ƽ��Ч
	 * bit[7:4]: ����Ҫ��ת
	 * bit[3] : 0 = PWREN���0���Ȳ�ʹ��
	 * bit[1] : 0 = BSWP
	 * bit[0] : 1 = HWSWP 2440�ֲ�P413
	 */ 
	 lcd_regs->lcdcon5 = (0<<10) | (1<<9) | (1<<8) | (0<<12) | (0<<1) | (0<<0);
	//lcd_regs->lcdcon5 = (1<<11) | (1<<9) | (1<<8) | (1<<0);
	
	//3.3�����Դ�framebuffer�����ѵ�ַ����LCD������
	s3c_lcd->screen_base = dma_alloc_writecombine(NULL, s3c_lcd->fix.smem_len, &s3c_lcd->fix.smem_start, GFP_KERNEL);

	/* LCDSADDR1
	 * LCDBANK  bit[29:21] ��������֡�ڴ���ʼ��ַA[30:22],֡�ڴ����ʼ��ַ����Ϊ4MB����
	 * LCDBASEU bit[20:0]  ����TFT LCD�����������ӿ�����Ӧ���ڴ���ʼ��ַA[21:1]
	 */
	//lcd_regs->lcdsddr1 = (s3c_lcd->fix.smem_start << 1) & ~(3<<30);
	//���������˼��,����A[30:1],��ôbit31, bit0λ�ǲ�Ҫ��,���ƺ�,bit0ȥ����,~(3<<30)�Ͱ�ԭ����bit31ȥ��
	lcd_regs->lcdsaddr1 = (s3c_lcd->fix.smem_start >> 1) & ~(3<<30);
	
	/* LCDSADDR2
	 * LCDBASEL = ((the frame end address)>>1) + 1
     *	       = LCDBASEL + (PAGEWIDTH+OFFSIZE)*(LINEVAL+1)
     */
    lcd_regs->lcdsaddr2 = ((s3c_lcd->fix.smem_start+s3c_lcd->fix.smem_len)>>1) & 0x1fffff;

	/* OFFSIZE   bit[21:11]  ��ֵΪ0
	 * PAGEWIDTH bit[10:0]   ��Ʒ(view point)�Ŀ�ȣ��԰���Ϊ��λ
	 */
	lcd_regs->lcdsaddr3 = 320*32/16; //һ�еĳ���(��λ: 2�ֽ�)
 
	//s3c_lcd->fix.smem_start = xxx//�Դ�������ַ

	//����LCD
	lcd_regs->lcdcon1 |= (1<<0); //ʹ��LCD������
	lcd_regs->lcdcon5 |= (1<<3); //ʹ��LCD����LCD_PWREN
	                             //MINI2440�ı����·��ͨ��LCD_PWREN�����Ƶģ�����Ҫ�����ı�������

	//4.ע��
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
	lcd_regs->lcdcon1 &= ~(1<<0);  //�ر�LCD������
	lcd_regs->lcdcon5 &= ~(1<<3);  //�ر�LCD����

	dma_free_writecombine(NULL, s3c_lcd->fix.smem_len, s3c_lcd->screen_base, s3c_lcd->fix.smem_start);
		                                              //   �����ַ               �����ַ
	iounmap(lcd_regs);
	iounmap(gpgcon);
	iounmap(gpccon);
	iounmap(gpdcon);
	
	framebuffer_release(s3c_lcd);
}


module_init(lcd_init);
module_exit(lcd_exit);

MODULE_LICENSE("GPL");

