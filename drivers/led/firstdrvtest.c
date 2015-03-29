
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  //那是是字母'l'不是数字1，写数字就错了
#include <stdio.h>

/*
 * firstdrvtest on      //two argument
 * firstdrvtest off
 */

int main(int argc,char **argv)
{
	int fd;
	int val = 1;
	fd = open("/dev/zfw",O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}
	if (argc != 2)
	{
		printf("Usage :\n");
		printf("%s <on|off>\n",argv[0]);  //print: firstdrvtest <on|off>
		return 0;  //finish procedure 
	}
	if (strcmp(argv[1],"on") == 0)  //campare right,then return 0
	{
		val = 1;   //led on
	}
	else
	{
		val = 0;   //led off
	}
	write(fd, &val, 4);
	return 0;
}



