#include "SipRegister.h"
#include "Common.h"
#include "SipMessage.h"

//回调函数不能有this指针，全局用static
static void  client_cb(struct pjsip_regc_cbparam *param)
{
    LOG(INFO)<<"code:"<<param->code;
    if(param->code==200)
    {
        GlobalCtl::SupDomainInfo* subinfo=(GlobalCtl::SupDomainInfo*)param->token;
        subinfo->registered=true;
    }
    return;
}

SipRegister::SipRegister()
{
    m_regTimer=new TaskTimer(3);
//    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
//     for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
//     {
//         if(!(iter->registered))
//         {
//             if (gbRegister(*iter) < 0)
//             {
//                 LOG(ERROR)<<"register error for:"<<iter->sipId;
//             }
//         } 
//     }
   
}
SipRegister::~SipRegister()
{
    if(m_regTimer)
    {
        delete m_regTimer;
        m_regTimer=NULL; 
    }
}

void SipRegister::registerServiceStart()
{
    if(m_regTimer)
    {
        m_regTimer->setTimerFun(SipRegister::RegisterProc,(void*)this);
        m_regTimer->start();
    } 
}

void SipRegister::RegisterProc(void* param)
{
    SipRegister* pthis=(SipRegister*)param;
    //GlobalCtl::get_global_mutex();
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
    {
        if(!(iter->registered))
        {
            if((pthis->gbRegister(*iter))<0)
            {
                LOG(ERROR)<<"register error for:"<<iter->sipId;
            }
        }
    }
    //GlobalCtl::free_global_mutex();
}



int SipRegister::gbRegister(GlobalCtl::SupDomainInfo& node)
{
    SipMessage msg;
    //From: <sip:11000000002000000001@127.0.1>;tag=2mdIf8lexOTBIMg2pPgKOBdDB3SowCcf
    msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());//From字段 本级信息
    //To: <sip:11000000002000000001@127.0.1>
    msg.setTo((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());//To字段 本级信息 下级向上级注册的时候，to字段是告诉上级要回复的地址（gb注册的时候是本级，其他都是对端）

    //Request-Line字段
    if (node.protocal==1)
    {
    //Request-Line: REGISTER sip:10000000002000000001@127.0.1:5061;transport=tcp SIP/2.0
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort,(char*)"tcp");
    }
    else
    {
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort);
    }
    //Contact: <sip:11000000002000000001@127.0.1:7101>
    msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());

    pj_str_t from =pj_str(msg.FromHeader());
    pj_str_t to=pj_str(msg.ToHeader());
    pj_str_t line=pj_str(msg.RequestUrl());
    pj_str_t contact=pj_str(msg.Contact());

    pj_status_t status=PJ_SUCCESS;

    do
    {
        pjsip_regc* regc;
        status=pjsip_regc_create(GBOJ(gSipServer)->GetEndPoint(),&node,&client_cb,&regc);//client_cb注册后接收注册结果的回调函数
        if(PJ_SUCCESS!=status)
        {   
            LOG(ERROR)<<"pjsip_regc_create error";
            break;
        }
        status=pjsip_regc_init(regc,&line,&from,&to,1,&contact,node.expires);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_init error";
            break;
        }

        if(node.isAuth)
        {
            //pjsip 的 pjsip_regc_set_credentials() 只是把凭据保存到 regc 对象里，第一次发送时 pjsip 并不会把它们放进报文；收到 401 后内部自动计算 response 再重发
            pjsip_cred_info cred;//鉴权信息结构体
            pj_bzero(&cred,sizeof(pjsip_cred_info));
            cred.scheme=pj_str("digest");
            //string realm_str=node.realm;
            cred.realm=pj_str((char*)node.realm.c_str());
            string usr_str=node.usr;
            cred.username=pj_str((char*)usr_str.c_str());
            cred.data_type=PJSIP_CRED_DATA_PLAIN_PASSWD;
            string pwd_str=node.pwd;
            //LOG(ERROR)<<"usr:"<<usr_str<<",realm:"<<realm_str<<",pwd:"<<pwd_str;
            cred.data=pj_str((char*)pwd_str.c_str());

            status=pjsip_regc_set_credentials(regc,1,&cred);
            if(PJ_SUCCESS!=status)
            {
                pjsip_regc_destroy(regc);
                LOG(ERROR)<<"pjsip_regc_set_credentials error";
                break;
            }
        }

        pjsip_tx_data* tdata=NULL;
        status=pjsip_regc_register(regc,PJ_TRUE,&tdata);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_register error";
            break;
        }
        status=pjsip_regc_send(regc,tdata);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_send error";
            break;
        }

    } while (0);

    int ret=0;
    if(PJ_SUCCESS!=status)
    {
        ret=-1;
    }
    return ret;

}