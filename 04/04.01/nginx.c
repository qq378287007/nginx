#include <stdio.h>
#include "ngx_conf.h"
#include "ngx_signal.h"

int main(int argc, char *const *argv)
{
    printf("非常高兴, 我们大家一起学习《linux C++通信架构实战》\n");

    myconf();
    mysignal();

    printf("程序退出, 再见!\n");
    return 0;
}

// gcc *.c & a.exe
