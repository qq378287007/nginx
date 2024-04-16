#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ngx_c_conf.h" //和配置文件处理相关的类,名字带c_表示和类有关
#include "ngx_setproctitle.h"

int main(int argc, char **argv)
{
    printf("--------------------------------------------------------\n");
    // 验证argv指向的内存和environ指向的内存紧挨着
    for (int i = 0; i < argc; i++)
    {
        printf("argv[%02d]地址=%x    ", i, (unsigned int)((unsigned long)argv[i]));
        printf("argv[%02d]内容=%s\n", i, argv[i]);
    }
    for (int i = 0; environ[i]; i++)
    {
        printf("environ[%02d]地址=%x    ", i, (unsigned int)((unsigned long)environ[i]));
        printf("environ[%02d]内容=%s\n", i, environ[i]);
    }
    printf("--------------------------------------------------------");

    ngx_init_setproctitle(argc, argv); // argv+environ变量搬家

    for (int i = 0; i < argc; i++)
    {
        printf("argv[%02d]地址=%x    ", i, (unsigned int)((unsigned long)argv[i]));
        printf("argv[%02d]内容=%s\n", i, argv[i]);
    }
    for (int i = 0; environ[i]; i++)
    {
        printf("environ[%02d]地址=%x    ", i, (unsigned int)((unsigned long)environ[i]));
        printf("environ[%02d]内容=%s\n", i, environ[i]);
    }
    printf("--------------------------------------------------------\n");

    // 要保证所有命令行参数从下面这行代码开始都不再使用，才能调用ngx_setproctitle函数，因为调用后，命令行参数的内容可能会被覆盖掉
    ngx_setproctitle("nginx: master process");
    sleep(10);
    ngx_free_setproctitle();

    // 我们在main中，先把配置读出来，供后续使用
    CConfig *p_config = CConfig::GetInstance(); // 单例类
    if (p_config->Load("nginx.conf") == false)
    { // 把配置文件内容载入到内存，配置文件与可执行程序在同一个目录，所以可以直接载入配置文件
        printf("配置文件载入失败，退出!\n");
        exit(1);
    }

    // 获取配置文件信息的用法
    int port = p_config->GetIntDefault("ListenPort", 0); // 0是缺省值
    printf("port=%d\n", port);
    const char *pDBInfo = p_config->GetString("DBInfo");
    if (pDBInfo != NULL)
        printf("DBInfo=%s\n", pDBInfo);
    printf("--------------------------------------------------------\n");

    printf("程序退出，再见!\n");
    return 0;
}

//g++ *.cxx & ./a.out
//ps -ef | grep master