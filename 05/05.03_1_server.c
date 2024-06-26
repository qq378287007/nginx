﻿
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SERV_PORT 9000

int main()
{
    struct sockaddr_in serv_addr; // 服务器的地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                // 选择协议族为IPV4
    serv_addr.sin_port = htons(SERV_PORT);         // 绑定我们自定义的端口号，客户端程序和我们服务器程序通讯时，就要往这个端口连接和传送数据
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本地所有的IP地址；INADDR_ANY表示的是一个服务器上所有的网卡（服务器可能不止一个网卡）多个本地ip地址都进行绑定端口号，进行侦听。

    // 服务器的socket套接字【文件描述符】
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); // 创建服务器的socket，大家可以暂时不用管这里的参数是什么，知道这个函数大概做什么就行

    int result = bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); // 绑定服务器地址结构体
    if (result == -1)
    {
        char *perrorinfo = strerror(errno);
        printf("bind返回的值为%d, 错误码为:%d，错误信息为:%s;\n", result, errno, perrorinfo);
        return -1;
    }

    result = listen(listenfd, 32); // 参数2表示服务器可以积压的未处理完的连入请求总个数，客户端来一个未连入的请求，请求数+1，连入请求完成，c/s之间进入正常通讯后，请求数-1
    if (result == -1)
    {
        char *perrorinfo = strerror(errno);
        printf("listen返回的值为%d, 错误码为:%d，错误信息为:%s;\n", result, errno, perrorinfo);
        return -1;
    }

    {
        struct sockaddr_in serv_addr2;
        memset(&serv_addr2, 0, sizeof(serv_addr2));
        serv_addr2.sin_family = AF_INET;
        serv_addr2.sin_port = htons(SERV_PORT); // 端口重复,bind会失败
        serv_addr2.sin_addr.s_addr = htonl(INADDR_ANY);

        // 再绑定一个
        int listenfd2 = socket(AF_INET, SOCK_STREAM, 0);

        result = bind(listenfd2, (struct sockaddr *)&serv_addr2, sizeof(serv_addr2));
        char *perrorinfo = strerror(errno);                                                   // 根据资料不会返回NULL;
        printf("bind返回的值为%d, 错误码为:%d，错误信息为:%s;\n", result, errno, perrorinfo); // bind返回的值为-1,错误码为:98，错误信息为:Address already in use;
    }

    const char *pcontent = "I sent sth to client!\n"; // 指向常量字符串区的指针
    while (1)
    {
        // 卡在这里，等客户单连接，客户端连入后，该函数走下去【注意这里返回的是一个新的socket——connfd，后续本服务器就用connfd和客户端之间收发数据，而原有的lisenfd依旧用于继续监听其他连接】
        int connfd = accept(listenfd, (struct sockaddr *)NULL, NULL);

        // 发送数据包给客户端
        write(connfd, pcontent, strlen(pcontent)); // 注意第一个参数是accept返回的connfd套接字
        printf("本服务器给客户端发送了一串字符~~~~~~~~~~~!\n");

        // 只给客户端发送一个信息，然后直接关闭套接字连接；
        close(connfd);
    }                // end for
    close(listenfd); // 实际本简单范例走不到这里，这句暂时看起来没啥用
    return 0;
}
