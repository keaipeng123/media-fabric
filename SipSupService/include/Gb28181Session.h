#ifndef _GB28181SESSION_H
#define _GB28181SESSION_H

#include "rtpsession.h"
#include "rtpsourcedata.h"
#include "rtptcptransmitter.h"
#include "rtptcpaddress.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtcpsrpacket.h"
#include "Common.h"
#include "mpeg-ps.h"

#include "SipDef.h"

#ifdef __cplusplus  // 开头1：检查是否是C++编译器
extern "C"{         // 若是，执行这行（打开C规则包裹）
#endif              // 结束1：闭合开头1的判断（不管是不是C++，都到这一步）
#include "jthread.h"// 核心操作：引入头文件，无论 C/C++ 环境都要引入
#ifdef __cplusplus  // 开头2：再次检查是否是C++编译器
}                   // 若是，执行这行（闭合C规则包裹）
#endif              // 结束2：闭合开头2的判断
//#include "GlobalCtl.h"
using namespace jrtplib;

typedef struct _PackProcStat
{
    _PackProcStat()
    {
        rSeq = -1;
        rTimeStamp = 0;
        rState = 1;
        rlen = 10*1024;
        rNow = 0;
        rBuf = (char*)malloc(rlen);
        unpackHnd = NULL;
        psFp = NULL;
        sCodec = 0;
        sBuf = (char*)malloc(rlen);
        slen = 10*1024;;
        sNow = 0;
        sKeyFrame = 0;
		sPts = -1;
		sFp = NULL;
		session = NULL;
    }
    ~_PackProcStat()
    {
        if(rBuf)
        {
            free(rBuf);
            rBuf = NULL;
        }
        if(psFp)
        {
            fclose(psFp);
            psFp = NULL;
        }
        if(sBuf)
        {
            free(sBuf);
            sBuf = NULL;
        }
        if(sFp)
        {
            fclose(sFp);
            sFp = NULL;
        }
    }
    int rSeq;//序号
    int rTimeStamp;//时间戳
    int rState;//丢包标识 ，默认1不丢包
    int rlen;   //当前buf的总大小
    int rNow;   //当前已经收到的数据包大小
    char* rBuf;  //当前收取数据的buf ps流
    char* sBuf;  //发送数据的buf h264/265流
    int slen;  //发送buf的总长度
    int sNow; //当前发送数据的大小
    void* unpackHnd; //ps解码句柄
    FILE* psFp;
    int sCodec; //媒体流判断
    int sKeyFrame; //关键帧标识
    int64_t sPts; //pts显示时间戳
	FILE* sFp;
	void* session;//由于是静态函数调用此结构体，需要透传gbsession对象指针，以便在静态函数里调用非静态成员函数

}PackProcStat;

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
        //gettimeofday(&m_curTime,NULL);
        //m_rtpPort=0;
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
    //timeval m_curTime;//检测下级推流异常

    //int m_rtpPort;
};

class Gb28181Session : public RTPSession,public Session
{
    public:
        Gb28181Session(const DeviceInfo& devInfo);
        ~Gb28181Session();

        int CreateRtpSession();
        //int CreateRtpSession(string dstip,int dstport);
        // int RtpTcpInit(string dstip,int dstport,int time);
		int SendPacket(int media,char* data,int datalen,int codecId);//发送给前端
    protected:
        enum
        {
            RtpPack_FrameContinue = 0,//当前帧未结束
            RtpPack_FrameCurFinsh,//帧结束（边界）
            RtpPack_FrameNextStart,//下个帧开始
        };
        //回调，当接收对端rtp/rtcp数据时触发或者本端发送rtcp数据时触发
        void OnPollThreadStep();
        void ProcessRTPPacket(RTPSourceData& srcdat,RTPPacket& pack);
        void OnRTPPacketProcPs(int mark,int curSeq,int timestamp,unsigned char* payloadData,int payloadLen);//处理ps数据包
        //当有新的数据源被添加到源表时会调用该接口
        void OnNewSource(RTPSourceData *srcdat)
        {
			LOG(INFO)<<"OnNewSource";
			LOG(INFO)<<"srcdat->IsOwnSSRC():"<<srcdat->IsOwnSSRC();
            if(srcdat->IsOwnSSRC())
                return;
            
            uint32_t ip;
            uint16_t port;
            if(srcdat->GetRTPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
			{
				return;
			}
                
            
            RTPIPv4Address dest(ip,port);
            AddDestination(dest);
            struct in_addr inaddr;
            inaddr.s_addr = htonl(ip);
            LOG(INFO)<<"Adding destination "<<string(inet_ntoa(inaddr))<<":"<<port;
        }

        //当对端释放资源或者关闭链接时调用
        void OnRemoveSource(RTPSourceData *srcdat)
        {
            if(srcdat->IsOwnSSRC())
                return;

            if(srcdat->ReceivedBYE())
                return;
            
            uint32_t ip;
            uint16_t port;
            if(srcdat->GetRTPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
                return;
            
            RTPIPv4Address dest(ip,port);
            DeleteDestination(dest);
			
			struct in_addr inaddr;
			inaddr.s_addr = htonl(ip);
			LOG(INFO) << __FUNCTION__ << " Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port;
        }

        //当对端结束会话发送bye包时调用
        void OnBYEPacket(RTPSourceData *srcdat)
        {
            if(srcdat->IsOwnSSRC())
                return;
            
            uint32_t ip;
            uint16_t port;
            if(srcdat->GetRTPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort();
            }
            else if(srcdat->GetRTCPDataAddress() != 0)
            {
                const RTPIPv4Address* addr = (const RTPIPv4Address*)srcdat->GetRTCPDataAddress();
                ip = addr->GetIP();
                port = addr->GetPort()-1;
            }
            else
                return;
            
            RTPIPv4Address dest(ip,port);
            DeleteDestination(dest);
			
			struct in_addr inaddr;
			inaddr.s_addr = htonl(ip);
			
			LOG(INFO) << " Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port;
        }

    private:
        PackProcStat* m_proc;
	    int m_count;
    //     int m_rtpTcpFd;
    //     int m_listenFd;
};

#endif








