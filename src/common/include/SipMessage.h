#ifndef _SIPMESSAGE_H
#define _SIPMESSAGE_H

class SipMessage
{
public:
    SipMessage();
    ~SipMessage();

    void setFrom(char* fromUsr, char* fromIp);
    void setTo(char* toUser, char* toIp);
    void setUrl(char* sipId, char* urlIp, int urlPort, char* urlProto = (char*)"udp");
    void setContact(char* sipId, char* natIp, int natPort);

    inline char* FromHeader() { return m_fromHeader; }
    inline char* ToHeader() { return m_toHeader; }
    inline char* RequestUrl() { return m_requestUrl; }
    inline char* Contact() { return m_contact; }

private:
    char m_fromHeader[128];
    char m_toHeader[128];
    char m_requestUrl[128];
    char m_contact[128];
};

#endif
