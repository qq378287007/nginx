#ifndef __NGX_SETPROCTITLE_H__
#define __NGX_SETPROCTITLE_H__

//设置可执行程序标题相关函数
void ngx_init_setproctitle(int argc0, char **argv0);
void ngx_setproctitle(const char *title);

#endif  