#include "Gb28181Session.h"
#include "SipDef.h"
// #include "mpeg4-avc.h"
// #include "h264_stream.hpp"
// #include "h265_stream.hpp"
// #include "mpeg4-hevc.h"  
// #include "ECSocket.h"
// using namespace EC;


// //定义个结构体
// typedef struct 
// {
//     uint16_t width;
//     uint16_t height;
//     float max_framerate;
// } Picinfo;
// //这里定义个接口来实现从h264的编码中获取分辨率和帧率
// int GetH264pic(const uint8_t* data,uint16_t size,Picinfo* info)
// {
//     if (data == NULL || size == 0) {
//         return 1;
//     }
//     if (info == NULL) {
//         return 1;
//     }
//     memset(info, 0, sizeof(Picinfo));

//     int ret = 999;
//     char s_buffer[4 * 1024];
//     memset(s_buffer, 0, sizeof(s_buffer));
//     struct mpeg4_avc_t avc;
//     memset(&avc, 0, sizeof(mpeg4_avc_t));
//     struct mpeg4_hevc_t hevc;
//     memset(&hevc, 0, sizeof(mpeg4_hevc_t));

// 	ret =(int)mpeg4_annexbtomp4(&avc, data, size, s_buffer, sizeof(s_buffer));
// 	if (avc.nb_sps <= 0) {
// 		return 1;
// 	}

// 	h264_stream_t* h4 = h264_new();
// 	ret =h264_configure_parse(h4, avc.sps[0].data, avc.sps[0].bytes, H264_SPS);
// 	if (ret != 0) {
// 		h264_free(h4);
// 		return 1;
// 	}
// 	info->width = h4->info->width;
// 	info->height = h4->info->height;
// 	info->max_framerate = h4->info->max_framerate;
// 	h264_free(h4);
	
// 	return 0;
// }

//接收ps解封装之后的裸流数据
//codecid音频/视频编解码器，需要用codecid来判别裸流是音频还是视频数据
//stream 流的编号
//flags 视频关键帧的标识
//data ps解封装后的流，data不一定是完整的一帧视频数据或者音频通道数据
static void ps_demux_callback(void* param, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    
	PackProcStat* pProc = (PackProcStat*)param;
    //然后需要在结构体里再添加几个成员

    //数据流类型(音/视频)
    int media = -1;
    //视频帧类型(I/P帧)
    int frameType = 1;
    
    //我们先判别下当前的codecid是否有效
	if(codecid == 0) // unknown codec id
	{
		return;
	}
	
    //首先先处理同一类型的同一帧流数据被拆成多个包的情况
    //当前回调被第一次调用时，不会走这个逻辑
	if(pProc->sCodec == codecid && pProc->sPts == pts)
	{
        //我们需要将当前的裸流保存到buffer，后续我们再处理buffer的数据
		if(pProc->slen < pProc->sNow + (int)bytes)
		{
			pProc->slen = pProc->sNow + (int)bytes + 1024;
			pProc->sBuf = (char *)realloc(pProc->sBuf, pProc->slen);
		}
		memcpy(pProc->sBuf + pProc->sNow, data, bytes);
		pProc->sNow += bytes;
		pProc->sKeyFrame = flags;
		return;
	}

    //当前这个接口第一次调用时，会走这个逻辑
    //后续这个逻辑是来处理当前传入的data为下一帧数据或者不同类型的数据，
	else 
	{
		//如果发送buffer的数据大于0，那么我们就进行发送，发送前还需要进行header的设定，一会我们再做
		if(pProc->sNow > 0)
		{
			//do{
				if(pProc->sCodec == STREAM_VIDEO_H264
					|| pProc->sCodec == STREAM_VIDEO_H265)
				{ 
                    //在这里可以先区分媒体流的类型，和当前视频帧是否为关键帧
					media = 2;
					//frameType = pProc->sKeyFrame > 1 ? FORMAT_VIDEO_I : FORMAT_VIDEO_P;
                    #if 1
					if(pProc->sFp == NULL)
					{
                        pProc->sFp = fopen("../conf/send.h264", "wb");
					}

					if(pProc->sFp != NULL)
					{
                        // 防止长时间运行无限落盘导致文件异常变大（调试用途）
                        const long cur = ftell(pProc->sFp);//返回当前文件读写位置
                        const long kMaxDumpBytes = 64L * 1024L * 1024L; // 64MB
                        if(cur >= 0 && cur >= kMaxDumpBytes)
                        {
                            LOG(WARNING) << "send.h264 dump reached limit (" << cur << "), stop dumping";
                            fclose(pProc->sFp);
                            pProc->sFp = NULL;
                        }
                        else
                        {
                            fwrite(pProc->sBuf, 1, pProc->sNow, pProc->sFp);//把缓冲区 sBuf 中当前累计的 sNow 字节数据，写入到文件 sFp 对应的文件里
                        }
					}
                    #endif
				}
				else if(pProc->sCodec == STREAM_AUDIO_AAC
						|| pProc->sCodec == STREAM_AUDIO_G711A
						|| pProc->sCodec == STREAM_AUDIO_G711U)
				{
					media = 1;
				}
				//else
				//{
				// 	if(pProc->unknownCodecCnt == 0)
				// 		LOG(INFO) << " unknown codec: " << pProc->sCodec;

				// 	if(pProc->unknownCodecCnt ++ > 200)
				// 		pProc->unknownCodecCnt = 0; 

					//break;
				//}

				//unsigned long long microsecIn = pProc->sDts * 1000 / 90000;
                //Gb28181Session* pGbSesson = (Gb28181Session*)pProc->session;
				//int sendLen = pGbSesson->SendPacket(media, (char *)pProc->sBuf, pProc->sNow, pProc->sCodec);
			//}while(0);
		}
		//LOG(INFO)<<"33333333";
		// copy new data to send buffer
        //代码首先走这里
        memset(pProc->sBuf, 0, pProc->sNow);
		pProc->sNow = 0;
		pProc->sKeyFrame = 0;
        //将当前数据copy到buffer中
		if(pProc->slen < pProc->sNow + (int)bytes)
		{
			pProc->slen = pProc->sNow + bytes + 1024;
			pProc->sBuf = (char *)realloc(pProc->sBuf, pProc->slen);
		}
		memcpy(pProc->sBuf + pProc->sNow, data, bytes);
		pProc->sNow += bytes;
		pProc->sKeyFrame = flags;

		pProc->sPts = pts;
		
		pProc->sCodec = codecid;
	}

	return;
}

// //需要使用codecId来区分码流的编码方式，然后来进行相应的解码获取分别率以及帧率
// int Gb28181Session::SendPacket(int media,char* data,int datalen,int codecId)
// {
//     //先计算下推送包的整个长度，header部分+负载部分
//     int len = sizeof(struct StreamHeader) + datalen;

//     char* streamBuf = new char[len];
//     memset(streamBuf,0,len);

//     //先给header部分的参数进行赋值
//     StreamHeader* header = (StreamHeader*)streamBuf;
//     header->length = datalen;
//     if(media == 2)
//     {
//         header->type = media;
//         //需要进行IDR帧的判断，并从SPS序列集中解出帧率和分别率
//         //判断下当前流的codeid是否属于h264编码
//         if(codecId == STREAM_VIDEO_H264)
//         {
//             int nalType = 0,keyFrame = 0;
// 			int videoW = 0, videoH = 0;
// 			int videoFps = 25;
//             //先保存下当前流的编码格式，将int类型转为char，4-》1，其实就是存储了ascll码的值
//             //header->format[0] = 1;
//             //然后再获取nalu类型，我们之前说过nalu的startcode为三个字节或者四个字节
//             if(data[0] == 0x0 && data[1] == 0x0 && data[2] == 0x0 && data[3] == 0x01)
// 			{
//                 nalType = data[4] & 0x1F;
// 			}
//             else if(data[0] == 0x0 && data[1] == 0x0 && data[2] == 0x01)
//             {
//                 nalType = data[3] & 0x1F;
//             }
// 			else
// 			{
// 				LOG(ERROR) << "Invalid h264 data, please check!";
// 				return -1;
// 			}

//             //如果说nalType为7，那么就代表这帧数据包含了SPS序列集，也就是IDR帧
//             //那么定义个变量标识下当前帧为关键帧
//             if(nalType == 7)
//             {
//                 keyFrame = 1;
//             }
//             else //这里只需要将包含了SPS参数集的数据看作为关键帧，其他都为非关键帧
//             {
//                 keyFrame = 0;
//             }

//             if(keyFrame == 1)
//             {
// 				//这里查询帧率和分辨率信息的频率不要太频繁，这里我们设置每50个关键帧查询一次
// 				if(m_count == 0 || m_count >50)
// 				{
// 					Picinfo info;
// 					if(GetH264pic((unsigned char *)data, datalen, &info) == 0)
// 					{
// 						if(info.height != 0 && info.width != 0)
// 						{
// 							header->videoH = info.height;
// 							header->videoW = info.width;
							
// 							LOG(INFO)<<"header->videoH:"<<header->videoH<<",header->videoW:"<<header->videoW;
// 							m_count++;
// 						}
// 						if(info.max_framerate > 0)
// 						{
// 							//header->format[0] = info.max_framerate;
// 							LOG(INFO)<<"info.max_framerate:"<<info.max_framerate;
// 						}
// 						else if(info.max_framerate == 0)
// 						{
// 							//header->format[0] = 25;
// 						}
// 						if(m_count >50)
// 						{
// 							m_count = 0;
// 						}
// 					}
// 					else
// 					{
// 						LOG(ERROR) << "can't analysis video data !! data: " << (void *)data << ", dataLen: " << datalen;
// 					}
// 				}
// 				else 
// 				{
// 					m_count++;
// 				}
				
// 				//最后再保存下帧类型
// 				//header->format[1] = keyFrame;  
// 				//将编码器类型也保存下
// 				//header->format[2] = codecId;   
//             }
			

//         }
//     }
    
//     return 0;
// }

// Gb28181Session::Gb28181Session(const DeviceInfo& devInfo)
// :Session(devInfo)
// {
//     m_proc = new PackProcStat();
// 	m_count = 0;
//     m_rtpTcpFd = -1;
//     m_listenFd = -1;
// }

Gb28181Session::Gb28181Session()
{
     m_proc = new PackProcStat();
}


Gb28181Session::~Gb28181Session()
{
	//发送BYE数据包并离开会话 不用等待
	// BYEDestroy(RTPTime(0, 0), 0, 0);
    // if(m_rtpTcpFd != -1)
    // {
    //     close(m_rtpTcpFd);
    //     m_rtpTcpFd = -1;
    // }

    // if(m_listenFd != -1)
    // {
    //     close(m_listenFd);
    //     m_listenFd = -1;
    // }
	
// 	GBOJ(gConfig)->pushOneRandNum(rtp_loaclport);
}

void Gb28181Session::OnPollThreadStep()
{
    //开始准备访问接听到的数据
    BeginDataAccess();

    if(GotoFirstSourceWithData())//检查是否有传入的数据包，在接收到的数据包列表中查找到第一个数据包，并将当前数据源设置为包含该数据包的数据源
    {
        do
        {
            RTPSourceData* srcdat = NULL;
            RTPPacket* pack = NULL;
            srcdat = GetCurrentSourceInfo();//获取当前源的数据包信息
            while((pack = GetNextPacket()) != NULL)//循环获取当前源的下一个数据包
            {
                //进行完整帧数据的组包
                ProcessRTPPacket(*srcdat,*pack);

                //删除数据包释放资源
                DeletePacket(pack);
            }

        } while (GotoNextSourceWithData());  //循环遍历每个数据源，并处理每个数据源中的数据包，直到没有更多的数据源包含数据包为止
    }
    //和BeginDataAccess相互对应
    EndDataAccess();

}

void Gb28181Session::ProcessRTPPacket(RTPSourceData& srcdat,RTPPacket& pack)
{
    int payloadType = pack.GetPayloadType();//获取负载类型
    int payloadLen = pack.GetPayloadLength();//获取负载长度
    int mark = pack.HasMarker();//获取标志位
    int curSeq = pack.GetExtendedSequenceNumber();//获取扩展序列号
    int timestamp = pack.GetTimestamp();//获取时间戳
    unsigned char* payloadData = pack.GetPayloadData();//获取负载数据

    if(payloadType != 96  && payloadType != 98)
    {
        LOG(ERROR)<<"rtp unknown payload type:"<<payloadType;
        return;
    }
    // //在这里更新下下级最后有rtp包推送的时间
	// gettimeofday(&m_curTime, NULL);
	// //那么就在接收rtp包的时机给这个session进行赋值
    // //这里需要先判断下
    // if(m_proc && m_proc->session == NULL)
    // {
    //     m_proc->session = (void*)this;
    // }


    if(payloadType == 96)//ps
    {
        OnRTPPacketProcPs(mark,curSeq,timestamp,payloadData,payloadLen);
    }
    else if(payloadType == 98)//h264
    {

    }
}

void Gb28181Session::OnRTPPacketProcPs(int mark,int curSeq,int timestamp,unsigned char* payloadData,int payloadLen)
{
 	LOG(INFO)<<"mark:"<<mark;
    int FrameStat = mark;

    if(m_proc->rSeq == -1)
        m_proc->rSeq = curSeq;
    
    if(m_proc->rTimeStamp == 0)
        m_proc->rTimeStamp = timestamp;

    if(curSeq - m_proc->rSeq > 1)//丢包
    {
        m_proc->rState = 2;
        LOG(ERROR)<<"rtp drop pack diff:"<<curSeq - m_proc->rSeq;
    }

    if(FrameStat == 0)
    {
		//LOG(INFO)<<"m_proc->rTimeStamp:"<<m_proc->rTimeStamp"<< timestamp:"<<timestamp;
		
        if(timestamp != m_proc->rTimeStamp)//当前时间戳不等于上一个，意味着下一个数据帧的开始
        {
            FrameStat = RtpPack_FrameNextStart;
        }
    }

    m_proc->rSeq = curSeq;
    m_proc->rTimeStamp = timestamp;

    if(m_proc->rState == 1)//不丢包
    {
        if(FrameStat == RtpPack_FrameContinue || FrameStat == RtpPack_FrameCurFinsh)//如果当前数据包不是下一帧的开始，那么就继续将数据包的数据保存到buffer中
        {
            if(m_proc->rlen < payloadLen + m_proc->rNow)//需要扩容
            {
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;//1024避免每来一个稍大的包都立刻重新分配内存，属于一种简单的“预留空间”策略
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);//从 payloadData 取出 payloadLen 字节，复制到 rBuf 的偏移 rNow 位置
            m_proc->rNow += payloadLen;
        }
    }
    else if(m_proc->rState == 2)//丢包
    {
        // 已发生丢包：当前帧内容不再可信，丢弃已缓存的残帧，并等待下一帧起始包再恢复组帧。
        if(m_proc->rNow > 0)
        {
            memset(m_proc->rBuf, 0, m_proc->rNow);
            m_proc->rNow = 0;
        }

        if(FrameStat == RtpPack_FrameNextStart)//新帧
        {
            if(m_proc->rlen < payloadLen + m_proc->rNow)
            {
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);
            m_proc->rNow += payloadLen;
            // 找到新的帧起始点后才回到正常状态
            m_proc->rState = 1;
        }

        // 其它情况(Continue/CurFinsh)：忽略，继续保持丢包状态等待NextStart
        return;
        
    }
	LOG(INFO)<<"FrameStat:"<<FrameStat;
    //如果改成 == 1，就会漏掉值为 2 的 NextStart。这样当代码通过时间戳判断出“新帧开始”时，就不会先处理上一帧已经缓存好的数据，逻辑上是有风险的
    if(FrameStat)//帧边界
    {
        #if 0
        if(m_proc->psFp == NULL)
        {
            m_proc->psFp = fopen("../conf/data.ps","w+");
        }
        if(m_proc->psFp)
        {
            fwrite(m_proc->rBuf,1,m_proc->rNow,m_proc->psFp);
        }
        #endif
        //ps demutex
        if(m_proc->unpackHnd == NULL)
		{
			LOG(INFO)<<"ps_demuxer_create"; 
            //这里需要定义一个回调接口，用来接收处理的解封装后的音视频数据
			m_proc->unpackHnd = (void *)ps_demuxer_create((ps_demuxer_onpacket)ps_demux_callback, (void *)m_proc);
		}

		if(m_proc->unpackHnd)
		{
			LOG(INFO)<<"PS SIZE:"<<m_proc->rNow;
            if(m_proc->rBuf == NULL || m_proc->rNow <= 0)
            {
                LOG(ERROR) << "PS buffer empty/null, skip demux";
            }
            else
            {
                size_t offset = 0;
                const size_t total = (size_t)m_proc->rNow;
                int last_ret = 0;
                while(offset < total)
                {
                    const size_t remain = total - offset;
                    // 这里需将缓存的数据传入到ps解封装接口中
                    // m_proc->rBuf + offset，不是在做数值相加，而是在做指针偏移：把起始地址往后挪 offset 个字节，从“还没处理的那一段”开始继续喂给 ps_demuxer_input
                    //1. 第一次 offset = 0，从缓冲区开头开始解析。
                    //2.如果 ps_demuxer_input 返回 ret = 120，通常表示这次消费了 120 字节。
                    //3.然后 offset += 120。
                    //4.下一轮再传 rBuf + 120，也就是从第 121 个字节开始继续处理。
                    int ret = ps_demuxer_input(
                        (struct ps_demuxer_t *)m_proc->unpackHnd,
                        (const uint8_t*)m_proc->rBuf + offset,
                        remain);
                    last_ret = ret;
                    // ret 语义通常为“消费的字节数”，<=0 表示错误或无法继续
                    if(ret <= 0)
                    {
                        LOG(ERROR) << "ps_demuxer_input failed/blocked, ret=" << ret << ", remain=" << remain;
                        break;
                    }
                    if((size_t)ret > remain)
                    {
                        LOG(ERROR) << "ps_demuxer_input over-consumed, ret=" << ret << ", remain=" << remain;
                        break;
                    }
                    offset += (size_t)ret;
                }

                // 针对上面while循环break的情况，说明当前缓存的数据还没有被完全消费掉，可能是因为数据不完整（比如最后一个包不完整），也可能是因为数据损坏导致解封装失败。无论哪种情况，我们都需要保留剩余未消费的数据，并在下一次接收新数据时将它们合并后再继续解析。
                // 按 mpeg-ps.h 约定：未消费的数据需要保留并与下一次输入合并
                if(offset < total)
                {
                    const size_t left = total - offset;

                    // ret < 0: 解析错误，通常是 PS 流不同步/数据损坏。重置 demuxer 并丢弃缓存。
                    // ret == 0: 数据不足(包尾不完整)，保留 left 字节等待下次合并。
                    if(last_ret < 0)
                    {
                        LOG(ERROR) << "ps demux error, reset demuxer and drop buffer, last_ret=" << last_ret;
                        ps_demuxer_destroy((struct ps_demuxer_t*)m_proc->unpackHnd);
                        m_proc->unpackHnd = NULL;
                        m_proc->rNow = 0;//表示逻辑上缓冲区为空，虽然物理内存里可能还残留着之前的数据，但我们不再认为它们是有效数据了，下一次新数据来的时候会覆盖掉这些残留数据，所以不需要单独清理
                    }
                    else
                    {
                        memmove(m_proc->rBuf, m_proc->rBuf + offset, left);
                        m_proc->rNow = (int)left;
                    }
                }
                else
                {
                    m_proc->rNow = 0;
                }
            }
			
		}

        if(FrameStat == RtpPack_FrameNextStart)
        {
            if(m_proc->rlen < payloadLen + m_proc->rNow)
            {
                m_proc->rlen = payloadLen + m_proc->rNow + 1024;
                m_proc->rBuf = (char*)realloc(m_proc->rBuf,m_proc->rlen);
            }
            memcpy(m_proc->rBuf + m_proc->rNow,payloadData,payloadLen);
            m_proc->rNow += payloadLen;
        }

    }

    return;

}

//int Gb28181Session::CreateRtpSession(string dstip,int dstport)
int Gb28181Session::CreateRtpSession()
{
	LOG(INFO)<<"CreateRtpSession";
    RTPSessionParams sessParams; //rtp会话参数设置
    sessParams.SetOwnTimestampUnit(1.0/90000.0);//设置会话时间戳，1个rtp时间戳单位，RTP 头里 timestamp 字段每增加 1，表示媒体时间前进了多少
    sessParams.SetAcceptOwnPackets(true);//是否接收自己的数据包
    sessParams.SetUsePollThread(true);//是否使用轮询线程
    sessParams.SetNeedThreadSafety(true);//是否需要线程安全
    sessParams.SetMinimumRTCPTransmissionInterval(RTPTime(5,0));//设置最小RTCP传输间隔，5s
	int ret = -1;
    // if(protocal == 0)
    // {
        RTPUDPv4TransmissionParams transparams;
        transparams.SetRTPReceiveBuffer(1024*1024);//这里设置过小会导致业务层来不及处理接收到的rtp导致业务侧的丢包
        transparams.SetPortbase(20000);
        //transparams.SetPortbase(rtp_loaclport);
        

        ret = Create(sessParams,&transparams);
		LOG(INFO)<<"creat rtp ret:"<<ret;
        if(ret < 0)
        {
            LOG(ERROR)<<"udp create fail";
        }
        else
        {
            LOG(INFO)<<"udp create ok,bind:"<<20000;
            //LOG(INFO)<<"udp create ok,bind:"<<rtp_loaclport;
        }
    //}
    // else
    // {
    //     sessParams.SetMaximumPacketSize(65535);
    //     RTPTCPTransmissionParams transParams;
    //     ret = Create(sessParams, &transParams, RTPTransmitter::TCPProto);
    //     if(ret < 0)
    //     {
    //         LOG(ERROR) << "Rtp tcp error: " << RTPGetErrorString(ret);
    //         return -1;
    //     }

    //     //会话创建成功后，接下来需要创建tcp连接
    //     int sessFd = RtpTcpInit(dstip,dstport,5);
	// 	LOG(INFO)<<"sessFd:"<<sessFd;
    //     if(sessFd < 0)
    //     {
    //         LOG(ERROR)<<"RtpTcpInit faild";
    //         return -1;
    //     }
    //     else
    //     {
    //         AddDestination(RTPTCPAddress(sessFd));
    //     }
    // }
    

    return ret;
}

// int Gb28181Session::RtpTcpInit(string dstip,int dstport,int time)
// {
//     int timeout = time*1000;
//     if(setupType == "active")
//     {
//         m_rtpTcpFd = ECSocket::createConnByActive(dstip,dstport,rtp_loaclport,&timeout);
//     }
//     else if(setupType == "passive")
//     {
//         m_rtpTcpFd = ECSocket::createConnByPassive(&m_listenFd,rtp_loaclport,&timeout);
//     }

//     return m_rtpTcpFd;
// }