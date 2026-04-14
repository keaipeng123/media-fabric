#include"GetRecordList.h"
#include"GlobalCtl.h"
#include"SipMessage.h"
#include"XmlParser.h"

GetRecordList::GetRecordList()
{
    RecordInfoGetPro(NULL);
}
GetRecordList::~GetRecordList()
{

}

void GetRecordList::RecordInfoGetPro(void *param)
{
    string devid="11000000001310000059";
    string playformId="11000000002000000001";
    SipMessage msg;
    AutoMutexLock lock(&GlobalCtl::globalLock);
    GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
    {
        if(iter->sipId==playformId)
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
            parse.InsertSubNode(rootNode,(char*)"CmdType",(char*)"RecordInfo");
            int sn=random()%1024;
            char tmpStr[32]={0};
            sprintf(tmpStr,"%d",sn);
            parse.InsertSubNode(rootNode,(char*)"SN",tmpStr);
            parse.InsertSubNode(rootNode,(char*)"DeviceID",devid.c_str()); 
            string starttime="2023-09-16T00:00:00";
            string endtime="2023-09-16T23:59:00";
            string recordType="all";

            parse.InsertSubNode(rootNode,(char*)"StartTime",starttime.c_str()); 
            parse.InsertSubNode(rootNode,(char*)"EndTime",endtime.c_str()); 
            parse.InsertSubNode(rootNode,(char*)"Type",recordType.c_str()); //查看的监控类型
            parse.InsertSubNode(rootNode,(char*)"IndistinctQuery","0"); //模糊查询 0不进行模糊查询 根据To的URI中的ID值确定查询的录像位置，
            //若为本域系统ID，则进行中心历史记录检索，若为前端设备ID，则进行前端设备历史记录检索；1进行模糊查询，两个检索都返回

            char* xmlbuf=new char[1024];
            memset(xmlbuf,0,1024);
            parse.getXmlData(xmlbuf);
            LOG(INFO)<<"getRecordXmlData:"<<xmlbuf;
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