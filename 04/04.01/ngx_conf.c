#include <stdio.h>
#include "ngx_conf.h"

void myconf()
{
    printf("执行了myconf()函数, MYVER=%s!\n", MYVER);
}
