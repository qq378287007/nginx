﻿
#pragma once

#include <vector>
using namespace std;

#define NGX_LISTEN_BACKLOG 511 // 已完成连接队列，nginx给511

typedef struct // 和监听端口有关的结构
{
	int port; // 监听的端口号
	int fd;	  // 套接字句柄socket
} ngx_listening_t, *lpngx_listening_t;

// socket相关类
class CSocekt
{
public:
	CSocekt();			// 构造函数
	virtual ~CSocekt(); // 释放函数

public:
	virtual bool Initialize(); // 初始化函数

private:
	bool ngx_open_listening_sockets();	// 监听必须的端口【支持多个端口】
	void ngx_close_listening_sockets(); // 关闭监听套接字
	bool setnonblocking(int sockfd);	// 设置非阻塞套接字

private:
	int m_ListenPortCount;						  // 所监听的端口数量
	vector<lpngx_listening_t> m_ListenSocketList; // 监听套接字队列
};
