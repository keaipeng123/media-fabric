#ifndef _OPENSTREAM_H
#define _OPENSTREAM_H
#include"TaskTimer.h"
#include"ThreadPool.h"
#include"Common.h"

class OpenStream:public ThreadTask
{
    public:
    OpenStream(struct bufferevent* bev,void* arg);
    ~OpenStream();
    virtual void run();

    //void StreamServiceStart();
    void StreamGetProc(void* param);
    //static void CheckSession(void* param);//检查rtp推流异常断开
    void StreamStop(string platformId ,string devId);

    private:
    TaskTimer *m_pStreamTimer;
    TaskTimer *m_pCheckSessionTimer;//检查rtp推流异常断开
};

#endif