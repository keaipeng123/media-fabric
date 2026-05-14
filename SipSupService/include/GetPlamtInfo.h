#ifndef GETPLAMTINFO_H
#define GETPLAMTINFO_H
#include "ThreadPool.h"
class GetPlamtInfo:public ThreadTask
{
    public:
        GetPlamtInfo(struct bufferevent* bev);
        ~GetPlamtInfo(){};

        virtual void run();
};

#endif