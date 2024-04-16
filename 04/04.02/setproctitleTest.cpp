#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define __USE_GNU //*.c文件需要定义
#include <unistd.h>  //environ

int argc;
char **argv;

int g_argv_len;//原始argv+environ占据内存字节数
char *g_argv; //指向原始argv+environ的内存空间

char *argv_environ; //指向重新创建的argv+environ的内存空间

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

//argv和environ，占据一段连续内存空间，不能释放（估计系统会处理）
int main(int argc, char **argv) {
	ngx_init_setproctitle(argc, argv);
	
	
	ngx_setproctitle("title");
	
	while(1){}
	
	printf("over\n");
	
	delete argv_environ;
	argv_environ = NULL;
	return 0;
}

/*
g++ setproctitleTest.cxx -o setproctitleTest
ps -ef | grep title
*/