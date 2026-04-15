#include"SipRecordList.h"
#include"GlobalCtl.h"
#include"XmlParser.h"
#include"SipMessage.h"

SipRecordList::SipRecordList()
{

}
SipRecordList::~SipRecordList()
{

}

void SipRecordList::run(pjsip_rx_data *rdata)
{
    int sn;
    string devid;
    resRecordInfo(rdata,sn,devid);

    sendRecordList(rdata,sn,devid);
    return;
}

void SipRecordList::resRecordInfo(pjsip_rx_data *rdata,int& sn,string& devid)
{
    int status_code=200;
    pjsip_msg* msg=rdata->msg_info.msg;
    tinyxml2::XMLDocument* xml =new tinyxml2::XMLDocument();
    if(xml)
    {
        xml->Parse((char*)msg->body->data);
    }
    do
    {
        tinyxml2::XMLElement* pRootElement=xml->RootElement();
        if (!pRootElement)
        {
            status_code=400;
            LOG(ERROR)<<"pRootElement";
            break;
        }
        string strSn;
        tinyxml2::XMLElement* pElement=pRootElement->FirstChildElement("DeviceID");
        if(pElement&&pElement->GetText())
        {
            devid=pElement->GetText();
        }
        
        pElement=pRootElement->FirstChildElement("SN");
        if(pElement)
        {
            strSn=pElement->GetText();
        }
        sn=atoi(strSn.c_str());

    } while (0);

    pj_status_t status=pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if (PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond";
    }
    return;

}

void SipRecordList::sendRecordList(pjsip_rx_data *rdata,int sn,string devid)
{
    XmlParser parse;//栈上创建
    tinyxml2::XMLElement* rootNode=parse.AddRootNode((char*)"Response");
    parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"RecordInfo");
    char tmpStr[32]={0};
    sprintf(tmpStr,"%d",sn);
    parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
    parse.InsertSubNode(rootNode,(char*)"DeviceID",devid.c_str());
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",1);
    parse.InsertSubNode(rootNode,(char*)"SumNum",tmpStr);
    tinyxml2::XMLElement* itemNode=parse.InsertSubNode(rootNode,(char*)"RecordList",(char*)"");
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",1);
    parse.SetNodeAttributes(itemNode,(char*)"Num",tmpStr);

    tinyxml2::XMLElement* node=parse.InsertSubNode(itemNode,(char*)"item",(char*)"");
    parse.InsertSubNode(node,(char*)"DeviceID",devid.c_str());
    parse.InsertSubNode(node,(char*)"Name",devid.c_str());
    parse.InsertSubNode(node,(char*)"Type","time");
    int sTime=10;
    int eTime=20;
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",sTime);
    parse.InsertSubNode(node,(char*)"StartTime",tmpStr);
    memset(tmpStr,0,sizeof(tmpStr));
    sprintf(tmpStr,"%d",eTime);
    parse.InsertSubNode(node,(char*)"EndTime",tmpStr);

    char* sendData=new char[BODY_SIZE];
    parse.getXmlData(sendData);

    //发送
    pjsip_msg* msg=rdata->msg_info.msg;
    string fromId=parseFromId(msg);
    SipMessage sipMsg;
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
    {
        if(iter->sipId==fromId)
        {
            sipMsg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
            sipMsg.setTo((char*)fromId.c_str(),(char*)iter->addrIp.c_str());
            sipMsg.setUrl((char*)fromId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
        }
        else
        {
            return;
        }
    }
    string method="NOTIFY";
    pjsip_method reqMethod={PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};

    pj_str_t line=pj_str(sipMsg.RequestUrl());
    pj_str_t from=pj_str(sipMsg.FromHeader());
    pj_str_t to=pj_str(sipMsg.ToHeader());

    pjsip_tx_data* tdata;

    pj_status_t status=pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
        return;
    }

    pj_str_t type=pj_str("Application");
    pj_str_t subtype=pj_str("MANSCDP+xml");
    pj_str_t xmldata=pj_str(sendData);

    tdata->msg->body =pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);

    status=pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
        return;
    }
    return;
}