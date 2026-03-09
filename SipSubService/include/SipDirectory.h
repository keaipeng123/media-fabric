#ifndef _SIPDIRECTORY_H
#define _SIPDIRECTORY_H
#include "SipTaskBase.h"
class SipDirectory:public SipTaskBase
{
    public:
    SipDirectory();
    ~SipDirectory();
    virtual void run(pjsip_rx_data *rdata); 
    void resDir(pjsip_rx_data *rdata,int* sn);
    void directoryQuery(Json::Value& jsonOut);

    void constructMANSCDPXml(Json::Value listdata,int* begin,int itemCount,char* sendData,int sn);

    void sendSipDirMsg(pjsip_rx_data *rdata,char* sendData);
};


#endif