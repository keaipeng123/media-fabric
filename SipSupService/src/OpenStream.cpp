#include"OpenStream.h"
#include"SipDef.h"
#include"SipMessage.h"
#include"GlobalCtl.h"
//#include"Gb28181Session.h"

//rtp负载类型定义
static string rtpmap_ps="96 PS/90000";

OpenStream::OpenStream()
{
    m_pStreamTimer=new TaskTimer(3);
    //m_pCheckSessionTimer=new TaskTimer(5);
}

OpenStream::~OpenStream()
{
    if(m_pStreamTimer)
    {
        delete m_pStreamTimer;
        m_pStreamTimer=NULL;
    }
    // if(m_pCheckSessionTimer)
    // {
    //     delete m_pCheckSessionTimer;
    //     m_pCheckSessionTimer=NULL;
    // }

}

void OpenStream::StreamServiceStart()
{
    //if(m_pStreamTimer && m_pCheckSessionTimer)
    if(m_pStreamTimer)
    {
        m_pStreamTimer->setTimerFun(OpenStream::StreamGetProc,this);
        m_pStreamTimer->start();

        // m_pCheckSessionTimer->setTimerFun(OpenStream::CheckSession,this);
        // m_pCheckSessionTimer->start();
    }
}

// void OpenStream::CheckSession(void* param)
// {
//     AutoMutexLock lck(&GlobalCtl::gStreamLock);
//     GlobalCtl::ListSession::iterator iter=GlobalCtl::glistSession.begin();
//     while(iter!=GlobalCtl::glistSession.end())
//     {
//         timeval currtime;
//         gettimeofday(&currtime,NULL);
//         Session* session=*iter;
//         if(session!=NULL&&currtime.tv_sec-session->m_curTime.tv_sec>10)//10s的时间差，认为rtp中断
//         {
//             //发送sip层的bye
//             OpenStream::StreamStop(session->playformId,session->devid);
//             //再发送rtp层的bye
//             Gb28181Session* gb28181Session=dynamic_cast<Gb28181Session*>(session);
//             //gb28181Session->Destroy();
//             LOG(INFO)<<"gb28181Session->Destroy()";
//             delete gb28181Session;
//             iter=GlobalCtl::glistSession.erase(iter);//从list中删掉 这里用iter接了一下为了防止内存溢出为何？
//         }
//         else
//         {
//             iter++;
//         }
//     }
// }

// void OpenStream::StreamStop(string platformId ,string devId)
// {
//     LOG(INFO)<<"StreamStop";
//     pj_thread_desc desc;
//     pjcall_thread_register(desc);//pjlib注册
//     SipMessage msg;
//     AutoMutexLock lock(&GlobalCtl::globalLock);
//     GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
//     for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
//     {
//         if(iter->sipId==platformId)
//         {
//             msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
//             msg.setTo((char*)devId.c_str(),(char*)iter->addrIp.c_str());
//             msg.setUrl((char*)devId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
//         } 
//         pjsip_tx_data* tdata;
//         pj_str_t from=pj_str(msg.FromHeader());
//         pj_str_t to=pj_str(msg.ToHeader());
//         pj_str_t line=pj_str(msg.RequestUrl());
//         string method="BYE";
//         pjsip_method reqMethod={PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
//         pj_status_t status= pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
//         if(PJ_SUCCESS!=status)
//         {
//             LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
//             return;
//         }

//         status=pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);
//         if(PJ_SUCCESS!=status)
//         {
//             LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
//             return;
//         }
//         return;
//     }
// }

void OpenStream::StreamGetProc(void* param)
{
    if(!GlobalCtl::gRcvIpc)
        return;
    LOG(INFO)<<"gRcvIpc is true";
    //模拟上级客户端请求    
    DeviceInfo info;
    info.devid="11000000001310000059";
    info.playformId="11000000002000000001";
    info.streamName="PlayBack";
    info.startTime=0;
    info.endTime=0;
    info.protocal=0;//0udp 1tcp
    //info.setupType="passive";
    
    // {
    //     AutoMutexLock lck(&GlobalCtl::gStreamLock);
    //     GlobalCtl::ListSession::iterator iter=GlobalCtl::glistSession.begin();
    //     while(iter!=GlobalCtl::glistSession.end())
    //     {
    //         if((*iter)->devid==info.devid&&(*iter)->streamName==info.streamName)
    //         {
    //             return;
    //         }
    //     }
    // }
   

    

    //request INVITE
    //int rtp_port=GBOJ(gConfig)->popOneRandNum();
    SipMessage msg;
    {   //缩小栈空间，即时释放智能锁
        AutoMutexLock lock(&(GlobalCtl::globalLock));
        GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
        for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
        {
            if(iter->sipId==info.playformId)
            {
                msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
                msg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
                msg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
                msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
            }
        }
    }

    pj_str_t from=pj_str(msg.FromHeader());
    pj_str_t to=pj_str(msg.ToHeader());
    pj_str_t contact=pj_str(msg.Contact());
    pj_str_t requestUrl=pj_str(msg.RequestUrl());

    pjsip_dialog* dlg;
    pj_status_t status=pjsip_dlg_create_uac(pjsip_ua_instance(),&from,&contact,&to,&requestUrl,&dlg);//使用用户代理客户端实例创建一个呼叫对话框，uac是用户代理客户端，uac负责向远程服务器发起呼叫，进行呼叫控制，处理媒体流和信令交互等等任务的管理对象（抽象）
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_dlg_create_uac ERROR";
        return;
    }

    //sdp part 这里也可以使用string字符串拼接的方式
    pjmedia_sdp_session* sdp=NULL;
    /*
        Session Description Protocol Version (v): 0
        Owner/Creator, Session Id (o): 33010602001310019325 0 0 IN IP4 10.64.49.44
        Session Name (s): Play
        Connection Information (c): IN IP4 10.64.49.44
        Time Description, active time (t): 0 0
        Media Description, name and address (m): video 5494 RTP/AVP 96
        Media Attribute (a): rtpmap:96 PS/90000
        Media Attribute (a): recvonly
    */
   //pjsip中对结构体内存的创建
   sdp=(pjmedia_sdp_session*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_session));//这里需要内存池poll，内存池poll一般在txdata或rxdata里面，需要将sip初始化处的内存池保存起来
   //Session Description Protocol Version (v): 0
   sdp->origin.version=0;
   // Owner/Creator, Session Id (o): 33010602001310019325 0 0 IN IP4 10.64.49.44
   sdp->origin.user=pj_str((char*)info.devid.c_str());
   sdp->origin.id=0;
   sdp->origin.net_type=pj_str("IN");
   sdp->origin.addr_type=pj_str("IP4");
   sdp->origin.addr=pj_str((char*)GBOJ(gConfig)->sipIp().c_str());//开流端ip
   //Session Name (s): Play
   sdp->name=pj_str((char*)info.streamName.c_str());
   //Connection Information (c): IN IP4 10.64.49.44
   sdp->conn=(pjmedia_sdp_conn*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_conn));
   sdp->conn->net_type=pj_str("IN");
   sdp->conn->addr_type=pj_str("IP4");
   sdp->conn->addr=pj_str((char*)GBOJ(gConfig)->sipIp().c_str());//收流端ip
   //Time Description, active time (t): 0 0
   sdp->time.start=info.startTime;
   sdp->time.stop=info.endTime;

   //Media Description, name and address (m): video 5494 RTP/AVP 96
   sdp->media_count=1;//多个媒体会话的描述index
   pjmedia_sdp_media* m=(pjmedia_sdp_media*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_media));
   sdp->media[0]=m;
   m->desc.media=pj_str("video");
   m->desc.port=20000;
   m->desc.port_count=1;
   if(info.protocal)
   {
        m->desc.transport=pj_str("TCP/RTP/AVP");
   }
   else
   {
        m->desc.transport=pj_str("RTP/AVP");
   }
   m->desc.fmt_count=1;
   m->desc.fmt[0]=pj_str("96");

   // Media Attribute (a): rtpmap:96 PS/90000
   // Media Attribute (a): recvonly
   // Media Attribute (a): setup:active
   m->attr_count=0;
   pjmedia_sdp_attr* attr=(pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
   attr->name=pj_str("rtpmap");
   attr->value=pj_str((char*)rtpmap_ps.c_str());
   m->attr[m->attr_count++]=attr;

   attr=(pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
   attr->name=pj_str("recvonly");
   m->attr[m->attr_count++]=attr;
   if(info.protocal)
   {
        attr=(pjmedia_sdp_attr*)pj_pool_zalloc(GBOJ(gSipServer)->GetPool(),sizeof(pjmedia_sdp_attr));
        attr->name=pj_str("setup");
        attr->value=pj_str((char*)info.setupType.c_str());
        m->attr[m->attr_count++]=attr;
   }

   pjsip_inv_session* inv;
   status=pjsip_inv_create_uac(dlg,sdp,0,&inv);
   if(PJ_SUCCESS!=status)
   {
        pjsip_dlg_terminate(dlg);
        LOG(ERROR)<<"pjsip_inv_create_uac ERROR";
        return;
   }

//    Session* pSession=new Gb28181Session(info);
//    pSession->m_rtpPort=rtp_port;
//    inv->mod_data[0]=(void*)pSession;

   pjsip_tx_data* tdata;
   status=pjsip_inv_invite(inv,&tdata);
   if(PJ_SUCCESS!=status)
   {
        pjsip_dlg_terminate(dlg);
        LOG(ERROR)<<"pjsip_inv_invite ERROR";
        return;
   }

//    pj_str_t subjectName=pj_str("Subject");
//    char subjectBuf[128]={0};
//    sprintf(subjectBuf,"%s:0,%s:0",info.devid.c_str(),GBOJ(gConfig)->sipId().c_str());
//    pj_str_t subjectValue=pj_str(subjectBuf);
//    pjsip_generic_string_hdr* hdr=pjsip_generic_string_hdr_create(GBOJ(gSipServer)->GetPool(),&subjectName,&subjectValue);
//    pjsip_msg_add_hdr(tdata->msg,(pjsip_hdr*)hdr);

//    status=pjsip_inv_send_msg(inv,tdata);
//    if(PJ_SUCCESS!=status)
//    {
//         pjsip_dlg_terminate(dlg);
//         LOG(ERROR)<<"pjsip_inv_send_msg ERROR";
//         return;
//    }


//    AutoMutexLock lck(&GlobalCtl::gStreamLock);
//    GlobalCtl::glistSession.push_back(pSession);

//    GlobalCtl::gRcvIpc=false;
//    //sleep(3);
//    //OpenStream::StreamStop("11000000002000000001","11000000001310000059");


   return;
}