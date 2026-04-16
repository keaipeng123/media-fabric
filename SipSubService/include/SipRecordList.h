#ifndef _SIPRECORDLIST_H
#define _SIPRECORDLIST_H
#include "SipTaskBase.h"

class SipRecordList:public SipTaskBase
{
    public:
    SipRecordList();
    ~SipRecordList();

    virtual void run(pjsip_rx_data *rdata);
    void resRecordInfo(pjsip_rx_data *rdata,int& sn,string& devid);

    void sendRecordList(pjsip_rx_data *rdata,int sn,string devid);

};

#endif