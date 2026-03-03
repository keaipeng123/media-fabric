#ifndef _SIPHEARTBEAT_H
#define _SIPHEARTBEAT_H
#include"SipTaskBase.h"
#include"GlobalCtl.h"
//继承taskbase
class SipHeartBeat:public SipTaskBase
{
    public:
    SipHeartBeat();
    ~SipHeartBeat();
    virtual pj_status_t run(pjsip_rx_data *rdata);
    pj_status_t HeartBeatMessage(pjsip_rx_data *rdata);
};
#endif