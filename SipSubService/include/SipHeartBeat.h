#ifndef _SIPHEARTBEAT_H
#define _SIPHEARTBEAT_H
#include "TaskTimer.h"
#include"GlobalCtl.h"
class SipHeartBeat
{
    public:
    SipHeartBeat();
    ~SipHeartBeat();

    void gbHeartBeatServiceStart();
    static void HeartBeatProc(void* param);
    int gbHeartBeat(GlobalCtl::SupDomainInfo& node);

    private:
    TaskTimer* m_heartTimer;
    static int m_snIndex;
};
#endif