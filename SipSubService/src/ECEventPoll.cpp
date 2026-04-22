#include"ECEventPoll.h"

using namespace EC;

namespace {
bool HasFdInSet(int sockfd, const fd_set& readset, const fd_set& writeset, const fd_set& exceptset)
{
    return FD_ISSET(sockfd, &readset) || FD_ISSET(sockfd, &writeset) || FD_ISSET(sockfd, &exceptset);//检查 fd 是否在集合中、或者 select 返回后是否就绪
}
}

int SelectSet::initSet()
{   
    FD_ZERO(&_rset);//清空集合
    FD_ZERO(&_wset);
    FD_ZERO(&_eset);
    _maxfd=-1;
    return 0;
}
int SelectSet::clearSet()
{
    return initSet();
}
int SelectSet::addFd(int sockfd,EventType type)
{
    if(sockfd<0||sockfd>=FD_SETSIZE)//FD_SETSIZE 最多能表示多大的 fd 编号范围 故设计有缺陷 对小规模 fd 集合还可以，poll 不再依赖这种固定编号位图
    {
        return -1;
    }
    else if(type==EventType::EC_POLLIN)
    {
        FD_SET(sockfd,&_rset);//FD_SET 是 select 相关的一个宏，作用是“把某个文件描述符标记到 fd_set 集合里”
    }
    else if(type==EventType::EC_POLLOUT)
    {
        FD_SET(sockfd,&_wset);
    }
    else if(type==EventType::EC_POLLERR)
    {
        FD_SET(sockfd,&_eset);
    }
    else
    {
        return -1;
    }

    if(sockfd>_maxfd)
    {
        _maxfd=sockfd;
    }
    return 0;
}
int SelectSet::deleteFd(int sockfd)
{
    if(sockfd<0||sockfd>=FD_SETSIZE)
    {
        return -1;
    }

    FD_CLR(sockfd,&_rset);//移除 fd
    FD_CLR(sockfd,&_wset);
    FD_CLR(sockfd,&_eset);

    if(sockfd==_maxfd)
    {
        while(_maxfd>=0&&!HasFdInSet(_maxfd,_rset,_wset,_eset))
        {
            --_maxfd;
        }
    }

    return 0;
}
int SelectSet::doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)
{
    outEvents.clear();//先把上一次轮询结果清掉，准备写本次结果
    if(_maxfd<0)//表示当前一个 fd 都没注册
    {
        return 0;
    }

    struct timeval* ptv=NULL;//超时时间。select等待阻塞的时间
    if(timeout!=NULL&&*timeout>=0)//timeout传入的是毫秒，select 需要 struct timeval 结构体表示秒和微秒，所以要转换一下
    {
        struct timeval tv;
        tv.tv_sec=*timeout/1000;//秒
        tv.tv_usec=(*timeout%1000)*1000;//微秒
        ptv=&tv;
    }
    fd_set readset,writeset,exceptset;
    memcpy(&readset,&_rset,sizeof(fd_set));
    memcpy(&writeset,&_wset,sizeof(fd_set));
    memcpy(&exceptset,&_eset,sizeof(fd_set));

    //select 有一个很重要的特点：它会修改你传进去的 fd_set。
    //调用前：
    //readset 里表示“我关心哪些 fd 的读事件”
    //writeset 里表示“我关心哪些 fd 的写事件”
    //exceptset 里表示“我关心哪些 fd 的异常事件”
    //调用后：
    //这些集合会被内核改写成“本次真正就绪的那些 fd”
    //所以这里不能直接把成员变量 _rset、_wset、_eset 传给 select，否则监听集合本身会被破坏。
    //因此先复制一份临时集合，再把临时集合传给 select。
    int ret=select(_maxfd+1,&readset,&writeset,&exceptset,ptv);//select 调用要求传入“最大 fd + 1”，不是传入 fd 的数量
    if (ret<=0)
    {
        return ret;
    }

    vector<PollEventType>::iterator it=inEvents.begin();
    for(;it!=inEvents.end();it++)
    {
        if(it->sockfd<0||it->sockfd>=FD_SETSIZE)
        {
            continue;
        }

        it->outEvents=-1;
        if(FD_ISSET(it->sockfd,&readset))
        {
            it->outEvents=EC_POLLIN;
        }
        if(FD_ISSET(it->sockfd,&writeset))
        {
            it->outEvents=EC_POLLOUT;
        }
        if(FD_ISSET(it->sockfd,&exceptset))
        {
            it->outEvents=EC_POLLERR;
        }
        if(it->outEvents!=-1)
        {
            outEvents.push_back(*it);
        }
    }
    return ret;
}

int EpollSet::initSet()
{
    _epollFd=epoll_create(1024);//向内核申请创建一个 epoll 实例，并返回一个文件描述符
    return _epollFd;
}
int EpollSet::clearSet()
{
    if(_epollFd!=-1)
    {
        close(_epollFd);
        _epollFd=-1;
    }
    return 0;
}
int EpollSet::addFd(int sockfd,EventType type)
{
    if(sockfd<=0)
    {
        return -1;
    }
    // EPOLL_CTL_ADD 的本质不是“只把 fd 放进去”。
    // 它还要同时告诉内核：你到底关心这个 fd 的哪些事件，比如读、写、异常。
    // 这些信息就放在 epoll_event 里的 events 字段中。
    // 另外 event.data 里还带了用户数据，你这里存的是 fd，本轮 epoll_wait 返回时要靠它识别是谁就绪了。
    struct epoll_event event;
    event.events=0;
    event.data.fd=sockfd;//event.data.fd = sockfd，这样后面 epoll_wait 返回就绪事件时，可以从 events[i].data.fd 拿回对应的 fd

    if(type==EventType::EC_POLLIN)
    {
        event.events=EPOLLIN;//关心的事件
    }
    if(type==EventType::EC_POLLOUT)
    {
        event.events=EPOLLOUT;
    }
    if(type==EventType::EC_POLLERR)
    {
        event.events=EPOLLERR;
    }
    if(event.events==0)
    {
        return -1;
    }
    //第一个参数是 epoll 实例本身，也就是 _epollFd。
    //第二个参数是操作类型。
    //第三个参数是你要操作的目标 fd，也就是 sockfd。
    //第四个参数是附加信息，主要在新增或修改监听时使用。
    return epoll_ctl(_epollFd,EPOLL_CTL_ADD,sockfd,&event);//把这个 sockfd 加入 epoll 监听集合
   

}
int EpollSet::deleteFd(int sockfd)
{
    //struct epoll_event event;
    return epoll_ctl(_epollFd,EPOLL_CTL_DEL,sockfd,NULL);
}
int EpollSet::doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)
{
    int tv=-1;
    if(timeout!=NULL&&*timeout>=0)
    {
        tv=*timeout;
    }
    const int MAX_EVENTS=10;
    struct epoll_event events[MAX_EVENTS];
    int evCount=epoll_wait(_epollFd,events,MAX_EVENTS,tv);//让当前线程在 epoll 实例上等待事件发生，并把已经就绪的事件结果写到 events 数组里
    if(evCount<=0)
    {
        return evCount;
    }

    outEvents.clear();
    vector<PollEventType>::iterator it=inEvents.begin();
    for(;it!=inEvents.end();it++)
    {
        it->outEvents=-1;
        for(int i=0;i<evCount;i++)
        {
            if(it->sockfd==events[i].data.fd)
            {
                if(events[i].events&(EPOLLERR|EPOLLHUP))
                {
                    it->outEvents=EC_POLLERR;
                }
                else if(events[i].events&EPOLLIN)
                {
                    it->outEvents=EC_POLLIN;
                }
                else if(events[i].events&EPOLLOUT)
                {
                    it->outEvents=EC_POLLOUT;
                }
            }
        }
       
        if(it->outEvents!=-1)
        {
            outEvents.push_back(*it);
        }
    }
    return evCount;
}

EventPoll::EventPoll()
{
    _events.clear();
    _pollset=NULL;
}
EventPoll::~EventPoll()
{
    this->destory();
}
int EventPoll::init(int method)
{
    if(method==1)
    {
        _pollset=new SelectSet();
    }
    else if(method==2)
    {
        _pollset=new EpollSet();
    }
    else
    {
        return -1;
    }
    if(_pollset)
    {
        _pollset->initSet();
    }
    return 0;
}
int EventPoll::destory()
{
    if(_pollset!=NULL)
    {
        _pollset->clearSet();
        delete _pollset;
        _pollset=NULL;
    }
    _events.clear();
    return 0;
}
int EventPoll::addEvent(const int& sockfd,EventType type)
{
    if(_pollset==NULL)
    {
        return -1;
    }
    if(_pollset->addFd(sockfd,type)!=0)
    {
        return -1;
    }

    PollEventType ev;
    ev.inEvents=type;
    ev.outEvents=-1;
    ev.sockfd=sockfd;
    _events.push_back(ev);
    return 0;
}
int EventPoll::removeEvent(const int& sockfd)
{
    if(_pollset==NULL)
    {
        return -1;
    }
    _pollset->deleteFd(sockfd);
    vector<PollEventType>::iterator it=_events.begin();
    for(;it!=_events.end();)
    {
        if(it->sockfd==sockfd)
        {
            _events.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return 0;
}
StatusType EventPoll::poll(vector<PollEventType>& outEvents,int* timeout)
{
    if(_pollset==NULL)
    {
        return ST_UNINIT;
    }
    int ret= _pollset->doSetPoll(_events,outEvents,timeout);
    if(ret==0)
    {
        return ST_TIMEOUT;
    }
    else if(ret<0)
    {
        return ST_SYSERROR;
    }
    return ST_OK;
}