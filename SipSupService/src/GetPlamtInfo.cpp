#include"GetPlamtInfo.h"
#include"GlobalCtl.h"

GetPlamtInfo::GetPlamtInfo(struct bufferevent* bev)
:ThreadTask(bev)
{

}

void GetPlamtInfo::run()
{
    Json::Value jsonIn;
    int index=0;
    LOG(INFO)<<"GetPlamtInfo run";
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
    {
        if(iter->registered)
        {
            jsonIn["subDomain"][index]["sipId"]=iter->sipId;
            index++;
        }
    }
    Json::FastWriter fast_writer;//把 Json::Value 对象转成紧凑的 JSON 字符串（无换行无缩进）
    string payLoadIn = fast_writer.write(jsonIn);
    int bodyLen=payLoadIn.length();
    int len=sizeof(int)+bodyLen;
    char* buf=new char[len];
    memset(buf,0,len);
    memcpy(buf,&bodyLen,sizeof(int));
    memcpy(buf+sizeof(int),payLoadIn.c_str(),bodyLen);
    bufferevent_write(m_bev,buf,len);
    delete[] buf;
}