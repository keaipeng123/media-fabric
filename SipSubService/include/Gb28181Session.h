#ifndef _GB28181SESSION_H
#define _GB28181SESSION_H
#include"rtpsession.h"
#include"rtpsourcedata.h"
#include"rtptcptransmitter.h"
#include"rtptcpaddress.h"
#include"rtpudpv4transmitter.h"
#include"rtpipv4address.h"
#include"rtpsessionparams.h"
#include"rtperrors.h"
#include"rtplibraryversion.h"
#include"rtcpsrpacket.h"

#include"mpeg-ps.h"

#include"Common.h"
#include"GlobalCtl.h"

#ifdef __cplusplus
extern "C"{
#endif 
#include "jthread.h"
#ifdef __cplusplus 
}
#endif

#define JRTP_SET_SEND_BUFFER  ((30)*1024)
#define JRTP_SET_MULITTTL  (255)
#define PS_SEND_TIMESTAME (3600)

using namespace jrtplib;

class Gb28181Session : public RTPSession
{
    public:
    Gb28181Session();
    ~Gb28181Session();

    // int CheckAlive()//检查rtcp包
    // {
    //     time_t cm_time=time(NULL);
    //     if(m_rtpRRTime==0)
    //     {
    //         m_rtpRRTime=cm_time;
    //     }
    //     if((cm_time-m_rtpRRTime)>10)
    //     {
    //         return 1;//10s没有收到rr，认定为无效的rtcp
    //     }
    //     return 0;
    // } 
    int CreateRtpSession(string dstip,int dstport);
    //int CreateRtpSession(int poto,string setup,string dstip,int dstport,int rtpPort);
    //int RtpTcpinit(string setup,int localport,string dstip,int dstport,int time);

    protected:

    //检测到对方发送rtcp包时回调
    // void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
    //     const RTPAddress *senderaddress);

    //新的数据源被添加到源表中
    void OnNewSource(RTPSourceData *srcdat)
    {
        if(srcdat->IsOwnSSRC())//是否已经在本地源表中
        {
            LOG(INFO)<<"ssrc:"<<srcdat->GetSSRC();
            return;
        }
        //进行目的端口信息的获取
        uint32_t ip;
        uint16_t port;
        if(srcdat->GetRTPDataAddress()!=0)//不为0则是RTP
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort();
        }
        else if(srcdat->GetRTCPDataAddress()!=0)
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort()-1;
        }
        else
            return;

        RTPIPv4Address dest(ip,port);
        AddDestination(dest);

        struct in_addr inaddr;
        inaddr.s_addr=htonl(ip);
        LOG(INFO)<<"Adding destination"<<string(inet_ntoa(inaddr))<<":"<<port;
         
    }
    //对方释放资源/关闭连接
    void OnRemoveSource(RTPSourceData *srcdat)
    {
        //如果rtp采用tcp则在tcp连接释放时会调用这个回调
        if(srcdat->IsOwnSSRC())//是否已经在本地源表中
            return;


         if (srcdat->ReceivedBYE())
            return;    
        //进行目的端口信息的获取
        uint32_t ip;
        uint16_t port;
        if(srcdat->GetRTPDataAddress()!=0)//不为0则是RTP
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort();
        }
        else if(srcdat->GetRTCPDataAddress()!=0)
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort()-1;
        }
        else
            return;

        RTPIPv4Address dest(ip,port);
        DeleteDestination(dest);
    }
    //对端结束会话发送rtp bye包
    void OnBYEPacket(RTPSourceData *srcdat)
    {
        //如果rtp采用tcp则在tcp连接释放时会调用这个回调
        if(srcdat->IsOwnSSRC())//是否已经在本地源表中
            return;
        //进行目的端口信息的获取
        uint32_t ip;
        uint16_t port;
        if(srcdat->GetRTPDataAddress()!=0)//不为0则是RTP
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort();
        }
        else if(srcdat->GetRTCPDataAddress()!=0)
        {
            const RTPIPv4Address* addr=(const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
            ip=addr->GetIP();
            port=addr->GetPort()-1;
        }
        else
            return;

        RTPIPv4Address dest(ip,port);
        DeleteDestination(dest);

        struct in_addr inaddr;
        inaddr.s_addr=htonl(ip);
        LOG(INFO)<<__FUNCTION__<<"Deleting destination"<<string(inet_ntoa(inaddr))<<":"<<port;       
    }

    // private:
    //     int m_rtpRRTime;
    //     int m_rtpTcpFd;
    //     int m_listenFd;
};

class SipPsCode
{
    public:
    //SipPsCode(int poto,string setup,string dstip,int dstport,int rtpPort,int s,int e)
    SipPsCode(string dstip,int dstport)
    {
        m_dstip=dstip;
        m_dstport=dstport;
        // m_avStreamIndex=-1;
        // m_auStreamIndex=-1;
        // stopFlag=false;
        // m_rtpPort=rtpPort;
        // m_sTime=s;
        // m_eTime=e;
        // m_poto=poto;
        // m_setup=setup;
    }

    ~SipPsCode()
    {
        // stopFlag=false;
        // if(m_muxer)
        // {
        //     ps_muxer_destroy(m_muxer);
        // }
        // if(m_gbRtpHandle)
        // {
        //     delete m_gbRtpHandle;
        //     m_gbRtpHandle=NULL;
        // }
        // GBOJ(gConfig)->pushOneRandNum(m_rtpPort);
    }

    int initPsEncode(); //初始化ps封装器
    int gbRtpInit(); //创建rtp会话

    static void* Alloc(void* param, size_t bytes);
    static void Free(void* param, void* packet);
    static int onPsPacket(void* param, int stream, void* packet, size_t bytes);

//     int incomeVideoData(unsigned char* avdata,int len,int pts,int isIframe);//ps流封装
//     int incomeAudioData(unsigned char* audata,int len,int pts);
//     int sendPackData(void* packet, size_t bytes);
//     bool stopFlag;
//     int m_sTime;
//     int m_eTime;
//     int m_poto;
//     string m_setup;

    private:
    string m_dstip;
    int m_dstport;
    Gb28181Session* m_gbRtpHandle;
    ps_muxer_t* m_muxer;
//     int m_avStreamIndex;
//     int m_auStreamIndex;
//     int m_rtpPort;
};

#endif