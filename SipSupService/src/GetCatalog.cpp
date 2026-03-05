#include"GetCatalog.h"
#include"GlobalCtl.h"
#include"SipMessage.h"
#include"XmlParser.h"

GetCatalog::GetCatalog()
{
    DirectoryGetPro(NULL);
}
GetCatalog::~GetCatalog()
{

}

void GetCatalog::DirectoryGetPro(void* param)
{
    SipMessage msg;
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
    {
        if(iter->registered)
        {
            msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());
            msg.setTo((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str());
            msg.setUrl((char*)iter->sipId.c_str(),(char*)iter->addrIp.c_str(),iter->sipPort);
            pj_str_t from=pj_str(msg.FromHeader());
            pj_str_t to=pj_str(msg.ToHeader());
            pj_str_t  line=pj_str(msg.RequestUrl());
            string method="MESSAGE";
            pjsip_tx_data* tdata;
            pjsip_method reqMethod={PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
            pj_status_t status= pjsip_endpt_create_request(GBOJ(gSipServer)->GetEndPoint(),&reqMethod,&line,&from,&to,NULL,NULL,-1,NULL,&tdata);
            if(PJ_SUCCESS!=status)
            {
                LOG(ERROR)<<"pjsip_endpt_create_request ERROR";
                return;
            }
            XmlParser parse;
            tinyxml2::XMLElement* rootNode=parse.AddRootNode((char*)"Query");
            parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"Catalog");
            int sn=random()%1024;
            char tmpStr[32]={0};
            sprintf(tmpStr,"%d",sn);
            parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
            parse.InsertSubNode(rootNode,(char*)"DeviceID",iter->sipId.c_str());
            char* xmlbuf=new char[1024];
            memset(xmlbuf,0,1024);
            parse.getXmlData(xmlbuf);
            LOG(INFO)<<"getXmlData:"<<xmlbuf;
            pj_str_t type=pj_str("Application");
            pj_str_t subtype=pj_str("MANSCDP+xml");
            pj_str_t xmldata=pj_str(xmlbuf);

            tdata->msg->body=pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);
            status=pjsip_endpt_send_request(GBOJ(gSipServer)->GetEndPoint(),tdata,-1,NULL,NULL);//这里只关心下级推送的实际节点目录，不关心响应回调是否为200
            if(PJ_SUCCESS!=status)
            {
                LOG(ERROR)<<"pjsip_endpt_send_request ERROR";
                return;
            }
        }
    }
    return;
}