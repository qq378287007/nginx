#pragma once

typedef struct
{
	char ItemName[50];
	char ItemContent[500];
} CConfItem, *LPCConfItem;

extern pid_t ngx_pid;
extern pid_t ngx_parent;
