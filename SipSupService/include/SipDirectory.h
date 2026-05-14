#ifndef _SIPDIRECTORY_H
#define _SIPDIRECTORY_H
#include"SipTaskBase.h"
class SipDirectory: public SipTaskBase
{
    public:
    SipDirectory(tinyxml2::XMLElement* root);//这里不是默认构造，所以不会隐式调用基类的默认构造，需要手动调用基类的默认构造
    ~SipDirectory();
    virtual pj_status_t run(pjsip_rx_data *rdata);
    void SaveDir(int& status_code);

    private:
    tinyxml2::XMLElement* m_pRootElement;
    static Json::Value m_jsonIn;
    static int m_jsonInIndex;
};
#endif