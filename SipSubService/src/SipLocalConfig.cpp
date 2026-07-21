#include "SipLocalConfig.h"

#define CONFIGFILE_PATH "/home/media-fabric/SipSubService/conf/SipSubService.conf"
#define LOCAL_SECTION "localserver"
#define SIP_SECTION "sipserver"

//被所有的类对象共享且不可修改
static const string keyLocalIp="local_ip";
static const string keyLocalPort="local_port";

static const string keySipId="sip_id";
static const string keySipIp="sip_ip";
static const string keySipPort="sip_port";
static const string keyRtpPortBegin="rtp_port_begin";
static const string keyRtpPortEnd="rtp_port_end";

static const string keySupNodeNum="supnode_num";
static const string keySupNodeId="sip_supnode_id";
static const string keySupNodeIp="sip_supnode_ip";
static const string keySupNodePort="sip_supnode_port";
static const string keySupNodePoto="sip_supnode_poto";
static const string keySupNodeExpires="sip_supnode_expires";
static const string keySupNodeUsr="sip_supnode_usr";
static const string keySupNodePwd="sip_supnode_pwd";
static const string keySupNodeAuth="sip_supnode_auth";
static const string keySupNodeRealm="sip_supnode_realm";


SipLocalConfig::SipLocalConfig()
:m_conf(CONFIGFILE_PATH)
{
    m_localIp="";
    m_localPort=0;
    m_sipId="";
    m_sipIp="";
    m_sipPort=0;
    m_supNodeIp="";
    m_supNodePort=0;
    m_supNodePoto=0;
    m_supNodeAuth=0;
    m_rtpPortBegin=0;
    m_rtpPortEnd=0;
    m_rtpPortLock=PTHREAD_MUTEX_INITIALIZER;

}

SipLocalConfig::~SipLocalConfig()
{

}

int SipLocalConfig::ReadConf()
{
    int ret=0;
    m_conf.setSection(LOCAL_SECTION);
    m_localIp=m_conf.readStr(keyLocalIp);
    if (m_localIp.empty())
    {
        ret=-1;
        LOG(ERROR)<<"localIp is woring";
        return ret;
    }
    m_localPort=m_conf.readInt(keyLocalPort);
    if (m_localPort<=0)
    {
        ret=-1;
        LOG(ERROR)<<"localPort is woring";
        return ret;
    }
    m_conf.setSection(SIP_SECTION);
    m_sipId=m_conf.readStr(keySipId);
    if (m_sipId.empty())
    {
        ret=-1;
        LOG(ERROR)<<"sipId is woring";
        return ret;
    }
    m_sipIp=m_conf.readStr(keySipIp);
    if (m_sipIp.empty())
    {
        ret=-1;
        LOG(ERROR)<<"sipIp is woring";
        return ret;
    }
    m_sipPort=m_conf.readInt(keySipPort);
    if (m_sipPort<=0)
    {
        ret=-1;
        LOG(ERROR)<<"sipPort is woring";
        return ret;
    }
   
    LOG(INFO)<<"localip:"<<m_localIp<<",localPort:"<<m_localPort<<",sipId:"<<m_sipId<<",sipIp:"<<m_sipIp\
    <<",sipPort:"<<m_sipPort;

    m_rtpPortBegin=m_conf.readInt(keyRtpPortBegin);
    if(m_rtpPortBegin<=0)
    {
        ret=-1;
        LOG(ERROR)<<"rtpPortBegin is NULL";
        return ret;
    }

    m_rtpPortEnd=m_conf.readInt(keyRtpPortEnd);
    if(m_rtpPortEnd<=0)
    {
        ret=-1;
        LOG(ERROR)<<"rtpPortEnd is NULL";
        return ret;
    }
    initRandPort();

    int num=m_conf.readInt(keySupNodeNum);
    for(int i=1;i<num+1;++i)
    {
        string id=keySupNodeId+to_string(i);
        string ip=keySupNodeIp+to_string(i);
        string port=keySupNodePort+to_string(i);
        string proto=keySupNodePoto+to_string(i);
        string expires=keySupNodeExpires+to_string(i);
        string usr=keySupNodeUsr+to_string(i);
        string pwd=keySupNodePwd+to_string(i);
        string auth=keySupNodeAuth+to_string(i);
        string realm=keySupNodeRealm+to_string(i);
        

        SupNodeInfo info;
        info.id=m_conf.readStr(id);
        info.ip=m_conf.readStr(ip);
        info.port=m_conf.readInt(port);
        info.poto=m_conf.readInt(proto);
        info.expires=m_conf.readInt(expires);
        info.usr=m_conf.readStr(usr);
        info.pwd=m_conf.readStr(pwd);
        info.auth=m_conf.readInt(auth);
        info.realm=m_conf.readStr(realm);
        upNodeInfoList.push_back(info);
        LOG(INFO)<<"supNodeId:"<<info.id<<"supNodeIp:"<<info.ip<<",supNodePort:"<<info.port<<",supNodePoto:"<<info.poto\
        <<",subNodeExpires:"<<info.expires<<",supNodeUsr:"<<info.usr<<",supNodePwd:"<<info.pwd\
        <<",supNodeAuth:"<<info.auth<<",supNodeRealm:"<<info.realm;
    }

    LOG(INFO)<<"upNodeInfoList.SIZE:"<<upNodeInfoList.size();

    return ret;
}


int SipLocalConfig::initRandPort()
{
    //将队列的起始端口设置为偶数，结束设置为奇数
    //通常rtp为偶数，rtcp为奇数
    while(m_rtpPortBegin%2)
    {
        m_rtpPortBegin++;
    }

    while(m_rtpPortEnd%2==0)
    {
        m_rtpPortEnd--;
    }

    AutoMutexLock lck(&m_rtpPortLock);
    for(int i=m_rtpPortBegin;i<=m_rtpPortEnd;i++)
    {
        m_RandNum.push(i);
    }
    LOG(INFO)<<"m_RandNum.SIZE:"<<m_RandNum.size();
}

int SipLocalConfig::popOneRandNum()
{
    int rtpPort=0;
    AutoMutexLock lck(&m_rtpPortLock);
    if(m_RandNum.size()>0)
    {
        rtpPort=m_RandNum.front();
        m_RandNum.pop();//rtp的
        if(rtpPort%2)//如果取的是奇数，重新取一个偶数
        {
            rtpPort=m_RandNum.front();
            m_RandNum.pop();
        }
        m_RandNum.pop();//rtcp的
    }
    return rtpPort;
}

int SipLocalConfig::pushOneRandNum(int num)
{
    if(num<m_rtpPortBegin||num>m_rtpPortEnd)
    {
        return -1;
    }
    AutoMutexLock lck(&m_rtpPortLock);
    m_RandNum.push(num);
    m_RandNum.push(num+1);
    LOG(INFO)<<"push rtp port:"<<num<<",rtcp port:"<<num+1;
}
