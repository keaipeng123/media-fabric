#include "GlobalCtl.h"
GlobalCtl::SUBDOMAININFOLIST GlobalCtl::subDomainInfoList;
pthread_mutex_t GlobalCtl::globalLock=PTHREAD_MUTEX_INITIALIZER;//宏，用于静态初始化一个互斥锁。它相当于给这个锁赋一个默认的初始状态（未锁定、可用）。
bool GlobalCtl::gStopPool=false;
GlobalCtl* GlobalCtl::m_pInstance=NULL;

GlobalCtl* GlobalCtl::instance()
{
    if(!m_pInstance)
    {
        m_pInstance = new GlobalCtl();
    }
    return m_pInstance;
}

bool GlobalCtl::init(void *param)
{
    gConfig=(SipLocalConfig*)param;
    if (!gConfig)
    {
        return false;
    }

    SubDomainInfo info;
    auto iter =gConfig->ubNodeInfoList.begin();//auto自动类型判断
    for(;iter != gConfig->ubNodeInfoList.end();++iter)
    {
        info.sipId=iter->id;
        info.addrIp=iter->ip;
        info.sipPort=iter->port;
        info.protocal=iter->poto;
        info.auth=iter->auth;
        subDomainInfoList.push_back(info);
    }

    LOG(INFO)<<"subDomainInfoList.SIZE:"<<subDomainInfoList.size();

    if (!gThpool)
    {
        gThpool=new ThreadPool();
        gThpool->createThreadPool(10);
    }
    if(!gSipServer)
    {
        gSipServer=new SipCore();
    }
    gSipServer->InitSip(gConfig->sipPort());
     
    return true;

}

bool GlobalCtl::checkIsExist(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end())
    {
        return true;
    }
    return false;
}

bool GlobalCtl::checkIsVaild(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end()&&it->registered)
    {
        return true;
    }
    return false;
}


void GlobalCtl::setExpires(string id,int expires)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end())
    {
        it->expires=expires;
    }
}

void GlobalCtl::setRegister(string id,bool registered)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        it->registered = registered;
    }
}

void GlobalCtl::setLastRegTime(string id,time_t t)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it = std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it != subDomainInfoList.end())
    {
        it->lastRegTime = t;
    }
}

bool GlobalCtl::getAuth(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end())
    {
        return it->auth;
    }
}

string GlobalCtl::randomNum(int length)
{
    #if 0
    //随机数种子
    random_device rd;
    //随机数生成器
    mt19937 gen(rd());
    //分布器类模板 设定一个0-15的均匀整数分布的区间范围
    uniform_int_distribution<> dis(0,15);
    stringstream ss;
    for(int i=0;i<length;++i)
    {
        //每次使用随机数生成器在指定的分布范围内获取随机数
        int value=dis(gen);
        ss<<std::hex<<value;
    }
    #endif
    stringstream ss;
    for(int i=0;i<length;++i)
    {
        int value=random()%16;
        ss<<std::hex<<value;
    }
    return ss.str();
    

}