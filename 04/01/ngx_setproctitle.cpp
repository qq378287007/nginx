#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //environ

#include "ngx_global.h"
#include "ngx_setproctitle.h"

static char *g_argv;       // 指向原始argv+environ的内存空间
static int g_argv_len = 0; // 原始argv+environ占据内存字节数
static char *argv_environ; // 指向重新创建的argv+environ的内存空间

// 分配内存拷贝命令行参数和环境变量
void ngx_init_setproctitle(int argc, char **argv)
{
    g_argv = argv[0];
    for (int i = 0; i < argc; i++)
        g_argv_len += strlen(argv[i]) + 1; // 末尾'\0'占1个字节
    for (int i = 0; environ[i]; i++)
        g_argv_len += strlen(environ[i]) + 1; // 末尾'\0'占1个字节
    argv_environ = new char[g_argv_len];      // 新建内存空间放置argv+environ
    memset(argv_environ, 0, g_argv_len);

    char *ptmp = argv_environ;
    for (int i = 0; i < argc; i++)
    {
        size_t size = strlen(argv[i]) + 1; // strlen不包括字符串末尾'\0'
        strcpy(ptmp, argv[i]);             // 拷贝环境变量内容, '\0'会被拷贝
        argv[i] = ptmp;                    // 环境变量指针指向新内存
        ptmp += size;
    }
    for (int i = 0; environ[i]; i++)
    {
        size_t size = strlen(environ[i]) + 1;
        strcpy(ptmp, environ[i]);
        environ[i] = ptmp;
        ptmp += size;
    }
}

void ngx_setproctitle(const char *title)
{
    if (title == NULL)
        return;

    memset(g_argv, 0, g_argv_len);                        // 原有内存空间argv+environ清空
    for (int i = 0; i <= g_argv_len - 2 && title[i]; i++) // 更改原有内存空间argv+environ内容，实现进程标题更改
        g_argv[i] = title[i];
}

void ngx_free_setproctitle()
{
    if (argv_environ)
    {
        delete argv_environ;
        argv_environ = NULL;
    }
}

// argv和environ，占据一段连续内存空间，不能释放（估计系统会处理）
#ifdef TEST_NGX_SETPROCTITLE
int main(int argc, char **argv)
{
    ngx_init_setproctitle(argc, argv);
    ngx_setproctitle("change title");
    sleep(10);
    ngx_free_setproctitle();

    printf("over\n");
    return 0;
}
#endif

/*
g++ ngx_setproctitle.cxx -DTEST_NGX_SETPROCTITLE -o ngx_setproctitle &&./ngx_setproctitle
ps -ef | grep title
*/