#ifndef __NGX_GBLDEF_H__
#define __NGX_GBLDEF_H__

//一些比较通用的定义放在这里

//结构定义
typedef struct{
	char ItemName[50];
	char ItemContent[500];
} CConfItem, *LPCConfItem;

//外部全局量声明
extern int argc;
extern char  **argv;

extern int g_argv_len;//原始argv+environ占据内存字节数
extern char *g_argv; //指向原始argv+environ的内存空间, 不能释放

extern char *argv_environ; //指向重新创建的argv+environ的内存空间

#endif