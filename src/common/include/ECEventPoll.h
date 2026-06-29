#ifndef _ECEVENTPOLL_H
#define _ECEVENTPOLL_H
#ifdef __linux__
#include<sys/epoll.h>
#endif
#include<sys/select.h>
#include<sys/stat.h>
#include<cstdint>
#include<map>
#include<vector>

namespace EC{
    using std::vector;

    enum StatusType
    {
        ST_OK=0,
        ST_SYSERROR,
        ST_TIMEOUT,
        ST_UNINIT,
    };
    enum EventType
    {
        EC_POLLIN=0,//读
        EC_POLLOUT,//写
        EC_POLLERR//错误
    };

    struct PollEventType
    {
        int sockfd; 
        int inEvents;//fd监听的事件
        int outEvents;//已经返回的就绪的事件
    };

    class PollSet
    {
        public:
        PollSet(){};
        virtual ~PollSet(){};//这里如果不是虚析构：常见后果是只调用基类析构，不调用派生类析构，派生类持有的资源就可能泄漏，甚至引发更隐蔽的问题
        virtual int initSet()=0;
        virtual int clearSet()=0;
        virtual int addFd(int sockfd,EventType type)=0;
        virtual int deleteFd(int sockfd)=0;
        virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout)=0;
    };

    class SelectSet:public PollSet
    {
        public:
        SelectSet(){}
        ~SelectSet(){}
        virtual int initSet();
        virtual int clearSet();
        virtual int addFd(int sockfd,EventType type);
        virtual int deleteFd(int sockfd);
        virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout);

        private:
        fd_set _rset;
        fd_set _wset;
        fd_set _eset;

        int _maxfd;
    };

#ifdef __linux__
    class EpollSet:public PollSet
    {
        public:
        EpollSet(){}
        ~EpollSet(){}
        virtual int initSet();
        virtual int clearSet();
        virtual int addFd(int sockfd,EventType type);
        virtual int deleteFd(int sockfd);
        virtual int doSetPoll(vector<PollEventType>& inEvents,vector<PollEventType>& outEvents,int* timeout);
        private:
        int _epollFd;
        std::map<int,uint32_t> _eventMasks;
    };
#endif

    class EventPoll //给用户使用的
    {
        public:
        EventPoll();
        ~EventPoll();
        int init(int method);
        int destory();
        int addEvent(const int& sockfd,EventType type);
        int removeEvent(const int& sockfd);
        StatusType poll(vector<PollEventType>& outEvents,int* timeout);

        private:
        vector<PollEventType> _events;
        PollSet* _pollset;

    };
}

#endif
