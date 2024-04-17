#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //env

#include "ngx_setproctitle.h"

static char *g_argv = nullptr;		 // 指向原始argv+environ的内存空间
static int g_argv_len = 0;			 // 原始argv+environ占据内存字节数
static char *argv_environ = nullptr; // 指向重新创建的argv+environ的内存空间
// 分配内存拷贝命令行参数和环境变量
void ngx_init_setproctitle(int argc, char **argv)
{
	g_argv = argv[0];
	for (int i = 0; i < argc; i++)
		g_argv_len += strlen(argv[i]) + 1; // 末尾'\0'占1个字节
	for (int i = 0; environ[i]; i++)
		g_argv_len += strlen(environ[i]) + 1; // 末尾'\0'占1个字节
	argv_environ = new char[g_argv_len];	  // 新建内存空间放置argv+environ
	memset(argv_environ, 0, g_argv_len);

	char *ptmp = argv_environ;
	for (int i = 0; i < argc; i++)
	{
		size_t size = strlen(argv[i]) + 1; // strlen不包括字符串末尾'\0'
		strcpy(ptmp, argv[i]);			   // 拷贝环境变量内容, '\0'会被拷贝
		argv[i] = ptmp;					   // 环境变量指针指向新内存
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
	if (title == nullptr)
		return;
	memset(g_argv, 0, g_argv_len);						  // 原有内存空间argv+environ清空
	for (int i = 0; i <= g_argv_len - 2 && title[i]; i++) // 更改原有内存空间argv+environ内容，实现进程标题更改
		g_argv[i] = title[i];
}
void ngx_free_setproctitle()
{
	delete argv_environ;
	argv_environ = nullptr;
}
