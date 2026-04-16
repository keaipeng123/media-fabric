#ifndef _SIPRECORDLIST_H
#define _SIPRECORDLIST_H
#include"SipTaskBase.h"
class SipRecordList:public SipTaskBase
{
    public:
    SipRecordList(tinyxml2::XMLElement* root);
    ~SipRecordList();
    virtual pj_status_t run(pjsip_rx_data *rdata);
    void SaveRecordList(int& code);

    private:
    tinyxml2::XMLElement* m_pRootElement;
};

#endif