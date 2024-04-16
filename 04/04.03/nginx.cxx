#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ngx_c_conf.h" //和配置文件处理相关的类,名字带c_表示和类有关
#include "ngx_setproctitle.h"
#include "ngx_log.h"

// 和进程本身有关的全局量
pid_t ngx_pid; // 当前进程的pid

static void freeresource() // 专门在程序执行末尾释放资源的函数【一系列的main返回前的释放动作函数】
{
    //(1)对于因为设置可执行程序标题导致的环境变量分配的内存，我们应该释放
    ngx_free_setproctitle();

    //(2)关闭日志文件
    ngx_log_close();
}

int main(int argc, char **argv)
{
    int exitcode = 0; // 退出代码，先给0表示正常退出

    //(1)无伤大雅也不需要释放的放最上边
    ngx_pid = getpid(); // 取得进程pid

    //(2)初始化失败，就要直接退出的
    // 配置文件必须最先要，后边初始化啥的都用，所以先把配置读出来，供后续使用
    CConfig *p_config = CConfig::GetInstance(); // 单例类
    if (p_config->Load("nginx.conf") == false)  // 把配置文件内容载入到内存
    {
        ngx_log_stderr(0, "配置文件[%s]载入失败，退出!", "nginx.conf");
        // exit(1);终止进程，在main中出现和return效果一样 ,exit(0)表示程序正常, exit(1)/exit(-1)表示程序异常退出，exit(2)表示表示系统找不到指定的文件
        exitcode = 2; // 标记找不到文件
        goto lblexit;
    }

    //(3)一些初始化函数，准备放这里
    ngx_log_init(); // 日志初始化(创建/打开日志文件)

    //(4)一些不好归类的其他类别的代码，准备放这里
    ngx_init_setproctitle(argc, argv);        // 把环境变量搬家
    ngx_setproctitle("nginx master process"); // 设置可执行程序标题

    ngx_log_stderr(0, "invalid option: %s , %d", "testInfo", 326);
    ngx_log_stderr(0, "invalid option: \"%s\"", argv[0]);           // nginx: invalid option: "./nginx"
    ngx_log_stderr(0, "invalid option: %10d", 21);                  // nginx: invalid option:         21  ---21前面有8个空格
    ngx_log_stderr(0, "invalid option: %.6f", 21.378);              // nginx: invalid option: 21.378000   ---%.这种只跟f配合有效，往末尾填充0
    ngx_log_stderr(0, "invalid option: %.6f", 12.999);              // nginx: invalid option: 12.999000
    ngx_log_stderr(0, "invalid option: %.2f", 12.999);              // nginx: invalid option: 13.00
    ngx_log_stderr(0, "invalid option: %xd", 1678);                 // nginx: invalid option: 68e
    ngx_log_stderr(0, "invalid option: %Xd", 1678);                 // nginx: invalid option: 68E
    ngx_log_stderr(15, "invalid option: %s , %d", "testInfo", 326); // nginx: invalid option: testInfo , 326 (15: Block device required)
    ngx_log_stderr(0, "invalid option: %d", 1678);                  // nginx: invalid option: 1678

    ngx_log_error_core(5, 8, "这个XXX工作的有问题，显示的结果=%s", "YYYY");

    // for (;;)
    for (int i = 0; i < 10; ++i)
    {
        sleep(1); // 休息1秒
        printf("休息1秒\n");
    }

    //--------------------------------------
lblexit:
    //(5)该释放的资源要释放掉
    freeresource(); // 一系列的main返回前的释放动作函数

    printf("程序退出，再见!\n");
    return exitcode;
}

// g++ *.cxx & ./a.out
