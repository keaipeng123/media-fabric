#include"SipHeartBeat.h"
#include"Common.h"
#include "SipMessage.h"
//#include"XmlParser.h"
//#include<string>

int SipHeartBeat::m_snIndex=0;

static void response_callback(void *token, pjsip_event *e)
{
    pjsip_transaction* tsx=e->body.tsx_state.tsx;
    GlobalCtl::SupDomainInfo* node=(GlobalCtl::SupDomainInfo*)token;
    if(tsx->status_code!=200)
    {
        node->registered=false;
    }
    return;
}


SipHeartBeat::SipHeartBeat()
{
    m_heartTimer=new TaskTimer(10);
}
SipHeartBeat::~SipHeartBeat()
{
    if(m_heartTimer)
    {
        delete m_heartTimer;
        m_heartTimer=NULL;
    }
}

void SipHeartBeat::gbHeartBeatServiceStart()
{
    if(m_heartTimer)
    {
        m_heartTimer->setTimerFun(HeartBeatProc,this);
        m_heartTimer->start();
    }
}

void SipHeartBeat::HeartBeatProc(void* param)
{
    SipHeartBeat* pthis=(SipHeartBeat*)param;
    //GlobalCtl::get_global_mutex();
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
    {
        if(iter->registered)
        {
            if((pthis->gbHeartBeat(*iter))<0)
            {
                LOG(ERROR)<<"keep alive error for:"<<iter->sipId;
            }
        }
    }
    //GlobalCtl::free_global_mutex();
}

int SipHeartBeat::gbHeartBeat(GlobalCtl::SupDomainInfo& node)
{
    SipMessage msg;
    msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());//From字段 本级信息
    
    msg.setTo((char*)node.sipId.c_str(),(char*)node.addrIp.c_str());//To字段 本级信息 下级向上级注册的时候，to字段是告诉上级要回复的地址（gb注册的时候是本级，其他都是对端）
   
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort);
    
    
    pj_str_t from =pj_str(msg.FromHeader());
    pj_str_t to=pj_str(msg.ToHeader());
    pj_str_t line=pj_str(msg.RequestUrl());
    string method="MESSAGE";
    pjsip_method reqMethod={PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
    pjsip_tx_data* tdata;
    pj_status_t status=pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
        return -1;
    }

    char strIndex[16]={0};
    sprintf(strIndex,"%d",m_snIndex++);
    #if 1
    string keepalive="<?xml version=\"1.0\"?>\r\n";
    keepalive+="<Notify>\r\n";
    keepalive+="<CmdType>keepalive</CmdType>\r\n";
    keepalive+="<SN>";
    keepalive+=strIndex;
    keepalive+="</SN>\r\n";
    keepalive+="<DeviceID>";
    keepalive+=GBOJ(gConfig)->sipId();
    keepalive+="</DeviceID>\r\n";
    keepalive+="<Status>OK</Status>\r\n";
    keepalive+="</Notify>\r\n";
    #endif

    // //XmlParser* m_heartXml =new XmlParser();//堆上创建，需要手动释放
    // XmlParser parse;//栈上创建
    // tinyxml2::XMLElement* rootNode=parse.AddRootNode((char*)"Notify");
    // parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"keepalive");
    // parse.InsertSubNode(rootNode,(char*)"SN",strIndex);
    // parse.InsertSubNode(rootNode,(char*)"DeviceID",(char*)GBOJ(gConfig)->sipId().c_str());
    // parse.InsertSubNode(rootNode,(char*)"Status",(char*)"OK");
    // char* xmlbuf=new char[1024];
    // memset(xmlbuf,0,1024);
    // parse.getXmlData(xmlbuf);
    // //LOG(INFO)<<"getXmlData:"<<xmlbuf;
    

    pj_str_t type =pj_str("Application");
    pj_str_t subtype=pj_str("MANSCDP+xml");
    //pj_str_t xmldata=pj_str(xmlbuf);
    pj_str_t xmldata=pj_str((char*)keepalive.c_str());
    tdata->msg->body=pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);
    status=pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,&node,&response_callback);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
        return -1;
    }
    return 0;
}