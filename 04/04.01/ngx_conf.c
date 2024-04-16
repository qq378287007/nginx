#include <stdio.h>
#include "ngx_conf.h"
#define MYVER "1.2"

void myconf()
{
    printf("执行了myconf()函数, MYVER=%s!\n", MYVER);
}
