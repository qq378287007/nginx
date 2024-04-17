#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>   //uintptr_t
#include <stdarg.h>   //va_start....
#include <unistd.h>   //STDERR_FILENO等
#include <sys/time.h> //gettimeofday
#include <time.h>     //localtime_r
#include <fcntl.h>    //open
#include <errno.h>    //errno
// #include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_log.h"
#include "ngx_printf.h"
#include "ngx_event.h"
#include "ngx_c_memory.h"
#include "ngx_c_socket.h"

// 构造函数
CSocekt::CSocekt()
{
    // 配置相关
    m_worker_connections = 1; // epoll连接最大项数
    m_ListenPortCount = 1;    // 监听一个端口

    // epoll相关
    m_epollhandle = -1;         // epoll返回的句柄
    m_pconnections = NULL;      // 连接池【连接数组】先给空
    m_pfree_connections = NULL; // 连接池中空闲的连接链
    // m_pread_events = NULL;       //读事件数组给空
    // m_pwrite_events = NULL;      //写事件数组给空

    // 一些和网络通讯有关的常用变量值，供后续频繁使用时提高效率
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);  // 包头的sizeof值【占用的字节数】
    m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER); // 消息头的sizeof值【占用的字节数】
}

// 释放函数
CSocekt::~CSocekt()
{
    // 释放必须的内存
    //(1)监听端口相关内存的释放--------
    for (std::vector<lpngx_listening_t>::iterator pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) // vector
        delete (*pos);
    m_ListenSocketList.clear();

    //(2)连接池相关的内容释放---------
    // if(m_pwrite_events != NULL)//释放写事件数组
    //    delete [] m_pwrite_events;

    // if(m_pread_events != NULL)//释放读事件数组
    //     delete [] m_pread_events;

    if (m_pconnections != NULL) // 释放连接池
        delete[] m_pconnections;

    //(3)接收消息队列中内容释放
    clearMsgRecvQueue();
}

// 初始化函数【fork()子进程之前干这个事】
// 成功返回true，失败返回false
bool CSocekt::Initialize()
{
    ReadConf();                          // 读配置项
    return ngx_open_listening_sockets(); // 打开监听端口
}

//--------------------------------------------------------------------
//(1)epoll功能初始化，子进程中进行 ，本函数被ngx_worker_process_init()所调用
int CSocekt::ngx_epoll_init()
{
    //(1)很多内核版本不处理epoll_create的参数，只要该参数>0即可
    // 创建一个epoll对象，创建了一个红黑树，还创建了一个双向链表
    m_epollhandle = epoll_create(m_worker_connections); // 直接以epoll连接的最大项数为参数，肯定是>0的；
    if (m_epollhandle == -1)
    {
        ngx_log_stderr(errno, "CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2); // 这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
    }

    //(2)创建连接池【数组】、创建出来，这个东西后续用于处理所有客户端的连接
    m_connection_n = m_worker_connections; // 记录当前连接池中连接总数
    // 连接池【数组，每个元素是一个对象】
    m_pconnections = new ngx_connection_t[m_connection_n]; // new不可以失败，不用判断结果，如果失败直接报异常更好一些
    // m_pread_events = new ngx_event_t[m_connection_n];
    // m_pwrite_events = new ngx_event_t[m_connection_n];
    // for(int i = 0; i < m_connection_n; i++)
    //{
    //     m_pconnections[i].instance = 1;    //失效标志位设置为1【失效】，此句抄自官方nginx，这句到底有啥用，后续再研究
    // } //end for

    int i = m_connection_n; // 连接池中连接数
    lpngx_connection_t next = NULL;
    lpngx_connection_t c = m_pconnections; // 连接池数组首地址
    do
    {
        i--; // 注意i是数字的末尾，从最后遍历，i递减至数组首个元素

        // 好从屁股往前来---------
        c[i].data = next;       // 设置连接对象的next指针，注意第一次循环时next = NULL;
        c[i].fd = -1;           // 初始化连接，无socket和该连接池中的连接【对象】绑定
        c[i].instance = 1;      // 失效标志位设置为1【失效】，此句抄自官方nginx，这句到底有啥用，后续再研究
        c[i].iCurrsequence = 0; // 当前序号统一从0开始
        //----------------------

        next = &c[i];                     // next指针前移
    } while (i);                          // 循环直至i为0，即数组首地址
    m_pfree_connections = next;           // 设置空闲连接链表头指针,因为现在next指向c[0]，注意现在整个链表都是空的
    m_free_connection_n = m_connection_n; // 空闲连接链表长度，因为现在整个链表都是空的，这两个长度相等；

    //(3)遍历所有监听socket【监听端口】，我们为每个监听socket增加一个 连接池中的连接【说白了就是让一个socket和一个内存绑定，以方便记录该sokcet相关的数据、状态等等】
    for (std::vector<lpngx_listening_t>::iterator pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos)
    {
        c = ngx_get_connection((*pos)->fd); // 从连接池中获取一个空闲连接对象
        if (c == NULL)
        {
            // 这是致命问题，刚开始怎么可能连接池就为空呢？
            ngx_log_stderr(errno, "CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2); // 这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
        }
        c->listening = (*pos);  // 连接对象 和监听对象关联，方便通过连接对象找监听对象
        (*pos)->connection = c; // 监听对象 和连接对象关联，方便通过监听对象找连接对象

        // rev->accept = 1; //监听端口必须设置accept标志为1  ，这个是否有必要，再研究

        // 对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三路握手的，所以监听端口关心的就是读事件
        c->rhandler = &CSocekt::ngx_event_accept;

        // 往监听socket上增加监听事件，从而开始让监听端口履行其职责【如果不加这行，虽然端口能连上，但不会触发ngx_epoll_process_events()里边的epoll_wait()往下走】
        if (ngx_epoll_add_event((*pos)->fd,    // socekt句柄
                                1, 0,          // 读，写【只关心读事件，所以参数2：readevent=1,而参数3：writeevent=0】
                                0,             // 其他补充标记
                                EPOLL_CTL_ADD, // 事件类型【增加，还有删除/修改】
                                c              // 连接池中的连接
                                ) == -1)
        {
            exit(2); // 有问题，直接退出，日志 已经写过了
        }
    } // end for
    return 1;
}

/*
//(2)监听端口开始工作，监听端口要开始工作，必须为其增加读事件，因为监听端口只关心读事件
void CSocekt::ngx_epoll_listenportstart()
{
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) //vector
    {
        //本函数如果失败，直接退出
        ngx_epoll_add_event((*pos)->fd,1,0); //只关心读事件
    } //end for
    return;
}
*/

// epoll增加事件，可能被ngx_epoll_init()等函数调用
// fd:句柄，一个socket
// readevent：表示是否是个读事件，0是，1不是
// writeevent：表示是否是个写事件，0是，1不是
// otherflag：其他需要额外补充的标记，弄到这里
// eventtype：事件类型  ，一般就是用系统的枚举值，增加，删除，修改等;
// c：对应的连接池中的连接的指针
// 返回值：成功返回1，失败返回-1；
int CSocekt::ngx_epoll_add_event(int fd,
                                 int readevent, int writeevent,
                                 uint32_t otherflag,
                                 uint32_t eventtype,
                                 lpngx_connection_t c)
{
    struct epoll_event ev;
    // int op;
    memset(&ev, 0, sizeof(ev));

    if (readevent == 1)
    {
        // 读事件，这里发现官方nginx没有使用EPOLLERR，因此我们也不用【有些范例中是使用EPOLLERR的】
        ev.events = EPOLLIN | EPOLLRDHUP; // EPOLLIN读事件，也就是read ready【客户端三次握手连接进来，也属于一种可读事件】   EPOLLRDHUP 客户端关闭连接，断连
                                          // 似乎不用加EPOLLERR，只用EPOLLRDHUP即可，EPOLLERR/EPOLLRDHUP 实际上是通过触发读写事件进行读写操作recv write来检测连接异常

        // ev.events |= (ev.events | EPOLLET);  //只支持非阻塞socket的高速模式【ET：边缘触发】，就拿accetp来说，如果加这个EPOLLET，则客户端连入时，epoll_wait()只会返回一次该事件，
        // 如果用的是EPOLLLT【水平触发：低速模式】，则客户端连入时，epoll_wait()会被触发多次，一直到用accept()来处理；

        // https://blog.csdn.net/q576709166/article/details/8649911
        // 找下EPOLLERR的一些说法：
        // a)对端正常关闭（程序里close()，shell下kill或ctr+c），触发EPOLLIN和EPOLLRDHUP，但是不触发EPOLLERR 和EPOLLHUP。
        // b)EPOLLRDHUP    这个好像有些系统检测不到，可以使用EPOLLIN，read返回0，删除掉事件，关闭close(fd);如果有EPOLLRDHUP，检测它就可以直到是对方关闭；否则就用上面方法。
        // c)client 端close()联接,server 会报某个sockfd可读，即epollin来临,然后recv一下 ， 如果返回0再掉用epoll_ctl 中的EPOLL_CTL_DEL , 同时close(sockfd)。
        // 有些系统会收到一个EPOLLRDHUP，当然检测这个是最好不过了。只可惜是有些系统，上面的方法最保险；如果能加上对EPOLLRDHUP的处理那就是万能的了。
        // d)EPOLLERR      只有采取动作时，才能知道是否对方异常。即对方突然断掉，是不可能有此事件发生的。只有自己采取动作（当然自己此刻也不知道），read，write时，出EPOLLERR错，说明对方已经异常断开。
        // e)EPOLLERR 是服务器这边出错（自己出错当然能检测到，对方出错你咋能知道啊）
        // f)给已经关闭的socket写时，会发生EPOLLERR，也就是说，只有在采取行动（比如读一个已经关闭的socket，或者写一个已经关闭的socket）时候，才知道对方是否关闭了。
        // 这个时候，如果对方异常关闭了，则会出现EPOLLERR，出现Error把对方DEL掉，close就可以了。
    }
    else
    {
        // 其他事件类型待处理
        //.....
    }

    if (otherflag != 0)
    {
        ev.events |= otherflag;
    }

    // 以下这段代码抄自nginx官方,因为指针的最后一位【二进制位】肯定不是1，所以 和 c->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的c->instance值取到
    // 比如c是个地址，可能的值是 0x00af0578，对应的二进制是‭101011110000010101111000‬，而 | 1后是0x00af0579
    ev.data.ptr = (void *)((uintptr_t)c | c->instance); // 把对象弄进去，后续来事件时，用epoll_wait()后，这个对象能取出来用
                                                        // 但同时把一个 标志位【不是0就是1】弄进去

    if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1)
    {
        ngx_log_stderr(errno, "CSocekt::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.", fd, readevent, writeevent, otherflag, eventtype);
        // exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦，后来发现不能直接退；
        return -1;
    }
    return 1;
}

// 开始获取发生的事件消息
// 参数unsigned int timer：epoll_wait()阻塞的时长，单位是毫秒；
// 返回值，1：正常返回  ,0：有问题返回，一般不管是正常还是问题返回，都应该保持进程继续运行
// 本函数被ngx_process_events_and_timers()调用，而ngx_process_events_and_timers()是在子进程的死循环中被反复调用
int CSocekt::ngx_epoll_process_events(int timer)
{
    // 等待事件，事件会返回到m_events里，最多返回NGX_MAX_EVENTS个事件【因为我只提供了这些内存】；
    // 如果两次调用epoll_wait()的事件间隔比较长，则可能在epoll的双向链表中，积累了多个事件，所以调用epoll_wait，可能取到多个事件
    // 阻塞timer这么长时间除非：a)阻塞时间到达 b)阻塞期间收到事件【比如新用户连入】会立刻返回c)调用时有事件也会立刻返回d)如果来个信号，比如你用kill -1 pid测试
    // 如果timer为-1则一直阻塞，如果timer为0则立即返回，即便没有任何事件
    // 返回值：有错误发生返回-1，错误在errno中，比如你发个信号过来，就返回-1，错误信息是(4: Interrupted system call)
    //        如果你等待的是一段时间，并且超时了，则返回0；
    //        如果返回>0则表示成功捕获到这么多个事件【返回值里】
    int events = epoll_wait(m_epollhandle, m_events, NGX_MAX_EVENTS, timer);

    if (events == -1)
    {
        // 有错误发生，发送某个信号给本进程就可以导致这个条件成立，而且错误码根据观察是4；
        // #define EINTR  4，EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        // 例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if (errno == EINTR)
        {
            // 信号所致，直接返回，一般认为这不是毛病，但还是打印下日志记录一下，因为一般也不会人为给worker进程发送消息
            ngx_log_error_core(NGX_LOG_INFO, errno, "CSocekt::ngx_epoll_process_events()中epoll_wait()失败!");
            return 1; // 正常返回
        }
        else
        {
            // 这被认为应该是有问题，记录日志
            ngx_log_error_core(NGX_LOG_ALERT, errno, "CSocekt::ngx_epoll_process_events()中epoll_wait()失败!");
            return 0; // 非正常返回
        }
    }

    if (events == 0) // 超时，但没事件来
    {
        if (timer != -1)
        {
            // 要求epoll_wait阻塞一定的时间而不是一直阻塞，这属于阻塞到时间了，则正常返回
            return 1;
        }
        // 无限等待【所以不存在超时】，但却没返回任何事件，这应该不正常有问题
        ngx_log_error_core(NGX_LOG_ALERT, 0, "CSocekt::ngx_epoll_process_events()中epoll_wait()没超时却没返回任何事件!");
        return 0; // 非正常返回
    }

    // 会惊群，一个telnet上来，4个worker进程都会被惊动，都执行下边这个
    // ngx_log_stderr(errno,"惊群测试1:%d",events);

    // 走到这里，就是属于有事件收到了
    lpngx_connection_t c;
    uintptr_t instance;
    uint32_t revents;
    for (int i = 0; i < events; ++i) // 遍历本次epoll_wait返回的所有事件，注意events才是返回的实际事件数量
    {
        c = (lpngx_connection_t)(m_events[i].data.ptr);         // ngx_epoll_add_event()给进去的，这里能取出来
        instance = (uintptr_t)c & 1;                            // 将地址的最后一位取出来，用instance变量标识, 见ngx_epoll_add_event，该值是当时随着连接池中的连接一起给进来的
                                                                // 取得的是你当时调用ngx_epoll_add_event()的时候，这个连接里边的instance变量的值；
        c = (lpngx_connection_t)((uintptr_t)c & (uintptr_t)~1); // 最后1位干掉，得到真正的c地址

        // 仔细分析一下官方nginx的这个判断
        // 过滤过期事件的；
        if (c->fd == -1) // 一个套接字，当关联一个 连接池中的连接【对象】时，这个套接字值是要给到c->fd的，
                         // 那什么时候这个c->fd会变成-1呢？关闭连接时这个fd会被设置为-1，哪行代码设置的-1再研究，但应该不是ngx_free_connection()函数设置的-1
        {
            // 比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭，那我们应该会把c->fd设置为-1；
            // 第二个事件照常处理
            // 第三个事件，假如这第三个事件，也跟第一个事件对应的是同一个连接，那这个条件就会成立；那么这种事件，属于过期事件，不该处理

            // 这里可以增加个日志，也可以不增加日志
            ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocekt::ngx_epoll_process_events()中遇到了fd=-1的过期事件:%p.", c);
            continue; // 这种事件就不处理即可
        }

        // 过滤过期事件的；
        if (c->instance != instance)
        {
            //--------------------以下这些说法来自于资料--------------------------------------
            // 什么时候这个条件成立呢？【换种问法：instance标志为什么可以判断事件是否过期呢？】
            // 比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭【麻烦就麻烦在这个连接被服务器关闭上了】，但是恰好第三个事件也跟这个连接有关；
            // 因为第一个事件就把socket连接关闭了，显然第三个事件我们是不应该处理的【因为这是个过期事件】，若处理肯定会导致错误；
            // 那我们上述把c->fd设置为-1，可以解决这个问题吗？ 能解决一部分问题，但另外一部分不能解决，不能解决的问题描述如下【这么离奇的情况应该极少遇到】：

            // a)处理第一个事件时，因为业务需要，我们把这个连接【假设套接字为50】关闭，同时设置c->fd = -1;并且调用ngx_free_connection将该连接归还给连接池；
            // b)处理第二个事件，恰好第二个事件是建立新连接事件，调用ngx_get_connection从连接池中取出的连接非常可能就是刚刚释放的第一个事件对应的连接池中的连接；
            // c)又因为a中套接字50被释放了，所以会被操作系统拿来复用，复用给了b)【一般这么快就被复用也是醉了】；
            // d)当处理第三个事件时，第三个事件其实是已经过期的，应该不处理，那怎么判断这第三个事件是过期的呢？ 【假设现在处理的是第三个事件，此时这个 连接池中的该连接 实际上已经被用作第二个事件对应的socket上了】；
            // 依靠instance标志位能够解决这个问题，当调用ngx_get_connection从连接池中获取一个新连接时，我们把instance标志位置反，所以这个条件如果不成立，说明这个连接已经被挪作他用了；

            //--------------------我的个人思考--------------------------------------
            // 如果收到了若干个事件，其中连接关闭也搞了多次，导致这个instance标志位被取反2次，那么，造成的结果就是：还是有可能遇到某些过期事件没有被发现【这里也就没有被continue】，照旧被当做没过期事件处理了；
            // 如果是这样，那就只能被照旧处理了。可能会造成偶尔某个连接被误关闭？但是整体服务器程序运行应该是平稳，问题不大的，这种漏网而被当成没过期来处理的的过期事件应该是极少发生的

            ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocekt::ngx_epoll_process_events()中遇到了instance值改变的过期事件:%p.", c);
            continue; // 这种事件就不处理即可
        }
        // 存在一种可能性，过期事件没被过滤完整【非常极端】，走下来的；

        // 能走到这里，我们认为这些事件都没过期，就正常开始处理
        revents = m_events[i].events; // 取出事件类型

        if (revents & (EPOLLERR | EPOLLHUP)) // 例如对方close掉套接字，这里会感应到【换句话说：如果发生了错误或者客户端断连】
        {
            // 这加上读写标记，方便后续代码处理，至于怎么处理，后续再说，这里也是参照nginx官方代码引入的这段代码；
            // 官方说法：if the error events were returned, add EPOLLIN and EPOLLOUT，to handle the events at least in one active handler
            // 我认为官方也是经过反复思考才加上着东西的，先放这里放着吧；
            revents |= EPOLLIN | EPOLLOUT; // EPOLLIN：表示对应的链接上有数据可以读出（TCP链接的远端主动关闭连接，也相当于可读事件，因为本服务器小处理发送来的FIN包）
                                           // EPOLLOUT：表示对应的连接上可以写入数据发送【写准备好】
        }

        if (revents & EPOLLIN) // 如果是读事件
        {
            // ngx_log_stderr(errno,"数据来了来了来了 ~~~~~~~~~~~~~.");
            // 一个客户端新连入，这个会成立，
            // 已连接发送数据来，这个也成立；
            // c->r_ready = 1;               //标记可以读；【从连接池拿出一个连接时这个连接的所有成员都是0】
            (this->*(c->rhandler))(c); // 注意括号的运用来正确设置优先级，防止编译出错；【如果是个新客户连入
                                       // 如果新连接进入，这里执行的应该是CSocekt::ngx_event_accept(c)】
                                       // 如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::ngx_wait_request_handler
        }

        if (revents & EPOLLOUT) // 如果是写事件【对方关闭连接也触发这个，再研究。。。。。。】，注意上边的 if(revents & (EPOLLERR|EPOLLHUP))  revents |= EPOLLIN|EPOLLOUT; 读写标记都给加上了
        {
            //....待扩展， 客户端关闭时，关闭的时候能够执行到这里，因为上边有if(revents & (EPOLLERR|EPOLLHUP))  revents |= EPOLLIN|EPOLLOUT; 代码

            ngx_log_stderr(errno, "111111111111111111111111111111.");
        }
    } // end for(int i = 0; i < events; ++i)
    return 1;
}

// 专门用于读各种配置项
void CSocekt::ReadConf()
{
    CConfig *p_config = CConfig::GetInstance();
    m_worker_connections = p_config->GetIntDefault("worker_connections", m_worker_connections); // epoll连接的最大项数
    m_ListenPortCount = p_config->GetIntDefault("ListenPortCount", m_ListenPortCount);          // 取得要监听的端口数量
    return;
}

// 监听端口【支持多个端口】，这里遵从nginx的函数命名
// 在创建worker进程之前就要执行这个函数；
bool CSocekt::ngx_open_listening_sockets()
{
    int isock;                    // socket
    struct sockaddr_in serv_addr; // 服务器的地址结构体
    int iport;                    // 端口
    char strinfo[100];            // 临时字符串

    // 初始化相关
    memset(&serv_addr, 0, sizeof(serv_addr));      // 先初始化一下
    serv_addr.sin_family = AF_INET;                // 选择协议族为IPV4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本地所有的IP地址；INADDR_ANY表示的是一个服务器上所有的网卡（服务器可能不止一个网卡）多个本地ip地址都进行绑定端口号，进行侦听。

    // 中途用到一些配置信息
    CConfig *p_config = CConfig::GetInstance();
    for (int i = 0; i < m_ListenPortCount; i++) // 要监听这么多个端口
    {
        // 参数1：AF_INET：使用ipv4协议，一般就这么写
        // 参数2：SOCK_STREAM：使用TCP，表示可靠连接【相对还有一个UDP套接字，表示不可靠连接】
        // 参数3：给0，固定用法，就这么记
        isock = socket(AF_INET, SOCK_STREAM, 0); // 系统函数，成功返回非负描述符，出错返回-1
        if (isock == -1)
        {
            ngx_log_stderr(errno, "CSocekt::Initialize()中socket()失败,i=%d.", i);
            // 其实这里直接退出，那如果以往有成功创建的socket呢？就没得到释放吧，当然走到这里表示程序不正常，应该整个退出，也没必要释放了
            return false;
        }

        // setsockopt（）:设置一些套接字参数选项；
        // 参数2：是表示级别，和参数3配套使用，也就是说，参数3如果确定了，参数2就确定了;
        // 参数3：允许重用本地地址
        // 设置 SO_REUSEADDR，目的第五章第三节讲解的非常清楚：主要是解决TIME_WAIT这个状态导致bind()失败的问题
        int reuseaddr = 1; // 1:打开对应的设置项
        if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr, sizeof(reuseaddr)) == -1)
        {
            ngx_log_stderr(errno, "CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败,i=%d.", i);
            close(isock); // 无需理会是否正常执行了
            return false;
        }
        // 设置该socket为非阻塞
        if (setnonblocking(isock) == false)
        {
            ngx_log_stderr(errno, "CSocekt::Initialize()中setnonblocking()失败,i=%d.", i);
            close(isock);
            return false;
        }

        // 设置本服务器要监听的地址和端口，这样客户端才能连接到该地址和端口并发送数据
        strinfo[0] = 0;
        sprintf(strinfo, "ListenPort%d", i);
        iport = p_config->GetIntDefault(strinfo, 10000);
        serv_addr.sin_port = htons((in_port_t)iport); // in_port_t其实就是uint16_t

        // 绑定服务器地址结构体
        if (bind(isock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        {
            ngx_log_stderr(errno, "CSocekt::Initialize()中bind()失败,i=%d.", i);
            close(isock);
            return false;
        }

        // 开始监听
        if (listen(isock, NGX_LISTEN_BACKLOG) == -1)
        {
            ngx_log_stderr(errno, "CSocekt::Initialize()中listen()失败,i=%d.", i);
            close(isock);
            return false;
        }

        // 可以，放到列表里来
        lpngx_listening_t p_listensocketitem = new ngx_listening_t;    // 千万不要写错，注意前边类型是指针，后边类型是一个结构体
        memset(p_listensocketitem, 0, sizeof(ngx_listening_t));        // 注意后边用的是 ngx_listening_t而不是lpngx_listening_t
        p_listensocketitem->port = iport;                              // 记录下所监听的端口号
        p_listensocketitem->fd = isock;                                // 套接字木柄保存下来
        ngx_log_error_core(NGX_LOG_INFO, 0, "监听%d端口成功!", iport); // 显示一些信息到日志中
        m_ListenSocketList.push_back(p_listensocketitem);              // 加入到队列中
    }                                                                  // end for(int i = 0; i < m_ListenPortCount; i++)
    if (m_ListenSocketList.size() <= 0)                                // 不可能一个端口都不监听吧
        return false;
    return true;
}

// 关闭socket，什么时候用，我们现在先不确定，先把这个函数预备在这里
void CSocekt::ngx_close_listening_sockets()
{
    for (int i = 0; i < m_ListenPortCount; i++) // 要关闭这么多个监听端口
    {
        // ngx_log_stderr(0,"端口是%d,socketid是%d.",m_ListenSocketList[i]->port,m_ListenSocketList[i]->fd);
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO, 0, "关闭监听端口%d!", m_ListenSocketList[i]->port); // 显示一些信息到日志中
    }
}

// 设置socket连接为非阻塞模式【这种函数的写法很固定】：非阻塞，概念在五章四节讲解的非常清楚【不断调用，不断调用这种：拷贝数据的时候是阻塞的】
bool CSocekt::setnonblocking(int sockfd)
{
    int nb = 1;                            // 0：清除，1：设置
    if (ioctl(sockfd, FIONBIO, &nb) == -1) // FIONBIO：设置/清除非阻塞I/O标记：0：清除，1：设置
    {
        return false;
    }
    return true;

    // 如下也是一种写法，跟上边这种写法其实是一样的，但上边的写法更简单
    /*
    //fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    //参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    if(opts < 0)
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_GETFL)失败.");
        return false;
    }
    opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    if(fcntl(sockfd, F_SETFL, opts) < 0)
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_SETFL)失败.");
        return false;
    }
    return true;
    */
}

// 建立新连接专用函数，当新连接进入时，本函数会被ngx_epoll_process_events()所调用
void CSocekt::ngx_event_accept(lpngx_connection_t oldc)
{
    // 因为listen套接字上用的不是ET【边缘触发】，而是LT【水平触发】，意味着客户端连入如果我要不处理，这个函数会被多次调用，所以，我这里这里可以不必多次accept()，可以只执行一次accept()
    // 这也可以避免本函数被卡太久，注意，本函数应该尽快返回，以免阻塞程序运行；
    struct sockaddr mysockaddr; // 远端服务器的socket地址
    socklen_t socklen;
    int err;
    int level;
    int s;
    static int use_accept4 = 1; // 我们先认为能够使用accept4()函数
    lpngx_connection_t newc;    // 代表连接池中的一个连接【注意这是指针】

    // ngx_log_stderr(0,"这是几个\n"); 这里会惊群，也就是说，epoll技术本身有惊群的问题

    socklen = sizeof(mysockaddr);
    do // 用do，跳到while后边去方便
    {
        if (use_accept4)
        {
            // 以为listen套接字是非阻塞的，所以即便已完成连接队列为空，accept4()也不会卡在这里；
            s = accept4(oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK); // 从内核获取一个用户端连接，最后一个参数SOCK_NONBLOCK表示返回一个非阻塞的socket，节省一次ioctl【设置为非阻塞】调用
        }
        else
        {
            // 以为listen套接字是非阻塞的，所以即便已完成连接队列为空，accept()也不会卡在这里；
            s = accept(oldc->fd, &mysockaddr, &socklen);
        }

        // 惊群，有时候不一定完全惊动所有4个worker进程，可能只惊动其中2个等等，其中一个成功其余的accept4()都会返回-1；错误 (11: Resource temporarily unavailable【资源暂时不可用】)
        // 所以参考资料：https://blog.csdn.net/russell_tao/article/details/7204260
        // 其实，在linux2.6内核上，accept系统调用已经不存在惊群了（至少我在2.6.18内核版本上已经不存在）。大家可以写个简单的程序试下，在父进程中bind,listen，然后fork出子进程，
        // 所有的子进程都accept这个监听句柄。这样，当新连接过来时，大家会发现，仅有一个子进程返回新建的连接，其他子进程继续休眠在accept调用上，没有被唤醒。
        // ngx_log_stderr(0,"测试惊群问题，看惊动几个worker进程%d\n",s); 【我的结论是：accept4可以认为基本解决惊群问题，但似乎并没有完全解决，有时候还会惊动其他的worker进程】

        if (s == -1)
        {
            err = errno;

            // 对accept、send和recv而言，事件未发生时errno通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）
            if (err == EAGAIN) // accept()没准备好，这个EAGAIN错误EWOULDBLOCK是一样的
            {
                // 除非你用一个循环不断的accept()取走所有的连接，不然一般不会有这个错误【我们这里只取一个连接，也就是accept()一次】
                return;
            }
            level = NGX_LOG_ALERT;
            if (err == ECONNABORTED) // ECONNRESET错误则发生在对方意外关闭套接字后【您的主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
            {
                // 该错误被描述为“software caused connection abort”，即“软件引起的连接中止”。原因在于当服务和客户进程在完成用于 TCP 连接的“三次握手”后，
                // 客户 TCP 却发送了一个 RST （复位）分节，在服务进程看来，就在该连接已由 TCP 排队，等着服务进程调用 accept 的时候 RST 却到达了。
                // POSIX 规定此时的 errno 值必须 ECONNABORTED。源自 Berkeley 的实现完全在内核中处理中止的连接，服务进程将永远不知道该中止的发生。
                // 服务器进程一般可以忽略该错误，直接再次调用accept。
                level = NGX_LOG_ERR;
            }
            else if (err == EMFILE || err == ENFILE) // EMFILE:进程的fd已用尽【已达到系统所允许单一进程所能打开的文件/套接字总数】。可参考：https://blog.csdn.net/sdn_prc/article/details/28661661   以及 https://bbs.csdn.net/topics/390592927
                                                     // ulimit -n ,看看文件描述符限制,如果是1024的话，需要改大;  打开的文件句柄数过多 ,把系统的fd软限制和硬限制都抬高.
            // ENFILE这个errno的存在，表明一定存在system-wide的resource limits，而不仅仅有process-specific的resource limits。按照常识，process-specific的resource limits，一定受限于system-wide的resource limits。
            {
                level = NGX_LOG_CRIT;
            }
            ngx_log_error_core(level, errno, "CSocekt::ngx_event_accept()中accept4()失败!");

            if (use_accept4 && err == ENOSYS) // accept4()函数没实现，坑爹？
            {
                use_accept4 = 0; // 标记不使用accept4()函数，改用accept()函数
                continue;        // 回去重新用accept()函数搞
            }

            if (err == ECONNABORTED) // 对方关闭套接字
            {
                // 这个错误因为可以忽略，所以不用干啥
                // do nothing
            }

            if (err == EMFILE || err == ENFILE)
            {
                // do nothing，这个官方做法是先把读事件从listen socket上移除，然后再弄个定时器，定时器到了则继续执行该函数，但是定时器到了有个标记，会把读事件增加到listen socket上去；
                // 我这里目前先不处理吧【因为上边已经写这个日志了】；
            }
            return;
        } // end if(s == -1)

        // 走到这里的，表示accept4()/accept()成功了
        // ngx_log_stderr(errno,"accept4成功s=%d",s); //s这里就是 一个句柄了
        newc = ngx_get_connection(s); // 这是针对新连入用户的连接，和监听套接字 所对应的连接是两个不同的东西，不要搞混
        if (newc == NULL)
        {
            // 连接池中连接不够用，那么就得把这个socekt直接关闭并返回了，因为在ngx_get_connection()中已经写日志了，所以这里不需要写日志了
            if (close(s) == -1)
            {
                ngx_log_error_core(NGX_LOG_ALERT, errno, "CSocekt::ngx_event_accept()中close(%d)失败!", s);
            }
            return;
        }
        //...........将来这里会判断是否连接超过最大允许连接数，现在，这里可以不处理

        // 成功的拿到了连接池中的一个连接
        memcpy(&newc->s_sockaddr, &mysockaddr, socklen); // 拷贝客户端地址到连接对象【要转成字符串ip地址参考函数ngx_sock_ntop()】
        //{
        //    //测试将收到的地址弄成字符串，格式形如"192.168.1.126:40904"或者"192.168.1.126"
        //    u_char ipaddr[100]; memset(ipaddr,0,sizeof(ipaddr));
        //    ngx_sock_ntop(&newc->s_sockaddr,1,ipaddr,sizeof(ipaddr)-10); //宽度给小点
        //    ngx_log_stderr(0,"ip信息为%s\n",ipaddr);
        //}

        if (!use_accept4)
        {
            // 如果不是用accept4()取得的socket，那么就要设置为非阻塞【因为用accept4()的已经被accept4()设置为非阻塞了】
            if (setnonblocking(s) == false)
            {
                // 设置非阻塞居然失败
                ngx_close_connection(newc); // 回收连接池中的连接（千万不能忘记），并关闭socket
                return;                     // 直接返回
            }
        }

        newc->listening = oldc->listening; // 连接对象 和监听对象关联，方便通过连接对象找监听对象【关联到监听端口】
        newc->w_ready = 1;                 // 标记可以写，新连接写事件肯定是ready的；【从连接池拿出一个连接时这个连接的所有成员都是0】

        newc->rhandler = &CSocekt::ngx_wait_request_handler; // 设置数据来时的读处理函数，其实官方nginx中是ngx_http_wait_request_handler()
        // 客户端应该主动发送第一次的数据，这里将读事件加入epoll监控，这样当客户端发送数据来时，会触发ngx_wait_request_handler()被ngx_epoll_process_events()调用
        if (ngx_epoll_add_event(s,             // socket句柄
                                1, 0,          // 读，写 ,这里读为1，表示客户端应该主动给我服务器发送消息，我服务器需要首先收到客户端的消息；
                                0,             // EPOLLET,          //其他补充标记【EPOLLET(高速模式，边缘触发ET)】
                                               // 后续因为实际项目需要，我们采用LT模式【水平触发模式/低速模式】
                                EPOLL_CTL_ADD, // 事件类型【增加，还有删除/修改】
                                newc           // 连接池中的连接
                                ) == -1)
        {
            // 增加事件失败，失败日志在ngx_epoll_add_event中写过了，因此这里不多写啥；
            ngx_close_connection(newc); // 回收连接池中的连接（千万不能忘记），并关闭socket
            return;                     // 直接返回
        }

        break; // 一般就是循环一次就跳出去
    } while (1);

    return;
}

// 来数据时候的处理，当连接上有数据来的时候，本函数会被ngx_epoll_process_events()所调用  ,官方的类似函数为ngx_http_wait_request_handler();
void CSocekt::ngx_wait_request_handler(lpngx_connection_t c)
{
    // 收包，注意我们用的第二个和第三个参数，我们用的始终是这两个参数，因此我们必须保证 c->precvbuf指向正确的收包位置，保证c->irecvlen指向正确的收包宽度
    ssize_t reco = recvproc(c, c->precvbuf, c->irecvlen);
    if (reco <= 0)
    {
        return; // 该处理的上边这个recvproc()函数处理过了，这里<=0是直接return
    }

    // 走到这里，说明成功收到了一些字节（>0），就要开始判断收到了多少数据了
    if (c->curStat == _PKG_HD_INIT) // 连接建立起来时肯定是这个状态，因为在ngx_get_connection()中已经把curStat成员赋值成_PKG_HD_INIT了
    {
        if (reco == m_iLenPkgHeader) // 正好收到完整包头，这里拆解包头
        {
            ngx_wait_request_handler_proc_p1(c); // 那就调用专门针对包头处理完整的函数去处理把。
        }
        else
        {
            // 收到的包头不完整--我们不能预料每个包的长度，也不能预料各种拆包/粘包情况，所以收到不完整包头【也算是缺包】是很可能的；
            c->curStat = _PKG_HD_RECVING;     // 接收包头中，包头不完整，继续接收包头中
            c->precvbuf = c->precvbuf + reco; // 注意收后续包的内存往后走
            c->irecvlen = c->irecvlen - reco; // 要收的内容当然要减少，以确保只收到完整的包头先
        }                                     // end  if(reco == m_iLenPkgHeader)
    }
    else if (c->curStat == _PKG_HD_RECVING) // 接收包头中，包头不完整，继续接收中，这个条件才会成立
    {
        if (c->irecvlen == reco) // 要求收到的宽度和我实际收到的宽度相等
        {
            // 包头收完整了
            ngx_wait_request_handler_proc_p1(c); // 那就调用专门针对包头处理完整的函数去处理把。
        }
        else
        {
            // 包头还是没收完整，继续收包头
            // c->curStat        = _PKG_HD_RECVING;                 //没必要
            c->precvbuf = c->precvbuf + reco; // 注意收后续包的内存往后走
            c->irecvlen = c->irecvlen - reco; // 要收的内容当然要减少，以确保只收到完整的包头先
        }
    }
    else if (c->curStat == _PKG_BD_INIT)
    {
        // 包头刚好收完，准备接收包体
        if (reco == c->irecvlen)
        {
            // 收到的宽度等于要收的宽度，包体也收完整了
            ngx_wait_request_handler_proc_plast(c);
        }
        else
        {
            // 收到的宽度小于要收的宽度
            c->curStat = _PKG_BD_RECVING;
            c->precvbuf = c->precvbuf + reco;
            c->irecvlen = c->irecvlen - reco;
        }
    }
    else if (c->curStat == _PKG_BD_RECVING)
    {
        // 接收包体中，包体不完整，继续接收中
        if (c->irecvlen == reco)
        {
            // 包体收完整了
            ngx_wait_request_handler_proc_plast(c);
        }
        else
        {
            // 包体没收完整，继续收
            c->precvbuf = c->precvbuf + reco;
            c->irecvlen = c->irecvlen - reco;
        }
    } // end if(c->curStat == _PKG_HD_INIT)
    return;
}

// 用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放【因为这里涉及到好几个要释放的资源，所以写成函数】
// 我们把ngx_close_accepted_connection()函数改名为让名字更通用，并从文件ngx_socket_accept.cxx迁移到本文件中，并改造其中代码，注意顺序
void CSocekt::ngx_close_connection(lpngx_connection_t c)
{
    if (close(c->fd) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT, errno, "CSocekt::ngx_close_connection()中close(%d)失败!", c->fd);
    }
    c->fd = -1;             // 官方nginx这么写，这么写有意义；
    ngx_free_connection(c); // 把释放代码放在最后边，感觉更合适
    return;
}

// 接收数据专用函数--引入这个函数是为了方便，如果断线，错误之类的，这里直接 释放连接池中连接，然后直接关闭socket，以免在其他函数中还要重复的干这些事
// 参数c：连接池中相关连接
// 参数buff：接收数据的缓冲区
// 参数buflen：要接收的数据大小
// 返回值：返回-1，则是有问题发生并且在这里把问题处理完毕了，调用本函数的调用者一般是可以直接return
//         返回>0，则是表示实际收到的字节数
ssize_t CSocekt::recvproc(lpngx_connection_t c, char *buff, ssize_t buflen) // ssize_t是有符号整型，在32位机器上等同与int，在64位机器上等同与long int，size_t就是无符号型的ssize_t
{
    ssize_t n;

    n = recv(c->fd, buff, buflen, 0); // recv()系统函数， 最后一个参数flag，一般为0；
    if (n == 0)
    {
        // 客户端关闭【应该是正常完成了4次挥手】，我这边就直接回收连接连接，关闭socket即可
        //    ngx_log_stderr(0,"连接被客户端正常关闭[4路挥手关闭]！");
        ngx_close_connection(c);
        return -1;
    }
    // 客户端没断，走这里
    if (n < 0) // 这被认为有错误发生
    {
        // EAGAIN和EWOULDBLOCK[【这个应该常用在hp上】应该是一样的值，表示没收到数据，一般来讲，在ET模式下会出现这个错误，因为ET模式下是不停的recv肯定有一个时刻收到这个errno，但LT模式下一般是来事件才收，所以不该出现这个返回值
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 我认为LT模式不该出现这个errno，而且这个其实也不是错误，所以不当做错误处理
            ngx_log_stderr(errno, "CSocekt::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立，出乎我意料！"); // epoll为LT模式不应该出现这个返回值，所以直接打印出来瞧瞧
            return -1;                                                                                               // 不当做错误处理，只是简单返回
        }
        // EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        // 例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if (errno == EINTR) // 这个不算错误，是我参考官方nginx，官方nginx这个就不算错误；
        {
            // 我认为LT模式不该出现这个errno，而且这个其实也不是错误，所以不当做错误处理
            ngx_log_stderr(errno, "CSocekt::recvproc()中errno == EINTR成立，出乎我意料！"); // epoll为LT模式不应该出现这个返回值，所以直接打印出来瞧瞧
            return -1;                                                                      // 不当做错误处理，只是简单返回
        }

        // 所有从这里走下来的错误，都认为异常：意味着我们要关闭客户端套接字要回收连接池中连接；

        // errno参考：http://dhfapiran1.360drm.com
        if (errno == ECONNRESET) // #define ECONNRESET 104 /* Connection reset by peer */
        {
            // 如果客户端没有正常关闭socket连接，却关闭了整个运行程序【真是够粗暴无理的，应该是直接给服务器发送rst包而不是4次挥手包完成连接断开】，那么会产生这个错误
            // 10054(WSAECONNRESET)--远程程序正在连接的时候关闭会产生这个错误--远程主机强迫关闭了一个现有的连接
            // 算常规错误吧【普通信息型】，日志都不用打印，没啥意思，太普通的错误
            // do nothing

            //....一些大家遇到的很普通的错误信息，也可以往这里增加各种，代码要慢慢完善，一步到位，不可能，很多服务器程序经过很多年的完善才比较圆满；
        }
        else
        {
            // 能走到这里的，都表示错误，我打印一下日志，希望知道一下是啥错误，我准备打印到屏幕上
            ngx_log_stderr(errno, "CSocekt::recvproc()中发生错误，我打印出来看看是啥错误！"); // 正式运营时可以考虑这些日志打印去掉
        }

        //    ngx_log_stderr(0,"连接被客户端 非 正常关闭！");

        // 这种真正的错误就要，直接关闭套接字，释放连接池中连接了
        ngx_close_connection(c);
        return -1;
    }

    // 能走到这里的，就认为收到了有效数据
    return n; // 返回收到的字节数
}

// 包头收完整后的处理，我们称为包处理阶段1【p1】：写成函数，方便复用
void CSocekt::ngx_wait_request_handler_proc_p1(lpngx_connection_t c)
{
    CMemory *p_memory = CMemory::GetInstance();

    LPCOMM_PKG_HEADER pPkgHeader;
    pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo; // 正好收到包头时，包头信息肯定是在dataHeadInfo里；

    unsigned short e_pkgLen;
    e_pkgLen = ntohs(pPkgHeader->pkgLen); // 注意这里网络序转本机序，所有传输到网络上的2字节数据，都要用htons()转成网络序，所有从网络上收到的2字节数据，都要用ntohs()转成本机序
                                          // ntohs/htons的目的就是保证不同操作系统数据之间收发的正确性，【不管客户端/服务器是什么操作系统，发送的数字是多少，收到的就是多少】
                                          // 不明白的同学，直接百度搜索"网络字节序" "主机字节序" "c++ 大端" "c++ 小端"
    // 恶意包或者错误包的判断
    if (e_pkgLen < m_iLenPkgHeader)
    {
        // 伪造包/或者包错误，否则整个包长怎么可能比包头还小？（整个包长是包头+包体，就算包体为0字节，那么至少e_pkgLen == m_iLenPkgHeader）
        // 报文总长度 < 包头长度，认定非法用户，废包
        // 状态和接收位置都复原，这些值都有必要，因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数；
        c->curStat = _PKG_HD_INIT;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
    }
    else if (e_pkgLen > (_PKG_MAX_LENGTH - 1000)) // 客户端发来包居然说包长度 > 29000?肯定是恶意包
    {
        // 恶意包，太大，认定非法用户，废包【包头中说这个包总长度这么大，这不行】
        // 状态和接收位置都复原，这些值都有必要，因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数；
        c->curStat = _PKG_HD_INIT;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
    }
    else
    {
        // 合法的包头，继续处理
        // 我现在要分配内存开始收包体，因为包体长度并不是固定的，所以内存肯定要new出来；
        char *pTmpBuffer = (char *)p_memory->AllocMemory(m_iLenMsgHeader + e_pkgLen, false); // 分配内存【长度是 消息头长度  + 包头长度 + 包体长度】，最后参数先给false，表示内存不需要memset;
        c->ifnewrecvMem = true;                                                              // 标记我们new了内存，将来在ngx_free_connection()要回收的
        c->pnewMemPointer = pTmpBuffer;                                                      // 内存开始指针

        // a)先填写消息头内容
        LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
        ptmpMsgHeader->pConn = c;
        ptmpMsgHeader->iCurrsequence = c->iCurrsequence; // 收到包时的连接池中连接序号记录到消息头里来，以备将来用；
        // b)再填写包头内容
        pTmpBuffer += m_iLenMsgHeader;                   // 往后跳，跳过消息头，指向包头
        memcpy(pTmpBuffer, pPkgHeader, m_iLenPkgHeader); // 直接把收到的包头拷贝进来
        if (e_pkgLen == m_iLenPkgHeader)
        {
            // 该报文只有包头无包体【我们允许一个包只有包头，没有包体】
            // 这相当于收完整了，则直接入消息队列待后续业务逻辑线程去处理吧
            ngx_wait_request_handler_proc_plast(c);
        }
        else
        {
            // 开始收包体，注意写法
            c->curStat = _PKG_BD_INIT;                  // 当前状态发生改变，包头刚好收完，准备接收包体
            c->precvbuf = pTmpBuffer + m_iLenPkgHeader; // pTmpBuffer指向包头，这里 + m_iLenPkgHeader后指向包体位置
            c->irecvlen = e_pkgLen - m_iLenPkgHeader;   // e_pkgLen是整个包【包头+包体】大小，-m_iLenPkgHeader【包头】  = 包体
        }
    } // end if(e_pkgLen < m_iLenPkgHeader)

    return;
}

// 收到一个完整包后的处理【plast表示最后阶段】，放到一个函数中，方便调用
void CSocekt::ngx_wait_request_handler_proc_plast(lpngx_connection_t c)
{
    // 把这段内存放到消息队列中来；
    inMsgRecvQueue(c->pnewMemPointer);
    //......这里可能考虑触发业务逻辑，怎么触发业务逻辑，这个代码以后再考虑扩充。。。。。。

    c->ifnewrecvMem = false; // 内存不再需要释放，因为你收完整了包，这个包被上边调用inMsgRecvQueue()移入消息队列，那么释放内存就属于业务逻辑去干，不需要回收连接到连接池中干了
    c->pnewMemPointer = NULL;
    c->curStat = _PKG_HD_INIT;     // 收包状态机的状态恢复为原始态，为收下一个包做准备
    c->precvbuf = c->dataHeadInfo; // 设置好收包的位置
    c->irecvlen = m_iLenPkgHeader; // 设置好要接收数据的大小
    return;
}

//---------------------------------------------------------------
// 当收到一个完整包之后，将完整包入消息队列，这个包在服务器端应该是 消息头+包头+包体 格式
void CSocekt::inMsgRecvQueue(char *buf) // buf这段内存 ： 消息头 + 包头 + 包体
{
    m_MsgRecvQueue.push_back(buf);

    //....其他功能待扩充，这里要记住一点，这里的内存都是要释放的，否则。。。。。。。。。。日后增加释放这些内存的代码
    //...而且逻辑处理应该要引入多线程，所以这里要考虑临界问题
    //....

    // 临时在这里调用一下该函数，以防止接收消息队列过大
    tmpoutMsgRecvQueue(); //.....临时，后续会取消这行代码

    // 为了测试方便，因为本函数意味着收到了一个完整的数据包，所以这里打印一个信息
    ngx_log_stderr(0, "非常好，收到了一个完整的数据包（包头+包体）！");
}

// 临时函数，用于将Msg中消息干掉
void CSocekt::tmpoutMsgRecvQueue()
{
    // 日后可能引入outMsgRecvQueue()，这个函数可能需要临界......
    if (m_MsgRecvQueue.empty()) // 没有消息直接退出
    {
        return;
    }
    int size = m_MsgRecvQueue.size();
    if (size < 1000) // 消息不超过1000条就不处理先
    {
        return;
    }
    // 消息达到1000条
    CMemory *p_memory = CMemory::GetInstance();
    int cha = size - 500;
    for (int i = 0; i < cha; ++i)
    {
        // 一次干掉一堆
        char *sTmpMsgBuf = m_MsgRecvQueue.front(); // 返回第一个元素但不检查元素存在与否
        m_MsgRecvQueue.pop_front();                // 移除第一个元素但不返回
        p_memory->FreeMemory(sTmpMsgBuf);          // 先释放掉把；
    }
    return;
}

// 各种清理函数-------------------------
// 清理接收消息队列，注意这个函数的写法。
void CSocekt::clearMsgRecvQueue()
{
    char *sTmpMempoint;
    CMemory *p_memory = CMemory::GetInstance();

    // 临界与否，日后再考虑，当前先不考虑。。。。。。如果将来有线程池再考虑临界问题
    while (!m_MsgRecvQueue.empty())
    {
        sTmpMempoint = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        p_memory->FreeMemory(sTmpMempoint);
    }
}

// 将socket绑定的地址转换为文本格式【根据参数1给定的信息，获取地址端口字符串，返回这个字符串的长度】
// 参数sa：客户端的ip地址信息一般在这里。
// 参数port：为1，则表示要把端口信息也放到组合成的字符串里，为0，则不包含端口信息
// 参数text：文本写到这里
// 参数len：文本的宽度在这里记录
size_t CSocekt::ngx_sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len)
{
    struct sockaddr_in *sin;
    u_char *p;

    switch (sa->sa_family)
    {
    case AF_INET:
        sin = (struct sockaddr_in *)sa;
        p = (u_char *)&sin->sin_addr;
        if (port) // 端口信息也组合到字符串里
        {
            p = ngx_snprintf(text, len, "%ud.%ud.%ud.%ud:%d", p[0], p[1], p[2], p[3], ntohs(sin->sin_port)); // 返回的是新的可写地址
        }
        else // 不需要组合端口信息到字符串中
        {
            p = ngx_snprintf(text, len, "%ud.%ud.%ud.%ud", p[0], p[1], p[2], p[3]);
        }
        return (p - text);
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

// 从连接池中获取一个空闲连接【当一个客户端连接TCP进入，我希望把这个连接和我的 连接池中的 一个连接【对象】绑到一起，后续 我可以通过这个连接，把这个对象拿到，因为对象里边可以记录各种信息】
lpngx_connection_t CSocekt::ngx_get_connection(int isock)
{
    lpngx_connection_t c = m_pfree_connections; // 空闲连接链表头

    if (c == NULL)
    {
        // 系统应该控制连接数量，防止空闲连接被耗尽，能走到这里，都不正常
        ngx_log_stderr(0, "CSocekt::ngx_get_connection()中空闲链表为空,这不应该!");
        return NULL;
    }

    m_pfree_connections = c->data; // 指向连接池中下一个未用的节点
    m_free_connection_n--;         // 空闲连接少1

    //(1)注意这里的操作,先把c指向的对象中有用的东西搞出来保存成变量，因为这些数据可能有用
    uintptr_t instance = c->instance;          // 常规c->instance在刚构造连接池时这里是1【失效】
    uint64_t iCurrsequence = c->iCurrsequence; // 序号也暂存，后续用于恢复
    //....其他内容再增加

    //(2)把以往有用的数据搞出来后，清空并给适当值
    memset(c, 0, sizeof(ngx_connection_t)); // 注意，类型不要用成lpngx_connection_t，否则就出错了
    c->fd = isock;                          // 套接字要保存起来，这东西具有唯一性
    c->curStat = _PKG_HD_INIT;              // 收包状态处于 初始状态，准备接收数据包头【状态机】

    c->precvbuf = c->dataHeadInfo;         // 收包我要先收到这里来，因为我要先收包头，所以收数据的buff直接就是dataHeadInfo
    c->irecvlen = sizeof(COMM_PKG_HEADER); // 这里指定收数据的长度，这里先要求收包头这么长字节的数据

    c->ifnewrecvMem = false;  // 标记我们并没有new内存，所以不用释放
    c->pnewMemPointer = NULL; // 既然没new内存，那自然指向的内存地址先给NULL
    //....其他内容再增加

    //(3)这个值有用，所以在上边(1)中被保留，没有被清空，这里又把这个值赋回来
    c->instance = !instance; // 抄自官方nginx，到底有啥用，以后再说【分配内存时候，连接池里每个连接对象这个变量给的值都为1，所以这里取反应该是0【有效】；】
    c->iCurrsequence = iCurrsequence;
    ++c->iCurrsequence; // 每次取用该值都增加1

    // wev->write = 1;  这个标记有没有 意义加，后续再研究
    return c;
}

// 归还参数c所代表的连接到到连接池中，注意参数类型是lpngx_connection_t
void CSocekt::ngx_free_connection(lpngx_connection_t c)
{
    if (c->ifnewrecvMem == true)
    {
        // 我们曾经给这个连接分配过内存，则要释放内存
        CMemory::GetInstance()->FreeMemory(c->pnewMemPointer);
        c->pnewMemPointer = NULL;
        c->ifnewrecvMem = false; // 这行有用？
    }

    c->data = m_pfree_connections; // 回收的节点指向原来串起来的空闲链的链头

    // 节点本身也要干一些事
    ++c->iCurrsequence; // 回收后，该值就增加1,以用于判断某些网络事件是否过期【一被释放就立即+1也是有必要的】

    m_pfree_connections = c; // 修改 原来的链头使链头指向新节点
    ++m_free_connection_n;   // 空闲连接多1
    return;
}
