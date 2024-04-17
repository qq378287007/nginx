#pragma once
#include <signal.h>
#include "ngx_c_socket.h"

typedef struct
{
	char ItemName[50];
	char ItemContent[500];
} CConfItem, *LPCConfItem;

extern pid_t ngx_pid;
extern pid_t ngx_parent;
extern int ngx_process;
extern sig_atomic_t ngx_reap;

extern int g_daemonized;

extern CSocekt       g_socket;  
