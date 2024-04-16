#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char *const *argv)
{
    printf("每个用户允许创建的最大进程数=%ld\n", sysconf(_SC_CHILD_MAX));

    fork(); // 一般fork都会成功所以不判断返回值了
    fork();

    for (;;)
    {
        sleep(1); // 休息1秒
        printf("休息1秒，进程id=%d!\n", getpid());
    }
    printf("再见了!\n");
    return 0;
}
