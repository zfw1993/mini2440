
#include <stdio.h>
#include <signal.h>



void signal_fun(int signum)  //´¦Àíº¯Êý
{
	static int cnt = 0;
	printf("signal = %d, %d times\n",signum, ++cnt);
}

int main(int argc, char **argv)
{

	signal(SIGUSR1, signal_fun);  //SIGUSR1: 10  

	while (1)
	{
		sleep(1000);
	}
	
	return 0;
}

