#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// 信号处理函数
void sig_usr(int signo)
{
    // 这里也malloc，这是错用，不可重入函数不能用在信号处理函数中
    int *p = (int *)malloc(sizeof(int)); // 用了不可重入函数
    free(p);

    if (signo == SIGUSR1)
        printf("收到了SIGUSR1信号!\n");
    else if (signo == SIGUSR2)
        printf("收到了SIGUSR2信号!\n");
    else
        printf("收到了未捕捉的信号%d!\n", signo);
}

int main(int argc, char *const *argv)
{
    if (signal(SIGUSR1, sig_usr) == SIG_ERR)
        printf("无法捕捉SIGUSR1信号!\n");

    if (signal(SIGUSR2, sig_usr) == SIG_ERR)
        printf("无法捕捉SIGUSR2信号!\n");

    while (1)
    {
        // sleep(1); //休息1秒
        // printf("休息1秒\n");
        int *p = (int *)malloc(sizeof(int));
        free(p);
    }

    printf("再见!\n");
    return 0;
}
