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
//#include "mpeg-ps.h"

//#include "SipDef.h"

#ifdef __cplusplus
extern "C"{
#endif
#include "jthread.h"
#ifdef __cplusplus
}
#endif
//#include "GlobalCtl.h"
using namespace jrtplib;

// typedef struct _PackProcStat
// {
//     _PackProcStat()
//     {
//         rSeq = -1;
//         rTimeStamp = 0;
//         rState = 1;
//         rlen = 10*1024;
//         rNow = 0;
//         rBuf = (char*)malloc(rlen);
//         unpackHnd = NULL;
//         psFp = NULL;
//         sCodec = 0;
//         sBuf = (char*)malloc(rlen);
//         slen = 10*1024;;
//         sNow = 0;
//         sKeyFrame = 0;
// 		sPts = 0;
// 		sFp = NULL;
// 		session = NULL;
//     }
//     ~_PackProcStat()
//     {
//         if(rBuf)
//         {
//             free(rBuf);
//             rBuf = NULL;
//         }
//         if(psFp)
//         {
//             fclose(psFp);
//             psFp = NULL;
//         }
//     }
//     int rSeq;
//     int rTimeStamp;
//     int rState;
//     int rlen;   //当前buf的总大小
//     int rNow;   //当前已经收到的数据包大小
//     char* rBuf;  //当前收取数据的buf
//     char* sBuf;
//     int slen;
//     int sNow;
//     void* unpackHnd;
//     FILE* psFp;
//     int sCodec;
//     int sKeyFrame;
// 	int sPts;
// 	FILE* sFp;
// 	void* session;

// }PackProcStat;

//class Gb28181Session : public RTPSession,public Session
class Gb28181Session : public RTPSession
{
    public:
        //Gb28181Session(const DeviceInfo& devInfo);
        Gb28181Session();
        ~Gb28181Session();

        // int CreateRtpSession(string dstip,int dstport);
        // int RtpTcpInit(string dstip,int dstport,int time);
		// int SendPacket(int media,char* data,int datalen,int codecId);
    protected:
        // enum
        // {
        //     RtpPack_FrameContinue = 0,
        //     RtpPack_FrameCurFinsh,
        //     RtpPack_FrameNextStart,
        // };
        // void OnPollThreadStep();
        // void ProcessRTPPacket(RTPSourceData& srcdat,RTPPacket& pack);
        // void OnRTPPacketProcPs(int mark,int curSeq,int timestamp,unsigned char* payloadData,int payloadLen);
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

    // private:
    //     PackProcStat* m_proc;
	// 	int m_count;
    //     int m_rtpTcpFd;
    //     int m_listenFd;
};

#endif