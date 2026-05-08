#include"EventMsgHandle.h"

void parseReadEvent(struct bufferevent *bev, void *ctx)
{
    LOG(INFO)<<"parseReadEvent";
}
void event_callback(struct bufferevent *bev, short events, void *ctx)
{
    if(events & BEV_EVENT_EOF)
    {
        LOG(ERROR)<<"connection closed";
        bufferevent_free(bev);
    }
}
void onAccept(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *argc)
{
    evutil_make_socket_nonblocking(fd);

    int on=1;
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(on));
    int keepIdle=60;
    int keepInterval=20;
    int keepCount=3;
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,(void*)&keepIdle,sizeof(keepIdle));
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,(void*)&keepInterval,sizeof(keepInterval));
    setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,(void*)&keepCount,sizeof(keepCount));

    event_base* base = (event_base*)argc;
    bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_THREADSAFE);

    bufferevent_setcb(bev,parseReadEvent,NULL,event_callback,NULL);

    bufferevent_enable(bev,EV_READ|EV_PERSIST);

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
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(m_servPort);
    servaddr.sin_addr.s_addr = inet_addr(m_servIp.c_str());

    int ret=evthread_use_pthreads();
    if(ret!=0)
    {
        LOG(ERROR)<<"evthread_use_pthreads failed";
        return -1;  
    }

    event_base* base = event_base_new();

    struct evconnlistener* listener = evconnlistener_new_bind(base,onAccept,base,
        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_THREADSAFE, 1000,
        (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));
    if(listener==nullptr)
    {
        LOG(ERROR)<<"evconnlistener_new_bind failed";
        return -1;
    }

    event_base_dispatch(base); 

    return 0;
}