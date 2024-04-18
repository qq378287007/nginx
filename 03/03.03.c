
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *const *argv)
{
    printf("非常高兴，和大家一起学习本书\n");

    while (1)
    {
        sleep(1);
        printf("进程休息1秒!\n");
    }

    printf("程序退出!再见!\n");
    return 0;
}
