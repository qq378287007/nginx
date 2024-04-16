#pragma once

void ngx_log_init();
void ngx_log_close();
void ngx_log_stderr(int err, const char *fmt, ...);
void ngx_log_error_core(int level, int err, const char *fmt, ...);
u_char *ngx_log_errno(u_char *buf, u_char *last, int err);
