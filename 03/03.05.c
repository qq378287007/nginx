#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// 信号处理函数
void sig_quit(int signo)
{
    printf("收到了SIGQUIT信号!\n");

    if (signal(SIGQUIT, SIG_DFL) == SIG_ERR)
    {
        printf("无法为SIGQUIT信号设置缺省处理!\n");
        exit(1);
    }
}

int main(int argc, char *const *argv)
{
    if (signal(SIGQUIT, sig_quit) == SIG_ERR) // 注册信号对应的信号处理函数
    {
        printf("无法捕捉SIGUSR1信号!\n");
        exit(1); // 退出程序，参数是错误代码，0表示正常退出，非0表示错误，但具体什么错误，没有特别规定，这个错误代码一般也用不到，先不管它
    }

    sigset_t newmask;             // 信号集
    sigemptyset(&newmask);        // 信号集中信号清0（所有信号都不屏蔽，能接收）
    sigaddset(&newmask, SIGQUIT); // 设置SIGQUIT信号位为1，该信号被阻塞

    // 设置该进程所对应的信号集
    sigset_t oldmask;
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) // 第一个参数用了SIG_BLOCK表明设置 进程 新的信号屏蔽字 为 “当前信号屏蔽字 和 第二个参数指向的信号集的并集
    {                                                   // 一个 ”进程“ 的当前信号屏蔽字，刚开始全部都是0的；所以相当于把当前 "进程"的信号屏蔽字设置成 newmask（屏蔽了SIGQUIT)；
                                                        // 第三个参数不为空，则进程老的(调用本sigprocmask()之前的)信号集会保存到第三个参数里，用于后续，这样后续可以恢复老的信号集给进程
        printf("sigprocmask(SIG_BLOCK)失败!\n");
        exit(1);
    }

    printf("我要开始休息10秒了--------begin--，此时我无法接收SIGQUIT信号!\n");
    sleep(10); // 这个期间无法收到SIGQUIT信号的
    printf("我已经休息了10秒了--------end----!\n");

    if (sigismember(&newmask, SIGQUIT)) // 测试一个指定的信号位是否被置位，这里测试的是newmask
        printf("SIGQUIT信号被屏蔽了!\n");
    else
        printf("SIGQUIT信号没有被屏蔽!!!!!!\n");

    if (sigismember(&newmask, SIGHUP)) // 测试另外一个指定的信号位是否被置位,测试的是newmask
        printf("SIGHUP信号被屏蔽了!\n");
    else
        printf("SIGHUP信号没有被屏蔽!!!!!!\n");

    // 现在我要取消对SIGQUIT信号的屏蔽(阻塞)--把信号集还原回去
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) // SIGSETMASK设置进程信号屏蔽字为oldmask
    {
        printf("sigprocmask(SIG_SETMASK)失败!\n");
        exit(1);
    }

    printf("sigprocmask(SIG_SETMASK)成功!\n");

    if (sigismember(&oldmask, SIGQUIT)) // 测试一个指定的信号位是否被置位,这里测试的是oldmask
    {
        printf("SIGQUIT信号被屏蔽了!\n");
    }
    else
    {
        printf("SIGQUIT信号没有被屏蔽，您可以发送SIGQUIT信号了，我要sleep(10)秒钟!!!!!!\n");
        int mysl = sleep(10);
        if (mysl > 0)
            printf("sleep还没睡够，剩余%d秒\n", mysl);
    }
    printf("再见了!\n");
    return 0;
}
