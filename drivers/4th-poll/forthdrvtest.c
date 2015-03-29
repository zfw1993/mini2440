
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <stdio.h>
#include <poll.h>


/*
struct pollfd {
			  int	fd; 		// file descriptor   文件描述
			  short events; 	// requested events  期待的事件
			  short revents;	// returned events   返回事件
		  };
*/

int main(int argc,char **argv)
{
	int fd;
	unsigned char key_val;
	int ret;               //返回值
	struct pollfd fds[1];  //只查询一个
	
	fd = open("/dev/buttons",O_RDWR);  //打开的设备
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	fds[0].fd     = fd;       //打开的设备是fd,所以查询这个设备
	fds[0].events = POLLIN;   //POLLIN There is data to read. 表示有数据等待读取
							  //这里就是表示你期待什么事件呢，填上POLLIN，就表示期待有数据读取的事件发生

	//如果想在一定时间内，没有事情发生，就返回，那么就得用poll机制
	while (1)  //一定时间内，如果没有按键按下，就返回
	{
		ret = poll(fds, 1, 5000); //查询一个事件，休眠时间以毫秒为单位
		if (0 == ret)        //返回值为0，表示超时
		{
			printf("time out!\n");
		}
		else
		{
			read(fd, &key_val, 1);	
			printf("key_val = 0x%x\n", key_val);      //%x : 以16进制输出

		}
	}

	return 0;
}



