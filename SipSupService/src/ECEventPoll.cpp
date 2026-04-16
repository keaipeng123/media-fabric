#include"ECEventPoll.h"

using namespace EC;
int SelectSet::initSet()
{   
    FD_ZERO(&_rset);
    FD_ZERO(&_wset);
    FD_ZERO(&_eset);
    _maxfd=0;
    return 0;
}
int SelectSet::clearSet()
{
    initSet();
}
int SelectSet::addFd(int sockfd,EventType type)
{
    if(sockfd==-1)
    {
        return -1;
    }
    if(type==EventType::EC_POLLIN)
    {
        FD_SET(sockfd,&_rset);
    }
    if(type==EventType::EC_POLLOUT)
    {
        FD_SET(sockfd,&_wset);
    }
    if(type==EventType::EC_POLLERR)
    {
        FD_SET(sockfd,&_eset);
    }
    _maxfd=sockfd;
}
int SelectSet::deleteFd(int sockfd)
{
    FD_CLR(sockfd,&_rset);
    FD_CLR(sockfd,&_wset);
    FD_CLR(sockfd,&_eset);

    return 0;
}
int SelectSet::doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)
{
    struct timeval* ptv=NULL;//超时时间
    if(timeout!=NULL&&*timeout>=0)
    {
        struct timeval tv;
        tv.tv_sec=*timeout/1000;
        tv.tv_usec=(*timeout%1000)*1000;
        ptv=&tv;
    }
    fd_set readset,writeset,exceptset;
    memcpy(&readset,&_rset,sizeof(fd_set));
    memcpy(&writeset,&_wset,sizeof(fd_set));
    memcpy(&exceptset,&_eset,sizeof(fd_set));

    int ret=select(_maxfd+1,&readset,&writeset,&exceptset,ptv);
    if (ret<=0)
    {
        return ret;
    }

    outEvents.clear();
    vector<PollEventType>::iterator it=inEvents.begin();
    for(;it!=inEvents.end();it++)
    {
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
    _epollFd=epoll_create(1024);
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
    struct epoll_event event;
    event.events=-1;
    event.data.fd=sockfd;

    if(type==EventType::EC_POLLIN)
    {
        event.events=EPOLLIN;
    }
    if(type==EventType::EC_POLLOUT)
    {
        event.events=EPOLLOUT;
    }
    if(type==EventType::EC_POLLERR)
    {
        event.events=EPOLLERR;
    }
    if(event.events==-1)
    {
        return -1;
    }

    return epoll_ctl(_epollFd,EPOLL_CTL_ADD,sockfd,&event);
   

}
int EpollSet::deleteFd(int sockfd)
{
    struct epoll_event event;
    return epoll_ctl(_epollFd,EPOLL_CTL_DEL,sockfd,&event);
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
    int evCount=epoll_wait(_epollFd,events,MAX_EVENTS,tv);
    if(evCount<=0)
    {
        return evCount;
    }

    outEvents.clear();
    vector<PollEventType>::iterator it=inEvents.begin();
    for(;it!=inEvents.end();it++)
    {
        it->outEvents=0;
        for(int i=0;i<evCount;i++)
        {
            if(it->sockfd==events[i].data.fd)
            {
                if(events[i].events==EPOLLIN)
                {
                    it->outEvents=EC_POLLIN;
                }
                else if(events[i].events==EPOLLOUT)
                {
                    it->outEvents=EC_POLLOUT;
                }
                else if(events[i].events==EPOLLERR)
                {
                    it->outEvents=EC_POLLERR;
                }
            }
        }
       
        if(it->outEvents!=0)
        {
            outEvents.push_back(*it);
        }
    }
    return 0;
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
    if(method=1)
    {
        _pollset=new SelectSet();
    }
    else if(method=2)
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
}
int EventPoll::addEvent(const int& sockfd,EventType type)
{
    if(_pollset==NULL)
    {
        return -1;
    }
    _pollset->addFd(sockfd,type);

    PollEventType ev;
    ev.inEvents=type;
    ev.outEvents=0;
    ev.sockfd=sockfd;
    _events.push_back(ev);
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