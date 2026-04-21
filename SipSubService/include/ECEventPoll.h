#ifndef _ECEVENTPOLL_H
#define _ECEVENTPOLL_H
#include<sys/epoll.h>
#include<sys/select.h>
#include<sys/stat.h>
#include<vector>
#include"Common.h"

namespace EC{
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
        ~PollSet(){};
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
    };

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