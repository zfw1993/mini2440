
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


/*
 *  Ӧ�ó��򲻻��Զ���ȥ��ȡ��ֵ
 *  ���������жϷ�����buttons_irq�У���������а��������ˣ�
 *  �ͻ��Ӧ�ó�����һ���ź�kill_fasync(&button_async, SIGIO, POLL_IN);
 *  ����źžͻᴥ��Ӧ�ó��������������źŴ�����signal
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
	unsigned char key_val;
	int ret;

	//signal(SIGIO, signal_fun);   //Ӧ�ó���ע���źŴ�����
	
//	fd = open("/dev/buttons",O_RDWR | O_NONBLOCK);  //�򿪵��豸     ��������ʽ
	fd = open("/dev/buttons",O_RDWR);               //�򿪵��豸     ������ʽ
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}

//	fcntl(fd, F_SETOWN, getpid());   //�����ںˣ�����˭ ��������pid(���̺�)������������
//	Oflags = fcntl(fd, F_GETFL);   //����Oflag 
//	fcntl(fd, F_SETFL, Oflags | FASYNC); // �ı�fasync��ǣ����ջ���õ�������faync > fasync_helper����ʼ��/�ͷ�fasync_struct

	while (1)  
	{	
		ret = read(fd, &key_val, 1);
		printf("key_val: 0x%x, ret = %d\n", key_val, ret);
//		sleep(5);    //��λ����
	}

	return 0;
}



