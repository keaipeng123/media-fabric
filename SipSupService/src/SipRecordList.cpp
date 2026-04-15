#include "SipRecordList.h"
#include"SipDef.h"
#include"GlobalCtl.h"

SipRecordList::SipRecordList(tinyxml2::XMLElement* root)
    :SipTaskBase()
{
    m_pRootElement=root;
}

SipRecordList::~SipRecordList()
{

}

pj_status_t SipRecordList::run(pjsip_rx_data *rdata)
{
    int status_code=200;
    SaveRecordList(status_code);
    pj_status_t status=pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }
    return status;
}

void SipRecordList::SaveRecordList(int& code)
{
    tinyxml2::XMLElement* pRootElement=m_pRootElement;
    if(!pRootElement)
    {
        code=SIP_BADREQUEST;
        return; 
    }

    string strSumNum,strDeviceID,strName,strStartTime,strEndTime,strType;
   
    tinyxml2::XMLElement* pElement=pRootElement->FirstChildElement("SumNum");
    if(pElement&&pElement->GetText())
    {
        strSumNum=pElement->GetText();
    }

    pElement=pRootElement->FirstChildElement("RecordList");
    if(pElement)
    {
        tinyxml2::XMLElement* pItem=pElement->FirstChildElement("item");
        while(pItem)
        {
            tinyxml2::XMLElement* pChild=pItem->FirstChildElement("DeviceID");
            if(pChild&&pChild->GetText())
            {
                strDeviceID=pChild->GetText();
            
            }

            pChild=pItem->FirstChildElement("Name");
            if(pChild&&pChild->GetText())
            {
                strName=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("StartTime");
            if(pChild&&pChild->GetText())
            {
                strStartTime=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("EndTime");
            if(pChild&&pChild->GetText())
            {
                strEndTime=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Type");
            if(pChild&&pChild->GetText())
            {
                strType=pChild->GetText();
            }

            pItem=pItem->NextSiblingElement();
        }
    }

    LOG(INFO)<<"strSumNum:"<<strSumNum<<",strDeviceID:"<<strDeviceID<<",strName:"<<strName<<",strStartTime:"<<strStartTime<<",strEndTime:"<<strEndTime<<",strType:"<<strType;

    return;
}