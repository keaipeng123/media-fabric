#ifndef _GLOBALCTL_H
#define _GLOBALCTL_H
#include "Common.h"
#include "SipLocalConfig.h"
#include "ThreadPool.h"
#include "SipCore.h"
#include "SipDef.h"
#include<sys/time.h>
#include<random>
#include<sstream>

class GlobalCtl;
#define GBOJ(obj) GlobalCtl::instance()->obj //宏定义简化单例成员访问

//每个线程在使用PJSIP功能前必须注册
static pj_status_t pjcall_thread_register(pj_thread_desc desc)
{
    pj_thread_t* thread=0;
    if(!pj_thread_is_registered())
    {
        return pj_thread_register(NULL,desc,&thread);
    }
    return PJ_SUCCESS;
}

class Session
{
    public:
    Session(const DeviceInfo& info)
    {
        devid=info.devid;
        playformId=info.playformId;
        streamName=info.streamName;
        setupType=info.setupType;
        protocal=info.protocal;
        startTime=info.startTime;
        endTime=info.endTime;
        gettimeofday(&m_curTime,NULL);
        m_rtpPort=0;
    }
    virtual ~Session(){}
    public:
    string devid;
    string playformId;//中心平台id
    string streamName;//实时流还是回放流
    string setupType; //指定rtp流为tcp时，需要指定setup为active主动或者passive被动
    int protocal;//tcp udp
    int startTime;
    int endTime;
    timeval m_curTime;//检测下级推流异常

    int m_rtpPort;
};

class GlobalCtl
{
    public:
    static GlobalCtl* instance();
    bool init(void *param);//void* 指向任意类型的指针

    SipLocalConfig* gConfig;
    ThreadPool* gThpool =NULL;
    SipCore* gSipServer=NULL;

    typedef struct _SubDomainInfo{
        _SubDomainInfo()
        {
            sipId="";
            addrIp="";
            sipPort=0;
            protocal=0;
            registered=false;
            expires=0;
            lastRegTime = 0;
            auth=false;
        }
        
        bool operator==(string id)
        {
            return (this->sipId==id);
        }

        string sipId;
        string addrIp;
        int sipPort;
        int protocal;
        bool registered;
        int expires;
        time_t lastRegTime;
        bool auth;
    }SubDomainInfo;
    typedef list<SubDomainInfo> SUBDOMAININFOLIST;

    SUBDOMAININFOLIST& getSubDomainInfoList()
    {
        return subDomainInfoList;
    }

    static void get_global_mutex()
    {
        pthread_mutex_lock(&globalLock);
    }

    static void free_global_mutex()
    {
        pthread_mutex_unlock(&globalLock);
    }

    static pthread_mutex_t globalLock;

    typedef list<Session*> ListSession;
    static ListSession glistSession;
    static pthread_mutex_t gStreamLock;

    static bool gStopPool;
    static bool gRcvIpc;

    public:
    static bool checkIsExist(string id);
    static bool checkIsVaild(string id);
    static void setExpires(string id,int expires);
    static void setRegister(string id,bool registered);
    static void setLastRegTime(string id,time_t t);
    static bool getAuth(string id); 
    static DevTypeCode getSipDevInfo(string id);

    static string randomNum(int length);

    private:
    //私有构造函数：防止外部通过 new GlobalCtl() 创建实例
    GlobalCtl(void)
    { 

    }
    ~GlobalCtl(void);
    //禁用拷贝和赋值：避免通过复制生成新实例
    GlobalCtl(const GlobalCtl& global);
    const GlobalCtl& operator=(const GlobalCtl& global);

    static GlobalCtl* m_pInstance;
    static SUBDOMAININFOLIST subDomainInfoList;
    
};
#endif