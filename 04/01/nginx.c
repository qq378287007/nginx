#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <error.h>

void sig_usr(int signo)
{
	if (signo == SIGUSR1)
	{
		printf("收到了SIGUSR1信号，休息10s\n");
		sleep(10);
		printf("收到了SIGUSR1信号，休息结束\n");
	}
	else if (signo == SIGUSR2)
	{
		printf("收到了SIGUSR2信号，休息10s\n");
		sleep(10);
		printf("收到了SIGUSR2信号，休息结束\n");
	}
	else
	{
		printf("收到了未捕捉的信号%d\n", signo);
	}
}

int main()
{
	if (signal(SIGUSR1, sig_usr) == SIG_ERR)
		printf("无法捕捉SIGUSR1信号！\n");

	if (signal(SIGUSR2, sig_usr) == SIG_ERR)
		printf("无法捕捉SIGUSR2信号！\n");

	while (1)
	{
		sleep(1);
		printf("休息1s\n");
	}
	printf("再见\n");
	return 0;
}

/*
ps -eo pid,ppid,sid,tty,pgrp,comm,cmd | grep -E 'bash|PID|nginx'

sudo kill -USR1 PID
sudo kill -USR1 PID
sudo kill -USR1 PID

sudo kill -USR2 PID
sudo kill -USR1 PID
*/
