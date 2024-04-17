#pragma once
#include <ctype.h>

void ngx_log_init();
void ngx_log_close();
void ngx_log_stderr(int err, const char *fmt, ...);
void ngx_log_error_core(int level, int err, const char *fmt, ...);
