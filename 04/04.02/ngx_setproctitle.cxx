#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //env
#include <string.h>

#include "ngx_global.h"
#include "ngx_setproctitle.h"

//分配内存拷贝命令行参数和环境变量
void ngx_init_setproctitle(int argc0, char **argv0) {
	int i;
	size_t size;
	char *ptmp;
	argc = argc0;
	argv = argv0;
	g_argv = argv[0]; 
	
	g_argv_len = 0;
	for(i=0; i<argc; i++)
        g_argv_len += strlen(argv[i]) + 1; //末尾'\0'占1个字节
    for(i=0; environ[i]; i++) 
        g_argv_len += strlen(environ[i]) + 1; //末尾'\0'占1个字节
	
	argv_environ = new char[g_argv_len]; //新建内存空间放置argv+environ
	
	ptmp = argv_environ;
    memset(ptmp, 0, g_argv_len);
	for(i=0; i<argc; i++){
        size = strlen(argv[i])+1 ; //strlen不包括字符串末尾'\0'
        strcpy(ptmp, argv[i]);      //拷贝环境变量内容, '\0'会被拷贝
        argv[i] = ptmp;            //环境变量指针指向新内存
        ptmp += size;
	}
    for(i=0; environ[i]; i++){
        size = strlen(environ[i])+1 ; 
        strcpy(ptmp, environ[i]);
        environ[i] = ptmp;
        ptmp += size;
    }
}

void ngx_setproctitle(const char *title){
	if(title == NULL)
		return;
    memset(g_argv, 0, g_argv_len);  //原有内存空间argv+environ清空
	for(int i=0; i<=g_argv_len-2 && title[i]; i++)//更改原有内存空间argv+environ内容，实现进程标题更改
		g_argv[i] = title[i];
}