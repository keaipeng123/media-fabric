#ifndef _EVENTMSGHANDLE_H_
#define _EVENTMSGHANDLE_H_
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "Common.h"

class EventMsgHandle
{
public:
    EventMsgHandle(const std::string& servIp,const int& servPort);
    ~EventMsgHandle();  

    int init();

private:
    std::string m_servIp;
    int m_servPort;

}

#endif