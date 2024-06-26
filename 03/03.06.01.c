#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// 信号处理函数
void sig_usr(int signo)
{
    switch (signo)
    {
    case SIGUSR1:
        printf("收到了SIGUSR1信号，进程id=%d!\n", getpid());
        return;
    case SIGCHLD:
        printf("收到了SIGCHLD信号，进程id=%d!\n", getpid());

        // 这里学了一个新函数waitpid，有人也用wait,但笔者要求大家掌握和使用waitpid即可
        // 这个waitpid说白了获取子进程的终止状态，这样，子进程就不会成为僵尸进程了
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG); // 第一个参数为-1，表示等待任何子进程
                                                   // 第二个参数：保存子进程的状态信息(大家如果想详细了解，可以搜索一下)
                                                   // 第三个参数：提供额外选项，WNOHANG表示不要阻塞，让这个waitpid()立即返回
        if (pid == 0)                              // 子进程没结束，会立即返回这个数字，但这里应该不是这个数字，这里的情况是子进程结束才触发父进程的该信号
            return;
        if (pid == -1) // 这表示这个waitpid调用有错误，有错误也立即返回，管不了这么多
            return;
        // 走到这里，表示  成功，那也return吧
        return;
    }
}

int main(int argc, char *const *argv)
{
    printf("进程开始执行!\n");

    // 先简单处理一个信号
    if (signal(SIGUSR1, sig_usr) == SIG_ERR)
    {
        printf("无法捕捉SIGUSR1信号!\n");
        exit(1);
    }

    if (signal(SIGCHLD, sig_usr) == SIG_ERR)
    {
        printf("无法捕捉SIGCHLD信号!\n");
        exit(1);
    }

    pid_t pid = fork(); // 创建一个子进程

    // 要判断子进程是否创建成功
    if (pid < 0)
    {
        printf("子进程创建失败，很遗憾!\n");
        exit(1);
    }

    // 现在，父进程和子进程同时开始 运行了
    while (1)
    {
        sleep(1); // 休息1秒
        printf("休息1秒，进程id=%d!\n", getpid());
    }

    printf("再见了!\n");
    return 0;
}
