#ifndef _SIPGBPLAY_H
#define _SIPGBPLAY_H
#include "SipTaskBase.h"
#include "Gb28181Session.h"
#include <unordered_map>
class SipGbPlay : public SipTaskBase
{
    public:
        typedef struct _MediaInfo
        {
            string sessionName;
            string sdp_protol = "RTP/AVP";
            string dstRtpAddr;
            int dstRtpPort;
            string setUp = "";
            int startTime = 0;
            int endTime = 0;
			int localRtpPort = 0;
        }MediaInfo;

        //我们就在本类中来定义个字典的数据结构
        //这里呢我使用unordered_map，使用map也可以
        //我这里简单的和大家说下这两者的区别和使用的时机
        /*
        1.底层实现的数据结构不一样，unordered_map底层用的是哈希表，map用的是红黑树
        2.性能不同，unordered_map是不安键值来排序的，插入的时间复杂度为O(logn),也就是当数据增大n倍时，耗时增大logn倍，
        这里的log是以2为底的，比如，当数据增大256倍时，耗时只增大8倍，查询的时间为O(1),这是最低的时间和空间复杂度了，
        也就是耗时与输入数据的大小无关，map呢是需要按照键值排序的，插入的时间复杂度为O(logn)，查询的时间复杂度也为O(logn)，
        所以map在查询方面没有unordered_map的性能优秀，
        3.使用的范围不同，unordered_map的key只能是int、double、string等基本类型，并且不能使用自定义的结构体，范围比较局限，
        而map的key呢是支持所有的数据类型，
        最后总结下，不需要按键值排序并且key的数据类型单一的那么就用unordered_map，否则就用map
        */
        typedef unordered_map<string, SipPsCode*> MediaStreamInfo;
        static MediaStreamInfo  mediaInfoMap;
        static pthread_mutex_t streamLock;
        
        SipGbPlay();
        ~SipGbPlay();
        virtual void run(pjsip_rx_data *rdata);
        void dealWithInvite(pjsip_rx_data *rdata);
        void resWithSdp(pjsip_rx_data *rdata,int status_code,string devid,MediaInfo sdpInfo);
        void dealWithBye(pjsip_rx_data *rdata);

        static void OnStateChanged(pjsip_inv_session *inv, pjsip_event *e);
        static void OnNewSession(pjsip_inv_session *inv, pjsip_event *e);
        static void OnMediaUpdate(pjsip_inv_session *inv_ses, pj_status_t status);

        void sendPsRtpStream(SipPsCode** ps);
        int recvFrame(SipPsCode** ps);

};
#endif