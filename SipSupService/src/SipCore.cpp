#include "SipCore.h"
#include "Common.h"
#include "SipDef.h"
#include "GlobalCtl.h"
#include "SipTaskBase.h"
#include "SipRegister.h"
#include "SipHeartBeat.h"
#include "ECThread.h"
#include "SipDirectory.h"  
#include "SipGbPlay.h"

using namespace EC;

static int pollingEvent(void* arg)
{
    LOG(INFO)<<"polling event thread start success";
    pjsip_endpoint* ept = (pjsip_endpoint*)arg;
    while(!(GlobalCtl::gStopPool))
    {
        pj_time_val timeout = {0,500};
        pj_status_t status = pjsip_endpt_handle_events(ept,&timeout);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"polling events failed,code:"<<status;
            return -1;
        }
    }
    return 0;
}

//rx 接收
pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    LOG(INFO)<<"onRxRequest come in";
    if(NULL==rdata||NULL==rdata->msg_info.msg)
    {
        return PJ_FALSE;
    }
    threadParam* param=new threadParam();
    pjsip_msg* msg=rdata->msg_info.msg;
    pjsip_rx_data_clone(rdata,0,&param->data);
    if(msg->line.req.method.id==PJSIP_REGISTER_METHOD)
    {
        param->base=new SipRegister();
    }
    else if(msg->line.req.method.id==PJSIP_OTHER_METHOD)
    {
        // tinyxml2::XMLDocument* pxmlDoc=NULL;
        // pxmlDoc=new tinyxml2::XMLDocument();
        // if(pxmlDoc)
        // {
        //     pxmlDoc->Parse((char*)msg->body->data);
        // }
        // tinyxml2::XMLElement* pRootElement=pxmlDoc->RootElement();
        // string rootType = pRootElement->Value();
        // LOG(INFO)<<"rootType:"<<rootType;
        // tinyxml2::XMLElement* cmdElement=pRootElement->FirstChildElement("CmdType");
        // string cmdType;
        // if (cmdElement&&cmdElement->GetText())
        // {
        //     cmdType=cmdElement->GetText();
        //     // if(rootType==SIP_NOTIFY&&cmdType==SIP_HEARTBEAT)
        //     // {
        //     //     param->base=new SipHeartBeat();
        //     // }
        // }
        //LOG(INFO)<<"cmdType:"<<cmdType;
        string rootType = "",cmdType = "CmdType",cmdValue;
        tinyxml2::XMLElement* root = SipTaskBase::parseXmlData(msg,rootType,cmdType,cmdValue);
        LOG(INFO)<<"rootType:"<<rootType;
        LOG(INFO)<<"cmdValue:"<<cmdValue;
        if(rootType == SIP_NOTIFY && cmdValue == SIP_HEARTBEAT)
        {
            param->base=new SipHeartBeat();
        }
        else if(rootType==SIP_RESPONSE)
        {
            if(cmdValue==SIP_CATALOG)
            {
                param->base=new SipDirectory(root);
            }
            else if(cmdValue==SIP_RECORDINFO)
            {
                //param->base=new SipRecordList(root);
            }
        }
    }
    pthread_t pid;
    int ret=EC::ECThread::createThread(SipCore::dealTaskThread,param,pid);
    if(ret!=0)
    {
        LOG(ERROR)<<"create task thread error";
        if(param)
        {
            delete param;
            param=NULL;
        }
        return PJ_FALSE;
    }
    return PJ_SUCCESS;
}

static pjsip_module recv_mod=
{
    NULL,NULL,//链表操作，指定模块儿的上下层
    {"mod-recv",8},//name 字符串长度
    -1,//id
    PJSIP_MOD_PRIORITY_APPLICATION,//优先级设置为应用层
    NULL,
    NULL,
    NULL,
    NULL,//四个回调，此处均置为空，暂不需要(load,start,stop,unload)
    onRxRequest,//接收请求消息的回调
    NULL,//接收响应消息的回调，这个回调会返回响应码，如果这里指定，还需要知道响应码对应的请求事件，后面会在每次事务请求时立马获取到对应的响应码，这里不指定
    NULL,//tx 发送请求消息之前的回调
    NULL,//发送响应消息之前的回调，检查之用
    NULL,//事务状态变更回调
};

void* SipCore::dealTaskThread(void* arg)
{
    threadParam* param = (threadParam*)arg;
    if(!param || param->base == NULL)
    {
        return NULL;
    }
    /*
    | 线程来源                                 | 是否已注册 | 举例                                                       |
| ------------------------------------ | ----- | -------------------------------------------------------- |
| PJSIP 内部创建                           | ✅ 已注册 | `pj_thread_create()`、`pjsip_endpt_handle_events()` 所在的线程 |
| 外部框架 / 自己写的 `ECThread::createThread` | ❌ 未注册 | `SipCore::dealTaskThread`                                |
 是你自己的跨平台线程封装（或第三方库），PJSIP 根本不知道它的存在，因此 PJLIB TLS 尚未初始化，任何 PJSIP API 都会触发：
Assertion failed: !"Calling PJLIB from unknown/external thread"

    */
    pj_thread_desc desc;
    pjcall_thread_register(desc);
    param->base->run(param->data);
    delete param;
    param=NULL;
}


SipCore::SipCore()
:m_endpt(NULL)
{

}
SipCore::~SipCore()
{
    pjmedia_endpt_destroy(m_mediaEndpt);
    pjsip_endpt_destroy(m_endpt);
    pj_caching_pool_destroy(&m_cachingPool);
    pj_shutdown();
    GlobalCtl::gStopPool=true;
}

bool SipCore::InitSip(int sipPort)
{
    pj_status_t status;
    //0-关闭 6-详细
    pj_log_set_level(0);
    do
    {
        //【目的】初始化 pjlib 核心。
        //【详解】该调用会初始化 PJLIB 的全局静态数据以及部分平台相关能力；这是后续使用 PJLIB/PJSIP 的前置条件。
        status=pj_init();
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init pjlib faild,code:"<<status;
            break;
        }
        //【目的】初始化 pjlib-util 组件。
        //【详解】它提供 DNS、XML、扫描器、哈希/摘要、STUN 等通用工具；这是上层某些 SIP 辅助能力的基础，但不等同于初始化 ICE。
        status=pjlib_util_init();
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init pjlib util faild,code:"<<status;
            break;
        }
        //缓存池,pjsip中的内存均是这个缓存池分配的，所以需要将该缓存池储存起来，不能销毁,
        //此处定义了一个栈对象，当函数结束这个对象也就释放了；cachingPool需要和sipCore对象的生命周期一致，所以将其放在类的成员中
        //pj_caching_pool cachingPool;
        pj_caching_pool_init(&m_cachingPool,NULL,SIP_STACK_SIZE);

        //【目的】创建并返回 SIP 端点对象
        //【详解】端点是整个 SIP 栈的核心句柄，后续所有模块（事务层、UA 层、传输层、定时器、消息调度）都要挂到它上面。
        status=pjsip_endpt_create(&m_cachingPool.factory,NULL,&m_endpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"create endpt faild,code:"<<status;
            break;
        }

        //【目的】获取 SIP endpoint 当前使用的 ioqueue。
        //【详解】把这个 ioqueue 传给 pjmedia 后，媒体层的 RTP/RTCP socket 就可以复用同一套事件轮询机制；它不要求一定是“端点内部新建”的队列。
        pj_ioqueue_t* ioqueue=pjsip_endpt_get_ioqueue(m_endpt);
        //【目的】创建 pjmedia 端点
        //【详解】该接口会初始化音频子系统并创建媒体端点对象；媒体端点持有 codec manager，并可基于传入的 ioqueue 管理 RTP/RTCP 所需的事件处理。
        status=pjmedia_endpt_create(&m_cachingPool.factory,ioqueue,0,&m_mediaEndpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"create media endpoint faild,code:"<<status;
            break;
        }

        //会话事务模块
        //【目的】注册事务层模块。
        //【详解】事务层为有状态 SIP 消息处理提供基础能力，包括 INVITE 与 non-INVITE 事务的状态机、定时器与重传处理。
        status=pjsip_tsx_layer_init_module(m_endpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init tsx layer faild,code:"<<status;
            break;
        }
        //会话模块
        //【目的】注册 UA（User Agent）基础层模块。
        //【详解】UA 层主要提供 dialog/dialog-set 管理，供 INVITE session、订阅等更高层 dialog usage 使用。
         status = pjsip_ua_init_module(m_endpt,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init UA layer faild,code:"<<status;
            break;
        }

        //sip 消息的发送和接收（tcp/udp）
        //【目的】（自定义函数）为 SIP 端点启动 UDP/TCP 传输监听。
        status=init_transport_layer(sipPort);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init transport layer faild,code:"<<status;
            break;
        }

        //【目的】注册用户自定义的接收/分发模块。
        //【详解】当前模块只实现了 on_rx_request()，用于拦截并分发入站 SIP 请求；响应消息仍由其它模块或事务上下文处理。
        status=pjsip_endpt_register_module(m_endpt,&recv_mod);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"register recv module faild,code:"<<status;
            break;
        }

        //【目的】注册 100rel（可靠临时响应）扩展模块。
        //【详解】该模块为 INVITE 会话提供 RFC 3262 所需的 100rel/PRACK 支持；它会注册 PRACK 方法以及相关能力标记，本身不是“发送 PRACK 响应”。
        status=pjsip_100rel_init_module(m_endpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"100rel module init faild,code:"<<status;
            break;
        }

        pjsip_inv_callback inv_cb;
        pj_bzero(&inv_cb,sizeof(inv_cb));
        inv_cb.on_state_changed=&SipGbPlay::OnStateChanged; //请求会话状态发生变更时回调
        inv_cb.on_new_session=&SipGbPlay::OnNewSession;//创建新的 INVITE session 时回调；这两个回调是 pjsip_inv_usage_init() 要求的必填项
        inv_cb.on_media_update=&SipGbPlay::OnMediaUpdate;//处理媒体相关事务，解析sdp协议，rtp传输等（下级发送200ok并且携带sdp时会触发）
        inv_cb.on_send_ack = &SipGbPlay::OnSendAck; //可选回调：在发送 ACK 前可检查或调整本次会话相关处理
        /*
        【目的】注册 INVITE会话（INVITE session）模块并挂回调
        【详解】
        pjsip_inv_usage_init() 要求回调结构体非空，且至少提供：
        on_state_changed   – 会话状态迁移回调
        on_new_session     – 新 INVITE session 创建回调
        on_media_update、on_send_ack 等其余回调可按业务需要选择性提供。
        */
        status=pjsip_inv_usage_init(m_endpt,&inv_cb);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"register invite module faild,code:"<<status;
            break;
        }

        /*
        【目的】从 endpoint 关联的 pool factory 申请一个应用侧内存池。
        【详解】这个 pool 只是“由 endpoint 创建/归还”的 pj_pool，不天然等同于 endpoint 生命周期；当前代码把它当作长生命周期 pool，用于事件线程等对象的分配。
        【与 m_cachingPool 的区别】m_cachingPool 是 pool factory；这里拿到的是从该 factory 派生出来的一块具体 pj_pool。
        */
        m_pool=pjsip_endpt_create_pool(m_endpt,NULL,SIP_ALLOC_POOL_1M,SIP_ALLOC_POOL_1M);//分配内存池
        if(NULL==m_pool)
        {
            LOG(ERROR)<<"create pool faild";
            break;
        }

        pj_thread_t* eventThread = NULL;
        //【目的】创建 事件分发线程
        //【详解】这里创建的是 PJLIB 线程；线程入口持续调用 pjsip_endpt_handle_events()，以驱动传输层和 timer heap 上的事件处理。
        status=pj_thread_create(m_pool,NULL,&pollingEvent,m_endpt,0,0,&eventThread);//启动线程轮询处理endpt事务
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"create pjsip thread faild,code:"<<status;
            break;
        }


    }while(0);

    bool bret=true;
    if(PJ_SUCCESS!=status)
    {
        bret=false;      
    }
    return bret;
}

pj_status_t SipCore::init_transport_layer(int sipPort)
{
    pj_sockaddr_in addr;
    pj_bzero(&addr,sizeof(addr));//将 addr 结构体的内存区域全部置零，确保没有未初始化的残留数据
    addr.sin_family=pj_AF_INET();//指定地址族为 IPv4（AF_INET）
    addr.sin_addr.s_addr=0;//将 IP 地址设置为 0.0.0.0，表示监听所有本地网络接口（即允许来自任意网卡的连接）
    addr.sin_port=pj_htons((pj_uint16_t)sipPort);//pj_htons 将主机字节序转为网络字节序（大端），确保跨平台兼容性

    pj_status_t status;
    do
    {
        // 启动 UDP 监听并注册到 transport manager；NULL 的 published address 表示对外公布本地绑定地址。
        status=pjsip_udp_transport_start(m_endpt,&addr,NULL,1,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"start udp server faild,code:"<<status;
            break;
        }
        // 启动 TCP 监听工厂并注册到 transport manager；这里并未初始化 TLS。
        status=pjsip_tcp_transport_start(m_endpt,&addr,1,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"start tcp server faild,code:"<<status;
            break;
        }

        LOG(INFO)<<"sip tcp:"<<sipPort<<" udp:"<<sipPort<<" running";
    } while (0);

    return status;
    
}