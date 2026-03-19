#include"SipGbPlay.h"
#include"Gb28181Session.h"

SipGbPlay::SipGbPlay()
{

}

SipGbPlay::~SipGbPlay()
{

}

void SipGbPlay::OnStateChanged(pjsip_inv_session *inv, pjsip_event *e)
{
	LOG(INFO)<<"OnStateChanged";
}

void SipGbPlay::OnNewSession(pjsip_inv_session *inv, pjsip_event *e)
{
	LOG(INFO)<<"OnNewSession";
}

//实现解析sdp处理逻辑，rtp会话的创建和链接
void SipGbPlay::OnMediaUpdate(pjsip_inv_session *inv_ses, pj_status_t status)
{
	LOG(INFO)<<"OnMediaUpdate";
    if(NULL == inv_ses)
    {
        return;
    }
    pjsip_tx_data* tdata;
    const pjmedia_sdp_session* remote_sdp=NULL;
    pjmedia_sdp_neg_get_active_remote(inv_ses->neg,&remote_sdp);

    if(NULL==remote_sdp)
    {
        //pjsip底层会自动回复ack，如果sdp解析有问题，这里会拦截pjsip底层自动发送的ack
        pjsip_inv_end_session(inv_ses,PJSIP_SC_UNSUPPORTED_MEDIA_TYPE,NULL,&tdata);
        pjsip_inv_send_msg(inv_ses,tdata);
        return;
    }

    if(remote_sdp->media_count<=0||NULL==remote_sdp->media[remote_sdp->media_count-1])
    {
        pjsip_inv_end_session(inv_ses,PJSIP_SC_FORBIDDEN,NULL,&tdata);
        pjsip_inv_send_msg(inv_ses,tdata);
        return;
    }
    //解析sdp
    pjmedia_sdp_media *m=remote_sdp->media[remote_sdp->media_count-1];
    int sdp_port=m->desc.port;
    pjmedia_sdp_conn* c=remote_sdp->conn;
    string ip(c->addr.ptr,c->addr.slen);

    LOG(INFO)<<"remote rtp ip:"<<ip<<" remote rtp port:"<<sdp_port;

    //Gb28181Session* rtpsession = dynamic_cast<Gb28181Session*>((Session*)inv_ses->mod_data[0]);
    //rtpsession->CreateRtpSession(ip,sdp_port);
    Gb28181Session* session = (Gb28181Session*)inv_ses->mod_data[0];
    session->CreateRtpSession();

    return;

}

void SipGbPlay::OnSendAck(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    LOG(INFO)<<"OnSendAck";
    pjsip_tx_data tdata;
	pjsip_inv_create_ack(inv, rdata->msg_info.cseq->cseq,&inv->last_ack);
	// pjsip_tpselector tp_sel;
	// tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
	// tp_sel.u.transport = inv->invite_tsx->transport;
	// pjsip_dlg_set_transport(inv->dlg, &tp_sel);
    pjsip_dlg_send_request(inv->dlg, inv->last_ack, -1, NULL);
	//pjsip_inv_send_msg(inv, tdata);
    //printBodyData((char*)rdata->msg_info.msg->body->data);

    return;
}