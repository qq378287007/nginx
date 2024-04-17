#include "ngx_global.h"
#include "ngx_event.h"

//处理网络事件和定时器事件，我们遵照nginx引入这个同名函数
void ngx_process_events_and_timers()
{
    g_socket.ngx_epoll_process_events(-1); //-1表示卡着等待把

    //...再完善
}

