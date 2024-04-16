
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
int main(int argc, char *const *argv)
{
    printf("非常高兴，和大家一起学习本书\n");

    //系统函数，设置某个信号来的时候的处理程序（用哪个函数来处理）
    signal(SIGHUP, SIG_IGN); //SIG_IGN代表 我要求忽略这个信号，请求操作系统不要执行缺省的该信号处理动作（也就是 不要把本进程杀掉）

    for(;;)
    {
        sleep(1);
        printf("休息1秒\n");
    }
    printf("程序退出!再见!\n");
    return 0;
}


/*
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
int main(int argc, char *const *argv)
{
    printf("非常高兴，和大家一起学习本书\n");

    //系统函数，设置某个信号来的时候的处理程序（用哪个函数来处理）
    //signal(SIGHUP, SIG_IGN); //SIG_IGN代表 我要求忽略这个信号，请求操作系统不要执行缺省的该信号处理动作（也就是 不要把本进程杀掉）
    setsid();
    for(;;)
    {
        sleep(1);
        printf("休息1秒\n");
    }
    printf("程序退出!再见!\n");
    return 0;
}
*/

/*
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
int main(int argc, char *const *argv)
{
    printf("非常高兴，和大家一起学习本书\n");

    pid_t pid = fork();  //系统函数 用来创建新进程，后续会讲解，子进程会从这句fork调用之后开始执行，原来的父进程也会接着往下执行
    if(pid < 0)
    {
        printf("fork进程出错!\n");
        return 1;
    }
    else if(pid == 0)
    {
        //若执行的是子进程，则这个条件就会满足
        printf("子进程开始执行!\n");

       // 创建新的session
        setsid();
        for(;;)
        {
            sleep(1);
            printf("子进程休息1秒!\n");
        }
        return 0;
    }
    else
    {
        //父进程才会走到这里
        for(;;)
        {
            sleep(1);
            printf("父进程休息1秒!\n");
        }
        return 0;
    }
}
*/
