#include"SipDirectory.h"
#include"SipDef.h"
#include"GlobalCtl.h"

Json::Value SipDirectory::m_jsonIn;
int SipDirectory::m_jsonInIndex=0;


//手动调用基类默认构造
SipDirectory::SipDirectory(tinyxml2::XMLElement* root)
:SipTaskBase()
{
    m_pRootElement=root;
}
SipDirectory::~SipDirectory()
{

}
pj_status_t SipDirectory::run(pjsip_rx_data *rdata)
{
    int status_code=SIP_SUCCESS;
    //1.解析message-body-xml数据
    SaveDir(status_code);
    //2.响应
    pj_status_t status=pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,NULL,NULL,NULL);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"pjsip_endpt_respond error";
    }
    return status;
}

void SipDirectory::SaveDir(int& status_code)
{
    tinyxml2::XMLElement* pRootElement=m_pRootElement;
    if(!pRootElement)
    {
        status_code=SIP_BADREQUEST;
        return; 
    }

    /*
    <?xml version="1.0"?>\n
    <Response>\n
        <CmdType>Catalog</CmdType>\n
        <SN>905</SN>\n
        <DeviceID>11000000002000000001</DeviceID>\n
        <SumNum>4</SumNum>\n
        <DeviceList Num="1">\n
            <item>\n
                <DeviceID>11000000002000000001</DeviceID>\n
                <Name>中国银行北京市分行</Name>\n
                <Manufacturer>unknow</Manufacturer>\n
                <Model>unknow</Model>\n
                <Owner>unknow</Owner>\n
                <CivilCode>11000</CivilCode>\n
                <Parental>1</Parental>\n
                <ParentID>11000000002000000001</ParentID>\n
                <SafetyWay>0</SafetyWay>\n
                <RegisterWay>1</RegisterWay>\n
                <Secrecy>0</Secrecy>\n
                <Status>ON</Status>\n
            </item>\n
        </DeviceList>\n
    </Response>\n
    */

    string strCenterDeviceID,strSumNum,strSn,strDeviceID,strName,strManufacturer,strModel,strOwner,
    strCivilCode,strParental,strParentID,strSafetyWay,strRegisterWay,strSecrecy,strStatus;
    tinyxml2::XMLElement* pElement=pRootElement->FirstChildElement("DeviceID");
    if(pElement&&pElement->GetText())
    {
        strCenterDeviceID=pElement->GetText();
    }
    if(!GlobalCtl::checkIsVaild(strCenterDeviceID))
    {
        status_code=SIP_BADREQUEST;
        return; 
    }
    pElement=pRootElement->FirstChildElement("SumNum");
    if(pElement&&pElement->GetText())
    {
        strSumNum=pElement->GetText();
    }
    pElement=pRootElement->FirstChildElement("SN");
    if(pElement&&pElement->GetText())
    {
        strSn=pElement->GetText();
    }

    pElement=pRootElement->FirstChildElement("DeviceList");
    if(pElement)
    {
        tinyxml2::XMLElement* pItem=pElement->FirstChildElement("item");
        while(pItem)
        {
            tinyxml2::XMLElement* pChild=pItem->FirstChildElement("DeviceID");
            if(pChild&&pChild->GetText())
            {
                strDeviceID=pChild->GetText();
                if(strDeviceID.length()==20)
                {
                    DevTypeCode type=GlobalCtl::getSipDevInfo(strDeviceID);
                    if (type==Ipc_Code || type==Camera_Code)
                    {
                        GlobalCtl::gRcvIpc=true;
                        LOG(INFO)<<"get ipc device";
                    }
                }
            }

            pChild=pItem->FirstChildElement("Manufacturer");
            if(pChild&&pChild->GetText())
            {
                strManufacturer=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Model");
            if(pChild&&pChild->GetText())
            {
                strModel=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Owner");
            if(pChild&&pChild->GetText())
            {
                strOwner=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("CivilCode");
            if(pChild&&pChild->GetText())
            {
                strCivilCode=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Parental");
            if(pChild&&pChild->GetText())
            {
                strParental=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("SafetyWay");
            if(pChild&&pChild->GetText())
            {
                strSafetyWay=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("RegisterWay");
            if(pChild&&pChild->GetText())
            {
                strRegisterWay=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Secrecy");
            if(pChild&&pChild->GetText())
            {
                strSecrecy=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Status");
            if(pChild&&pChild->GetText())
            {
                strStatus=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("Name");
            if(pChild&&pChild->GetText())
            {
                strName=pChild->GetText();
            }

            pChild=pItem->FirstChildElement("ParentID");
            if(pChild&&pChild->GetText())
            {
                strParentID=pChild->GetText();
            }

            GlobalCtl::get_global_mutex();
            m_jsonIn["catalog"][m_jsonInIndex]["DeviceID"]=strDeviceID;
            m_jsonIn["catalog"][m_jsonInIndex]["Name"]=strName;
            m_jsonIn["catalog"][m_jsonInIndex]["Parental"]=strParental;
            m_jsonIn["catalog"][m_jsonInIndex]["ParentID"]=strParentID;
            m_jsonInIndex++;
            GlobalCtl::free_global_mutex();



            pItem=pItem->NextSiblingElement();
        }
    }
    int sumNum=atoi(strSumNum.c_str());
    if(sumNum== m_jsonIn["catalog"].size())
    {
        GlobalCtl::get_global_mutex();
        GlobalCtl::getCatalogPayload=JsonParse(m_jsonIn).toString();
        GlobalCtl::free_global_mutex();
        m_jsonIn.clear();
        m_jsonInIndex=0;
        GBOJ(gThpool)->postInfo();//解除阻塞
    }
}