#pragma once
#include <ctype.h>

u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);
u_char *ngx_slprintf(u_char *buf, u_char *last, const char *fmt, ...);
u_char *ngx_snprintf(u_char *buf, size_t max, const char *fmt, ...);
