#include"EventMsgHandle.h"
#include "SipDef.h"
#include "ThreadPool.h"
#include "GetPlamtInfo.h"
#include"GetCatalog.h"
#include"GlobalCtl.h"
#include "OpenStream.h"

//libevent 的读事件回调函数。当 TCP 客户端发来数据时，libevent 会自动调用这个函数
//struct bufferevent *bev：指向"缓冲事件"对象的指针，封装了一个 socket 的读写缓冲区。
//void *ctx：用户自定义的上下文指针，可以传任意数据进来。
void parseReadEvent(struct bufferevent *bev, void *ctx)
{
    LOG(INFO)<<"parseReadEvent";
    char* buf[1024]={0};//读缓冲区
    int len=bufferevent_read(bev, buf, 4);//从 bev 的输入缓冲区读数据到 buf 中，最多读 sizeof(buf) 字节
    ThreadTask* task=NULL;
    if(len==4)
    {
        int command=*(int*)buf;//把 buf 前 4 字节当成一个整数，得到命令码
        LOG(INFO)<<"recv command:"<<command;
        switch (command)
        {
        case Command_Session_Register:
        {
            LOG(INFO)<<"recv Command_Session_Register";
            task = new GetPlamtInfo(bev);
            break;
        }
        case Command_Session_Catalog:
        {
            LOG(INFO)<<"recv Command_Session_Catalog";
            task = new GetCatalog(bev);
            break;
        }
        case Command_Session_RealPlay:
        {
            LOG(INFO)<<"recv Command_Session_RealPlay";
            task = new OpenStream(bev,&command);
            break;
        }
        
        default:
            return;
        }
        if(task!=NULL)
        {
            GBOJ(gThpool)->postTask(task);//把任务投递到线程池，让线程池的工作线程去执行
        }
    }
}
//libevent 的事件回调函数，当连接上发生"异常事件"（比如断开、出错）时被调用
//short events：一个位掩码，表示发生了什么事件。short 是 2 字节整数类型
void event_callback(struct bufferevent *bev, short events, void *ctx)
{
    //判断 events 中是否包含 "对端关闭连接" 这个标志
    if(events & BEV_EVENT_EOF)
    {
        LOG(ERROR)<<"connection closed";
        bufferevent_free(bev);//释放 bufferevent 对象，回收内存。不释放会造成内存泄漏
    }
}
//接受新连接的的回调函数
void onAccept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *argc)
{
    // address 是 struct sockaddr* 类型，但实际存的是 IPv4 地址，所以需要强制类型转换成 struct sockaddr_in*。
    // ->sin_addr：取出 IP 地址（二进制形式）。
    // inet_ntoa(...)：把二进制的 IP 地址转换成人类可读的点分十进制字符串（比如 "192.168.1.100"）。
    // ->sin_port：取出端口号（网络字节序，即大端序）。
    // ntohs(...)：Network TO Host Short，把网络字节序的端口号转换成本机字节序。
    // fd：文件描述符，操作系统给这个连接分配的一个整数编号。
    LOG(INFO)<<"onAccept client ip:"<<inet_ntoa(((struct sockaddr_in*)address)->sin_addr)<<" port:"<<ntohs(((struct sockaddr_in*)address)->sin_port)<<" fd:"<<fd;
    evutil_make_socket_nonblocking(fd);//设置非阻塞，如果数据还没到，读操作不会卡住等

    //TCP KeepAlive 是心跳检测机制，用来发现"对端已经死掉但没发 FIN 包"的僵死连接
    //setsockopt 的参数模式是：(socket, 协议层, 选项名, 值指针, 值大小)
    // 选项	含义	设置值
    // SO_KEEPALIVE	开启 KeepAlive 总开关	1（开启）
    // TCP_KEEPIDLE	连接空闲多久后开始发探测包	60 秒
    // TCP_KEEPINTVL	探测包之间的间隔	20 秒
    // TCP_KEEPCNT	最多发几个探测包	3 个
    //接空闲 60 秒后开始探测，每 20 秒发一个，发 3 个都没回应就判定连接已死

    int on=1;
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(on));
    int keepIdle=60;
    int keepInterval=20;
    int keepCount=3;
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,(void*)&keepIdle,sizeof(keepIdle));
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,(void*)&keepInterval,sizeof(keepInterval));
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,(void*)&keepCount,sizeof(keepCount));

    // argc 是 onAccept 的参数，实际传的是 event_base* 指针，所以强制转换回去。
    // event_base* base：libevent 的事件循环引擎，所有事件都在上面调度。
    // bufferevent_socket_new：为这个新连接的 fd 创建一个 bufferevent 对象，它自带输入/输出缓冲区。
    // BEV_OPT_CLOSE_ON_FREE：释放 bufferevent 时自动关闭底层 socket。
    // BEV_OPT_THREADSAFE：启用线程安全，多线程环境下可以安全操作这个 bufferevent。
    // | 是按位或，把两个选项合并到一起。
    event_base* base = (event_base*)argc;
    bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_THREADSAFE);

    // 设置回调，四个回调参数分别是：
    //
    // 参数	值	含义
    // 读回调	parseReadEvent	有数据可读时调用
    // 写回调	NULL	写缓冲区低水位时不关心
    // 事件回调	event_callback	发生 EOF/ERROR 时调用∂
    // 用户上下文	NULL	没有额外数据要传
    bufferevent_setcb(bev,parseReadEvent,NULL,event_callback,NULL);

    //启用读事件：EV_READ：监听读事件（有数据到达就触发）EV_PERSIST：持久模式，触发后不会自动移除监听，会一直监听。
    bufferevent_enable(bev,EV_READ|EV_PERSIST);

    // 设置水位线
    // EV_WRITE：设置的是写事件的水位线。
    // 第三个参数 0（低水位）：写缓冲区降到 0 字节时触发写回调。
    // 第四个参数 0（高水位）：写缓冲区没有上限。
    // 这里低水位和高水位都设为 0，实际上等于禁用了写事件的自动触发（因为低水位设为 0 意味着只要缓冲区不满就一直触发，但高水位为 0 又表示无限制，libevent 对此有特殊处理逻辑）。
    bufferevent_setwatermark(bev,EV_WRITE,0,0);
}


EventMsgHandle::EventMsgHandle(const std::string& servIp,const int& servPort)
    :m_servIp(servIp),m_servPort(servPort)
{

}
EventMsgHandle::~EventMsgHandle()
{

}

int EventMsgHandle::init()
{
    //构造服务端地址
    struct sockaddr_in servaddr;//C 语言中表示 IPv4 地址+端口 的结构体
    servaddr.sin_family = AF_INET;//地址族 = IPv4
    servaddr.sin_port = htons(m_servPort);//Host TO Network Short，把本机字节序的端口转成网络字节序（大端）
    servaddr.sin_addr.s_addr = inet_addr(m_servIp.c_str());//把点分十进制 IP 字符串（如 "0.0.0.0"）转成 32 位二进制。.c_str() 把 C++ string 转成 C 风格字符串

    //告诉 libevent 使用 pthread 来做线程同步。这一步必须在调用其他 libevent 函数之前做
    int ret=evthread_use_pthreads();
    if(ret!=0)
    {
        LOG(ERROR)<<"evthread_use_pthreads failed";
        return -1;  
    }

    //创建一个 event_base 对象。可以把它理解为事件调度中心——所有 I/O 事件、定时器事件都注册到它上面，然后它负责轮询和分发
    event_base* base = event_base_new();

    // 创建监听器并绑定端口
    // 参数	值	含义
    // base	事件引擎	把监听器注册到这个引擎上
    // onAccept	回调函数	新连接到达时调用这个函数
    // base	用户上下文	会作为 argc 传给 onAccept
    // LEV_OPT_REUSEABLE	选项	允许重用 TIME_WAIT 状态的端口（快速重启）
    // LEV_OPT_CLOSE_ON_FREE	选项	释放监听器时自动关闭 socket
    // LEV_OPT_THREADSAFE	选项	线程安全
    // 1000	backlog	操作系统等待队列最大长度（排队等待 accept 的连接数）
    // &servaddr	地址	要监听的 IP 和端口
    // sizeof(...)	地址大小	地址结构体的大小
    struct evconnlistener* listener = evconnlistener_new_bind(base,onAccept,base,
        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_THREADSAFE, 1000,
        (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));
    if(listener==nullptr)
    {
        LOG(ERROR)<<"evconnlistener_new_bind failed";
        return -1;
    }

    //启用事件循环
    //调用后程序进入死循环，不断监听事件并调用对应的回调函数。这个函数永远不会返回（除非手动 event_base_loopbreak() 或程序被信号杀死）。所以下面的 return 0 实际上几乎执行不到
    event_base_dispatch(base); 

    return 0;
}