#include"Gb28181Session.h"
#include"ECSocket.h"
using namespace EC;

Gb28181Session::Gb28181Session()
{
    m_rtpRRTime=0;
    m_rtpTcpFd=-1;
    m_listenFd=-1;
}

Gb28181Session::~Gb28181Session()
{
    if(m_rtpTcpFd!=-1)
    {
        DeleteDestination(RTPTCPAddress(m_rtpTcpFd));
        close(m_rtpTcpFd);
        m_rtpTcpFd=-1;
    }
    if(m_listenFd!=-1)
    {
        close(m_listenFd);
        m_listenFd=-1;
    }
    BYEDestroy(RTPTime(0,0),0,0);
}

int Gb28181Session::CreateRtpSession(int poto,string setup,string dstip,int dstport,int rtpPort)
{
    LOG(INFO)<<"CreateRtpSession:m_dstip:"<<dstip;
    uint32_t destip=inet_addr(dstip.c_str());
    destip=ntohl(destip);
    RTPSessionParams sessionParams;
    sessionParams.SetOwnTimestampUnit(1.0/90000.0);//取决于传输的音视频数据的采样率（invite时指定了90000）
    sessionParams.SetAcceptOwnPackets(true);//设置是否接收自己发送的数据包，用于本地调试
    sessionParams.SetUsePollThread(true);//设置会话是否使用轮询线程，确保rtp会话的处理是在后台线程中进行，不干扰主线程
    sessionParams.SetNeedThreadSafety(true);//线程安全，上述两个设置使用到了jthread库的互斥锁
    sessionParams.SetMinimumRTCPTransmissionInterval(RTPTime(5,0));//设置最小的RTCP发送间隔为5s

    int ret=-1;
    if(poto==0)
    {
        //设置udp传输端口配置
        RTPUDPv4TransmissionParams transparams;
        transparams.SetPortbase(rtpPort);
        transparams.SetRTPSendBuffer(JRTP_SET_SEND_BUFFER);//设置发送缓冲区大小
        transparams.SetMulticastTTL(JRTP_SET_MULITTTL);//设置最大跳数

        ret=Create(sessionParams,&transparams);
        if(ret<0)
        {
            LOG(ERROR)<<"udp create failed";
        }
        else
        {
            RTPIPv4Address dest(destip,dstport); //上级的onNewSource会被触发
            LOG(INFO)<<"dest create:"<<"destip:"<<destip<<"dstport"<<dstport;
            ret=AddDestination(dest);
            if(ret<0)
            {
                LOG(ERROR)<<"udp create failed";
            }

            LOG(INFO)<<"udp create ok rtp port:"<<rtpPort;
        }
    }
    else
    {
        sessionParams.SetMaximumPacketSize(65535);//将分组设置为最大值，减少tcp分段头部占比，降低网络带宽消耗
        RTPTCPTransmissionParams transparams;
        ret=Create(sessionParams,&transparams,RTPTransmitter::TCPProto);
        if(ret<0)
        {
            LOG(ERROR)<<"rtp tcp error:"<<RTPGetErrorString(ret);
            return ret;
        }
        LOG(INFO)<<"protocal为1,开启tcp服务"<<"CreateRtpSession";
        int sessFd=RtpTcpInit(setup,rtpPort,dstip,dstport,5);
        if(sessFd<0)
        {
            LOG(ERROR)<<"RtpTcpInit faild";
        }
        else
        {
            AddDestination(RTPTCPAddress(sessFd));
        }
    }

    return ret;
}

int Gb28181Session::RtpTcpInit(string setup,int localport,string dstip,int dstport,int time)
{
    int timeout = time*1000;
    StatusType status = ST_UNINIT;
    LOG(INFO)<<"RtpTcpInit setup="<<setup<<" localport="<<localport<<" dstip="<<dstip<<" dstport="<<dstport<<" timeoutMs="<<timeout;
    if(setup == "active")
    {//上级是active，代表下级做服务端
        status = ECSocket::createConnByPassive(localport,&m_listenFd,&m_rtpTcpFd,&timeout);
    }
    else if(setup == "passive")
    {
        status = ECSocket::createConnByActive(localport,dstip,dstport,&m_rtpTcpFd,&timeout);
    }

    if(status != ST_OK)
    {
        LOG(ERROR)<<"RtpTcpInit status="<<status<<" setup="<<setup<<" localport="<<localport<<" dstip="<<dstip<<" dstport="<<dstport;
        return -1;
    }

    return m_rtpTcpFd;
}

// int Gb28181Session::RtpTcpinit(string setup,int localport,string dstip,int dstport,int time)
// {
//     int timeout=time*1000;
//     LOG(INFO)<<"setup:"<<setup;
//     if(setup=="active")//主动
//     {
//         m_rtpTcpFd=ECSocket::createConnByPassive(localport,&m_listenFd,&timeout);
//     }
//     else if(setup=="passive")
//     {
//         m_rtpTcpFd=ECSocket::createConnByActive(localport,dstip,dstport,&timeout);   
//     }
//     return m_rtpTcpFd;
// }

void Gb28181Session::OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
    const RTPAddress *senderaddress)
{
    RTCPPacket* rtcpPack;
    pack->GotoFirstPacket();//获取第一个包
    while((rtcpPack=pack->GetNextPacket())!=NULL)//循环获取包
    {
        if(rtcpPack->IsKnownFormat())//是否是符合rtcp协议的有效包
        {
            switch(rtcpPack->GetPacketType())
            case RTCPPacket::RR: //下级只需要检查rr包，上级则检查sr包
                {
                    //每次收到rr包，更新获取到的时间戳
                    time_t now_time=time(NULL);
                    m_rtpRRTime=now_time;
                    LOG(INFO)<<"m_rtpRRTime:"<<m_rtpRRTime;
                    break;
                }
        }
    }
}

void* SipPsCode::Alloc(void* param, size_t bytes)
{
    return malloc(bytes);
}

void SipPsCode::Free(void* param, void* packet)
{
    return free(packet);
}
int SipPsCode::onPsPacket(void* param, int stream, void* packet, size_t bytes)
{
    LOG(INFO)<<bytes<<" packet demutex";
    SipPsCode* self=(SipPsCode*)param; //静态需要指定对象指针
    self->sendPackData(packet,bytes);
    return 0;
}

int SipPsCode::initPsEncode()
{
    struct ps_muxer_func_t func;//设置回调，初始化封装器中对于数据包的内存分配，释放以及处理的回调函数
    func.alloc=Alloc;
    func.free=Free;
    func.write=onPsPacket;

    m_muxer=ps_muxer_create(&func,this);
    if(NULL==m_muxer)
    {
        LOG(ERROR)<<"ps_muxer_create failed";
        return -1;
    }

    if(gbRtpInit()<0)
    {
        LOG(ERROR)<<"rtp init error";
        return -1;
    }
    return 0;

}
int SipPsCode::gbRtpInit()
{
    m_gbRtpHandle=new Gb28181Session();
    return m_gbRtpHandle->CreateRtpSession(this->m_poto,this->m_setup,this->m_dstip,this->m_dstport,this->m_rtpPort);
   //return m_gbRtpHandle->CreateRtpSession(this->m_dstip, this->m_dstport,this->m_rtpPort);
}

int SipPsCode::incomeVideoData(unsigned char* avdata,int len,int pts,int isIframe)
{
    if(m_avStreamIndex==-1) //h264的流只初始化一次
    {
        m_avStreamIndex=ps_muxer_add_stream(m_muxer,STREAM_VIDEO_H264,NULL,0);//向m_muxer封装器添加需要封装流的信息,返回添加流的索引值
        LOG(INFO)<<"add stream index:"<<m_avStreamIndex;
    }
    if(m_gbRtpHandle->CheckAlive())
    {
        LOG(ERROR)<<"recv rtcp RR error";
        this->stopFlag=true;
        return -1;
    }
    //关键帧：ps h | ps sys h | ps sys map | pes h | h264 raw
    //非关键帧： ps h | pes h | h264 raw
    //音频：pes h |aac raw
    int64_t pts90k=(int64_t)pts*90;
    int ret=ps_muxer_input(m_muxer,m_avStreamIndex,isIframe,pts90k,pts90k,avdata,len);//isIframe是否I帧
    if(ret<0)
    {
        LOG(INFO)<<"error to push frame:"<<ret;
    }
    return ret;
}

int SipPsCode::incomeAudioData(unsigned char* audata,int len,int pts)
{
    if(m_auStreamIndex==-1) //h264的流只初始化一次
    {
        m_auStreamIndex=ps_muxer_add_stream(m_muxer,STREAM_AUDIO_AAC,NULL,0);//向m_muxer封装器添加需要封装流的信息,返回添加流的索引值
        LOG(INFO)<<"add stream index:"<<m_auStreamIndex;
    }
    if(m_gbRtpHandle->CheckAlive())
    {
        LOG(ERROR)<<"recv rtcp RR error";
        this->stopFlag=true;
        return -1;
    }
    //关键帧：ps h | ps sys h | ps sys map | pes h | h264 raw
    //非关键帧： ps h | pes h | h264 raw
    //音频：pes h |aac raw
    int64_t pts90k=(int64_t)pts*90;
    int ret=ps_muxer_input(m_muxer,m_auStreamIndex,0,pts90k,pts90k,audata,len);//isIframe是否I帧
    if(ret<0)
    {
        LOG(INFO)<<"error to push frame:"<<ret;
    }
    return ret;
}

int SipPsCode::sendPackData(void* packet, size_t bytes)
{
    int ps_buff_len=1300;//每次发送的rtp包字节，小于mtu，需要对原数据进行拆解
    int size=0;//已经发送的大小
    int status=0;
    while(size<bytes)
    {
        int packlen=(bytes-size)>=ps_buff_len?ps_buff_len:(bytes-size);
        if(bytes<ps_buff_len)//当前包小于设定的rtp包字节，直接发送，并且mark位置1（音频包）
        {
            status=m_gbRtpHandle->SendPacket(packet,bytes,96,true,PS_SEND_TIMESTAME);//mark对于视频来说代表一帧数据的结尾，对于音频来说代表数据的开始
            if(status<0)
            {
                LOG(ERROR)<<RTPGetErrorString(status);
            }
        }
        else
        {
            if((bytes-size)>ps_buff_len)//当前包大于设定的rtp包字节，说明需要拆包发送，拆包后mark位置0（视频包）
            {
                status=m_gbRtpHandle->SendPacket(packet+size,packlen,96,false,0);//mark对于视频来说代表一帧数据的结尾，对于音频来说代表数据的开始
                if(status<0)
                {
                    LOG(ERROR)<<RTPGetErrorString(status);
                }
            }
            else//最后一个包，直接发送，并且mark位置1（视频包）
            {
                //PS_SEND_TIMESTAME 每一帧使用同一个时间戳
                status=m_gbRtpHandle->SendPacket(packet+size,packlen,96,true,PS_SEND_TIMESTAME);//mark对于视频来说代表一帧数据的结尾，对于音频来说代表数据的开始
                if(status<0)
                {
                    LOG(ERROR)<<RTPGetErrorString(status);
                }
            }
        }
        size+=packlen;
    }
    return status;

}