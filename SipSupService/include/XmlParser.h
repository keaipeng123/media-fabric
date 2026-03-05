#ifndef _XMLPARSER_H
#define _XMLPARSER_H
#include "tinyxml2.h"
#include<string>

const int max_title=64;
#define XMLTITLE "<?xml version=\"1.0\"?>\n"
//#define XMLTITLE "<?xml version=\"1.0"\? encoding =\"gb2312\"?>\n"

/*
1.插入根节点，指定根节点的名称
2.在指定的节点下再插入子节点并设置节点名称和相应的值
3.添加xml的声明部分并返回整个xml的char*数据
*/
class XmlParser
{
    public:
    XmlParser();
    ~XmlParser();

    tinyxml2::XMLElement* AddRootNode(const char* rootName);
    tinyxml2::XMLElement* InsertSubNode(tinyxml2::XMLElement* parentNode,const char* itemName,const char* value);
    void SetNodeAttributes(tinyxml2::XMLElement* node,char* attrName,char* attrValue);
    void getXmlData(char* xmlBuf);

    private:
    tinyxml2::XMLDocument* m_document;
    tinyxml2::XMLElement* m_rootElement;
    char* m_xmlTitle;
};

#endif