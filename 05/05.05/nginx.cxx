
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "ngx_macro.h"
#include "ngx_c_conf.h"
#include "ngx_log.h"
#include "ngx_signal.h"
#include "ngx_setproctitle.h"
#include "ngx_process_cycle.h"
#include "ngx_daemon.h"
#include "ngx_c_socket.h"

// 守护进程标记，标记是否启用了守护进程模式，0：未启用，1：启用了
int g_daemonized = 0;

// socket相关
CSocekt g_socekt; // socket全局对象

// 和进程本身有关的全局量
pid_t ngx_pid;    // 当前进程的pid
pid_t ngx_parent; // 父进程的pid
int ngx_process;  // 进程类型，比如master,worker进程等

sig_atomic_t ngx_reap; // 标记子进程状态变化[一般是子进程发来SIGCHLD信号表示退出],sig_atomic_t:系统定义的类型：访问或改变这些变量需要在计算机的一条指令内完成
                       // 一般等价于int【通常情况下，int类型的变量通常是原子访问的，也可以认为 sig_atomic_t就是int类型的数据】

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

    // 全局量有必要初始化的
    ngx_process = NGX_PROCESS_MASTER; // 先标记本进程是master进程
    ngx_reap = 0;                     // 标记子进程没有发生变化

    //(2)初始化失败，就要直接退出的
    // 配置文件必须最先要，后边初始化啥的都用，所以先把配置读出来，供后续使用
    CConfig *p_config = CConfig::GetInstance(); // 单例类
    if (p_config->Load("nginx.conf") == false)  // 把配置文件内容载入到内存
    {
        ngx_log_init(); // 初始化日志
        ngx_log_stderr(0, "配置文件[%s]载入失败，退出!", "nginx.conf");
        // exit(1);终止进程，在main中出现和return效果一样 ,exit(0)表示程序正常, exit(1)/exit(-1)表示程序异常退出，exit(2)表示表示系统找不到指定的文件
        exitcode = 2; // 标记找不到文件
        goto lblexit;
    }

    //(3)一些必须事先准备好的资源，先初始化
    ngx_log_init(); // 日志初始化(创建/打开日志文件)，这个需要配置项，所以必须放配置文件载入的后边；

    //(4)一些初始化函数，准备放这里
    if (ngx_init_signals() != 0) // 信号初始化
    {
        exitcode = 1;
        goto lblexit;
    }
    if (g_socekt.Initialize() == false) // 初始化socket
    {
        exitcode = 1;
        goto lblexit;
    }

    //(5)一些不好归类的其他类别的代码，准备放这里
    ngx_init_setproctitle(argc, argv); // 把环境变量搬家

    //------------------------------------
    //(6)创建守护进程
    if (p_config->GetIntDefault("Daemon", 0) == 1) // 读配置文件，拿到配置文件中是否按守护进程方式启动的选项
    {
        // 1：按守护进程方式运行
        int cdaemonresult = ngx_daemon();
        if (cdaemonresult == -1) // fork()失败
        {
            exitcode = 1; // 标记失败
            goto lblexit;
        }
        if (cdaemonresult == 1)
        {
            // 这是原始的父进程
            freeresource(); // 只有进程退出了才goto到 lblexit，用于提醒用户进程退出了
                            // 而我现在这个情况属于正常fork()守护进程后的正常退出，不应该跑到lblexit()去执行，因为那里有一条打印语句标记整个进程的退出，这里不该限制该条打印语句；
            exitcode = 0;
            return exitcode; // 整个进程直接在这里退出
        }
        // 走到这里，成功创建了守护进程并且这里已经是fork()出来的进程，现在这个进程做了master进程
        g_daemonized = 1; // 守护进程标记，标记是否启用了守护进程模式，0：未启用，1：启用了
    }

    //(7)开始正式的主工作流程，主流程一致在下边这个函数里循环，暂时不会走下来，资源释放啥的日后再慢慢完善和考虑
    ngx_master_process_cycle(); // 不管父进程还是子进程，正常工作期间都在这个函数里循环；

    //--------------------------------------------------------------
    // for(;;)
    //{
    //    sleep(1); //休息1秒
    //    printf("休息1秒\n");
    //}

    //--------------------------------------
lblexit:
    //(5)该释放的资源要释放掉
    freeresource();

    ngx_log_stderr(0, "程序退出，再见了!");
    return exitcode;
}

// g++ *.cxx & ./a.out
// ps -ef | grep "master"
// sudo kill -9 
