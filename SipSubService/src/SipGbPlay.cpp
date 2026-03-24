#include "SipGbPlay.h"
#include "GlobalCtl.h"
#include "Common.h"
#include "SipDef.h"
#include "Gb28181Session.h"
#include "SipMessage.h"


//SipGbPlay::MediaStreamInfo SipGbPlay::mediaInfoMap;
//pthread_mutex_t SipGbPlay::streamLock = PTHREAD_MUTEX_INITIALIZER;

SipGbPlay::SipGbPlay()
{

}

SipGbPlay::~SipGbPlay()
{

}

void SipGbPlay::run(pjsip_rx_data *rdata)
{
    pjsip_msg* msg = rdata->msg_info.msg;
    if(msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
        dealWithInvite(rdata);
    }
    else if(msg->line.req.method.id == PJSIP_BYE_METHOD)
    {
        //这里我们来实现下级处理上级的BYE请求
        dealWithBye(rdata);
    }
}

void SipGbPlay::dealWithBye(pjsip_rx_data *rdata)
{
	// int code = SIP_SUCCESS;

	// std::string devId = parseToId(rdata->msg_info.msg);
	// LOG(INFO)<<"======BYE  devId:"<<devId;
	// do
	// {
	// 	if(devId == "")
	// 	{
	// 		code = SIP_BADREQUEST;
	// 		break;
	// 	}
	// 	AutoMutexLock lck(&streamLock);
	// 	auto iter = mediaInfoMap.find(devId);
	// 	if(iter != mediaInfoMap.end())
	// 	{
    //         //我们先判断下SipPsCode句柄是否不为空
	// 		if(iter->second != NULL)
	// 		{
	// 			iter->second->stopFlag = true;
	// 		}
    //         //delete iter->second;
    //         //最后还需要从map中删除当前的键值对
	// 		iter = mediaInfoMap.erase(iter);
	// 	}
	// 	else
	// 	{
	// 		code = SIP_FORBIDDEN;
	// 	}
	// }while(false);
	
	// pj_status_t status = pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(), NULL, rdata, code, NULL,NULL, NULL, NULL);
	// if (PJ_SUCCESS != status)
	// {
	// 	LOG(ERROR)<<"create response failed";
	// 	return;
	// }
	
}

void SipGbPlay::dealWithInvite(pjsip_rx_data *rdata)
{
    string fromId = parseFromId(rdata->msg_info.msg);
    bool flag = false;
    int status_code = 200;
    string id;
    MediaInfo sdpInfo;
    SipPsCode* ps = NULL;
    do
    {
       {
            AutoMutexLock lock(&(GlobalCtl::globalLock));
            GlobalCtl::SUPDOMAININFOLIST::iterator iter = GlobalCtl::instance()->getSupDomainInfoList().begin();
            for(;iter != GlobalCtl::instance()->getSupDomainInfoList().end(); iter++)
            {
                if(iter->sipId == fromId && iter->registered)
                {
                    flag = true;
                    break;
                }
            }
        }

        if(!flag)
        {
            status_code = SIP_FORBIDDEN;
            break;
        }

        
        pjmedia_sdp_session* sdp = NULL;
        if(rdata->msg_info.msg->body)
        {
            pjsip_rdata_sdp_info* sdp_info = pjsip_rdata_get_sdp_info(rdata);
            sdp = sdp_info->sdp;
        }

        if(sdp && sdp->media_count == 0)
        {
            status_code = SIP_BADREQUEST;
            break;
        }

        string devId(sdp->origin.user.ptr,sdp->origin.user.slen);
        DevTypeCode type = GlobalCtl::getSipDevInfo(devId);
        if(type == Error_Code)
        {
            status_code = SIP_FORBIDDEN;
            break;
        }
        id = devId;

        string tmp(sdp->name.ptr,sdp->name.slen);
        sdpInfo.sessionName = tmp;
        if(sdpInfo.sessionName == "PlayBack")
        {
            sdpInfo.startTime = sdp->time.start;
            sdpInfo.endTime = sdp->time.stop;
        }

        pjmedia_sdp_conn* c = sdp->conn;
        string dst_ip(c->addr.ptr,c->addr.slen);
        sdpInfo.dstRtpAddr = dst_ip;

        pjmedia_sdp_media* m = sdp->media[sdp->media_count-1];
        int sdp_port = m->desc.port;
        sdpInfo.dstRtpPort = sdp_port;

        string protol(m->desc.transport.ptr,m->desc.transport.slen);
        sdpInfo.sdp_protol = protol;
        int poto = 0;
        if(sdpInfo.sdp_protol == "TCP/RTP/AVP")
        {
            poto = 1;
            pjmedia_sdp_attr* attr = pjmedia_sdp_attr_find2(m->attr_count,m->attr,"setup",NULL);
            string setup(attr->value.ptr,attr->value.slen);
            sdpInfo.setUp = setup;
        }
		
		// sdpInfo.localRtpPort = GBOJ(gConfig)->popOneRandNum();
        ps = new SipPsCode(dst_ip,sdp_port);
        //ps = new SipPsCode(dst_ip,sdp_port,sdpInfo.localRtpPort,poto,sdpInfo.setUp,sdpInfo.startTime,sdpInfo.endTime);
        // {
        //     //需要在ps对象实例化后就插入到map中
        //     AutoMutexLock lck(&streamLock);
		// 	mediaInfoMap.insert(pair<string, SipPsCode*>(devId, ps));
        // }

    } while (0);
    ps->initPsEncode();
    resWithSdp(rdata,status_code,id,sdpInfo);
    recvFrame(&ps);
    //sendPsRtpStream(&ps);
    
    
    
}

// void SipGbPlay::sendPsRtpStream(SipPsCode** ps)
// {
//     int ret = (*ps)->initPsEncode();
//     if(ret < 0)
//     {
//         LOG(ERROR)<<"initPsEncode error:"<<ret;
//         return;
//     }

//     ret = recvFrame(&(*ps));
//     if(ret < 0)
//     {
//         LOG(ERROR)<<"recvFrame error";
//     }
//     return;


// }

// struct StreamHeader
// {
// 	//媒体类型，1-》audio 2-》video
// 	char type;
// 	//显示时间戳  毫秒级
// 	int pts;
// 	//本帧长度, 指后续负载长度, bytes
// 	int length;
// 	//是否为I帧
// 	int keyFrame;
// };
int SipGbPlay::recvFrame(SipPsCode** ps)
{
    // string out_video_path = "/home/ap/safm/ccbc/bin/forkCancer/out222222222.h264";
    // FILE* h264_fp = fopen(out_video_path.c_str(),"wb");
    // FILE* fp = fopen("/home/ap/safm/ccbc/bin/forkCancer/stream.file","rb");
    // if(!fp)
        // return -1;
	
	string out_video_path = "../conf/out.h264";
    FILE* h264_fp = fopen(out_video_path.c_str(),"wb");
    FILE* fp = fopen("../conf/stream.file","rb");
    if(!fp)
        return -1;
	
	//在这里我们先对录像时间进行个转换，转成毫秒,因为数据的头部里的pts为毫秒级别
	//int start = 0;
	//int end = 0;
	//bool backflag = false;  //定义个回放flag
	//if((*ps)->m_sTime >= 0 && (*ps)->m_eTime > 0)
	//{
	//	start = (*ps)->m_sTime * 1000;
	//	end = (*ps)->m_eTime * 1000;
	//	backflag = true;
	//}
    int ret = 0;
    unsigned char* buf = new unsigned char[sizeof(StreamHeader)];
    while(!feof(fp))
    {
        //需要判断下结束的flag
        //为true则发送rtp层的bye，并退出当前取流和推流的线程
        // if((*ps)->stopFlag)
        // {
        //     delete *ps;
        //     *ps = NULL;
        //     break;
        // }
        memset(buf,0,sizeof(StreamHeader));
        int size = fread(buf,1,sizeof(StreamHeader),fp);//读帧头
        if(size < 0)
        {
            ret = -1;
            break;
        }
        StreamHeader* header = (StreamHeader*)buf;
        if(header->type==2)
        {
            unsigned char* data = new unsigned char[header->length];
            fread(data,1,header->length,fp);//读帧数据

            if((*ps)->incomeVideoData(data,header->length,header->pts,header->keyFrame)<0)
            {
                continue;
            }


            size = fwrite(data,1,header->length,h264_fp);
            LOG(INFO)<<"write size:"<<size;
            delete data;
        }
        else
        {
            continue;
        }
        usleep(80000);
		// LOG(INFO)<<"header->pts:"<<header->pts;
		// unsigned char* data = new unsigned char[header->length];
		// fread(data,1,header->length,fp);
        // if(header->type == 2)
        // {
		// 	if(backflag)
		// 	{
		// 		if(header->pts < start)
		// 		{
		// 			continue;  //协议头里的pts小于起始时间，那么我们就不推流
		// 		}
		// 		else if(header->pts > end)
		// 		{
		// 			break;   //当pts大于录像结束时间后我们断流
		// 		}
		// 	}
            

        //     //ps demetex
        //     if((*ps)->incomeVideoData(data,header->length,header->pts,header->keyFrame)<0)
        //     {
        //         continue;
        //     }
		// 	//LOG(INFO)<<"header->length:"<<header->length;
        //     size = fwrite(data,1,header->length,h264_fp);
        //     //LOG(INFO)<<"write size:"<<size;
        //     delete data;
        // }
        // else
        // {
        //     continue;
        // }
        // usleep(90000);
    }
    delete buf;
    fclose(h264_fp);
    fclose(fp);
    if((*ps) != NULL)
    {
        delete *ps;
        *ps = NULL;
    }
   

    return ret;
}

void SipGbPlay::resWithSdp(pjsip_rx_data *rdata,int status_code,string devid,MediaInfo sdpInfo)
{
    pjsip_tx_data* tdata;
    pjsip_endpt_create_response(GBOJ(gSipServer)->GetEndPoint(),rdata,status_code,NULL,&tdata);
    pj_str_t type = {"Application",11};
    pj_str_t sdptype = {"SDP",3};
    if(status_code != SIP_SUCCESS)
    {
        tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&sdptype,&(pjsip_rdata_get_sdp_info(rdata)->body));
    }
    else
    {
        stringstream ss;
        ss<<"v="<<"0"<<"\r\n";
        ss<<"o="<<devid<<" 0 0 IN IP4 "<<GBOJ(gConfig)->sipIp()<<"\r\n";
        ss<<"s="<<"Play"<<"\r\n";
        ss<<"c="<<"IN IP4 "<<GBOJ(gConfig)->sipIp()<<"\r\n";
        ss<<"t="<<"0 0"<<"\r\n";
        ss<<"m=video "<<30000<<" "<<sdpInfo.sdp_protol<<" 96"<<"\r\n";
        ss<<"a=rtpmap:96 PS/90000"<<"\r\n";
        ss<<"a=sendonly"<<"\r\n";
        if(sdpInfo.setUp != "")
        {
            if(sdpInfo.setUp == "passive")
            {
                ss<<"a=setup:"<<"active"<<"\r\n";
            }
            else if(sdpInfo.setUp == "active")
            {
                ss<<"a=setup:"<<"passive"<<"\r\n";
            }
        }

        string sResp = ss.str();
        pj_str_t sdpData = pj_str((char*)sResp.c_str());
        tdata->msg->body = pjsip_msg_body_create(tdata->pool,&type,&sdptype,&sdpData);
    }
	SipMessage msg;
	msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());
	const pj_str_t contactHeader = pj_str("Contact");
	const pj_str_t param = pj_str(msg.Contact());
	pjsip_generic_string_hdr* customHeader = pjsip_generic_string_hdr_create(tdata->pool,&contactHeader,&param);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)customHeader);
	pjsip_response_addr res_addr;
	pj_status_t status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
    if (PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(tdata);
        return;
    }
    status = pjsip_endpt_send_response(GBOJ(gSipServer)->GetEndPoint(), &res_addr, tdata, NULL, NULL);
    if (PJ_SUCCESS != status)
    {
        pjsip_tx_data_dec_ref(tdata);
        return;
    }

    return;
}

void SipGbPlay::OnStateChanged(pjsip_inv_session *inv, pjsip_event *e)
{

}
void SipGbPlay::OnNewSession(pjsip_inv_session *inv, pjsip_event *e)
{

}
void SipGbPlay::OnMediaUpdate(pjsip_inv_session *inv_ses, pj_status_t status)
{
    
}