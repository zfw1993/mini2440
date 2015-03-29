
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <stdio.h>
#include <poll.h>


/*
struct pollfd {
			  int	fd; 		// file descriptor   �ļ�����
			  short events; 	// requested events  �ڴ����¼�
			  short revents;	// returned events   �����¼�
		  };
*/

int main(int argc,char **argv)
{
	int fd;
	unsigned char key_val;
	int ret;               //����ֵ
	struct pollfd fds[1];  //ֻ��ѯһ��
	
	fd = open("/dev/buttons",O_RDWR);  //�򿪵��豸
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	fds[0].fd     = fd;       //�򿪵��豸��fd,���Բ�ѯ����豸
	fds[0].events = POLLIN;   //POLLIN There is data to read. ��ʾ�����ݵȴ���ȡ
							  //������Ǳ�ʾ���ڴ�ʲô�¼��أ�����POLLIN���ͱ�ʾ�ڴ������ݶ�ȡ���¼�����

	//�������һ��ʱ���ڣ�û�����鷢�����ͷ��أ���ô�͵���poll����
	while (1)  //һ��ʱ���ڣ����û�а������£��ͷ���
	{
		ret = poll(fds, 1, 5000); //��ѯһ���¼�������ʱ���Ժ���Ϊ��λ
		if (0 == ret)        //����ֵΪ0����ʾ��ʱ
		{
			printf("time out!\n");
		}
		else
		{
			read(fd, &key_val, 1);	
			printf("key_val = 0x%x\n", key_val);      //%x : ��16�������

		}
	}

	return 0;
}



