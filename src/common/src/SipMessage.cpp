#include "SipMessage.h"

#include <cstdio>
#include <cstring>

SipMessage::SipMessage()
{
    memset(m_fromHeader, 0, sizeof(m_fromHeader));
    memset(m_toHeader, 0, sizeof(m_toHeader));
    memset(m_requestUrl, 0, sizeof(m_requestUrl));
    memset(m_contact, 0, sizeof(m_contact));
}

SipMessage::~SipMessage()
{
}

void SipMessage::setFrom(char* fromUsr, char* fromIp)
{
    snprintf(m_fromHeader, sizeof(m_fromHeader), "<sip:%s@%s>", fromUsr, fromIp);
}

void SipMessage::setTo(char* toUser, char* toIp)
{
    snprintf(m_toHeader, sizeof(m_toHeader), "<sip:%s@%s>", toUser, toIp);
}

void SipMessage::setUrl(char* sipId, char* urlIp, int urlPort, char* urlProto)
{
    snprintf(m_requestUrl, sizeof(m_requestUrl), "sip:%s@%s:%d;transport=%s", sipId, urlIp, urlPort, urlProto);
}

void SipMessage::setContact(char* sipId, char* natIp, int natPort)
{
    snprintf(m_contact, sizeof(m_contact), "sip:%s@%s:%d", sipId, natIp, natPort);
}
