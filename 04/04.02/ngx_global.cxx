#include "ngx_global.h"

//和设置标题有关的全局量
int argc;//命令行参数个数
char  **argv;//命令行参数

char *g_argv; //原始argv+environ内存空间，不能释放
int g_argv_len;//原始argv+environ内存空间所占内存大小

char *argv_environ; //指向重新创建的argv+environ的内存空间