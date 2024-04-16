#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ngx_c_conf.h"
#include "ngx_log.h"
#include "ngx_signal.h"
#include "ngx_setproctitle.h"
#include "ngx_process_cycle.h"

// 和进程本身有关的全局量
pid_t ngx_pid;    // 当前进程的pid
pid_t ngx_parent; // 父进程的pid

static void freeresource()
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
    ngx_pid = getpid();     // 取得进程pid
    ngx_parent = getppid(); // 取得父进程的id

    //(2)初始化失败，就要直接退出的
    // 配置文件必须最先要，后边初始化啥的都用，所以先把配置读出来，供后续使用
    CConfig *p_config = CConfig::GetInstance(); // 单例类
    if (p_config->Load("nginx.conf") == false)  // 把配置文件内容载入到内存
    {
        ngx_log_stderr(0, "配置文件[%s]载入失败，退出!", "nginx.conf");
        // exit(0)正常, exit(1)/exit(-1)异常，exit(2)系统找不到指定的文件
        exitcode = 2;
        goto lblexit;
    }

    //(3)一些初始化函数，准备放这里
    ngx_log_init();              // 日志初始化(创建/打开日志文件)
    if (ngx_init_signals() != 0) // 信号初始化
    {
        exitcode = 1;
        goto lblexit;
    }

    //(4)一些不好归类的其他类别的代码，准备放这里
    ngx_init_setproctitle(argc, argv); // 把环境变量搬家

    //(5)开始正式的主工作流程，主流程一致在下边这个函数里循环，暂时不会走下来，资源释放啥的日后再慢慢完善和考虑
    ngx_master_process_cycle(); // 不管父进程还是子进程，正常工作期间都在这个函数里循环；

    // for(;;)
    //{
    //    sleep(1); //休息1秒
    //    printf("休息1秒\n");
    //}
lblexit:
    //(5)该释放的资源要释放掉
    freeresource();

    printf("程序退出，再见!\n");
    return exitcode;
}

// g++ *.cxx & ./a.out
// ps -ef | grep "master"
// sudo kill -9 
