#pragma once

void ngx_init_setproctitle(int argc, char **argv);
void ngx_setproctitle(const char *title);
void ngx_free_setproctitle();
