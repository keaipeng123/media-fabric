#include"XmlParser.h"
XmlParser::XmlParser()
{
    m_document=new tinyxml2::XMLDocument();
    m_rootElement=NULL;
    m_xmlTitle=new char[max_title];
    memset(m_xmlTitle,0,max_title);//将 m_xmlTitle 指向的内存块的前 max_title 个字节全部设置为 0（即清零）
    memcpy(m_xmlTitle,XMLTITLE,max_title);//将 XMLTITLE 的内容逐字节复制到 m_xmlTitle 中，复制长度为 max_title 字节
}
XmlParser::~XmlParser()
{
    if(m_document)
    {
        delete m_document;
        m_document=NULL; //析构函数中对象即将销毁，成员指针即将销毁，此时没有意义
    }
    // if(m_rootElement)
    // {
    //     delete m_rootElement;
    //     m_rootElement=NULL;
    // }
    //document释放后这里已经释放了

    delete []m_xmlTitle;
    //m_xmlTitle=NULL;
}

tinyxml2::XMLElement* XmlParser::AddRootNode(const char* rootName)
{
    if(m_document)
    {
        m_rootElement=m_document->NewElement(rootName);
        m_document->LinkEndChild(m_rootElement);
    }
    return m_rootElement;
}
tinyxml2::XMLElement* XmlParser::InsertSubNode(tinyxml2::XMLElement* parentNode,const char* itemName,const char* value)
{
    if(!parentNode)
        return NULL;
    tinyxml2::XMLElement* insertNode =m_document->NewElement(itemName);
    parentNode->LinkEndChild(insertNode);
    if(strlen(value)!=0)
    {
        tinyxml2::XMLText* txt=m_document->NewText(value);
        insertNode->LinkEndChild(txt);
    }
    return insertNode;

}

void XmlParser::SetNodeAttributes(tinyxml2::XMLElement* node,char* attrName,char* attrValue)
{
    if(!node)
        return;
    node->SetAttribute(attrName,attrValue);
}

void XmlParser::getXmlData(char* xmlBuf)
{
    //判断地址是否有效
    if(!xmlBuf)
        return;

    tinyxml2::XMLPrinter printer;
    //需要经常检查，因为内存泄漏或者非法访问都会导致程序崩溃
    if(m_document)
    {
        m_document->Accept(&printer);
        if(strlen(m_xmlTitle)!=0)
            strncpy(xmlBuf,m_xmlTitle,max_title);//将 m_xmlTitle 的字符串内容复制到 xmlBuf，最多复制 max_title 个字符
        strcat(xmlBuf,(char*)printer.CStr());//字符串拼接 CStr()返回生成的 XML 内容的 C 风格字符串（即以 \0 结尾的字符数组）
        
    }
}

//memcpy和strncpy比较
/*
memcpy作用：
内存块复制：将 XMLTITLE 指向的内存块的 前 max_title 个字节 逐字节复制到 m_xmlTitle 缓冲区。
不依赖终止符：无论 XMLTITLE 是否包含字符串终止符 \0，都会严格复制 max_title 字节。
典型场景：
用于复制 二进制数据（如结构体、数组）或 固定长度的字符串。
当需要精确控制复制的字节数时（例如复制标题的完整内容，包括可能的填充字节）。
*/

/*
strncpy作用：
安全字符串复制：将 m_xmlTitle 中的字符串复制到 xmlBuf，最多复制 max_title 个字符。
处理终止符：
如果 m_xmlTitle 的字符串长度（含 \0）小于 max_title：
会复制整个字符串（包括 \0），并在目标缓冲区剩余空间填充 \0。
如果 m_xmlTitle 的字符串长度 ≥ max_title：
仅复制前 max_title 个字符，且 不会自动添加终止符。
典型场景：
用于安全地复制字符串，避免缓冲区溢出（如用户输入的标题）。
当需要确保目标缓冲区以 \0 终止时（需手动检查）。
*/