
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


/*
 *  应用程序不会自动地去读取键值
 *  在驱动的中断服务函数buttons_irq中，如果发现有按键按下了，
 *  就会给应用程序发送一个信号kill_fasync(&button_async, SIGIO, POLL_IN);
 *  这个信号就会触发应用程序来调用它的信号处理函数signal
 */


int fd;

void signal_fun(int signum)
{
	unsigned char key_val;
	
	read(fd, &key_val, 1);
	printf("key_val: 0x%x\n", key_val);
}


int main(int argc,char **argv)
{
	int Oflags;

	signal(SIGIO, signal_fun);   //应用程序注册信号处理函数
	
	fd = open("/dev/buttons",O_RDWR);  //打开的设备
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	fcntl(fd, F_SETOWN, getpid());   //告诉内核，发给谁 ，把它的pid(进程号)告诉驱动程序
	Oflags = fcntl(fd, F_GETFL);     //读出Oflag 
	fcntl(fd, F_SETFL, Oflags | FASYNC); // 改变fasync标记，最终会调用到驱动的faync > fasync_helper：初始化/释放fasync_struct

	while (1)  
	{
		sleep(1000);
	}

	return 0;
}



