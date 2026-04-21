#ifndef _ECSOCKET_H
#define _ECSOCKET_H

#include<unistd.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<sys/un.h>
#include<arpa/inet.h>
#include<sys/time.h>
#include<netdb.h>
#include<netinet/tcp.h>
#include<sys/stat.h>
#include"Common.h"

namespace EC
{
    class ECSocket
    {
        public:
        static int createConnByPassive(int localport,int* lsockfd,int* timeout);//被动（服务器）
        static int createConnByActive(int localPort,string dspip,int dstport,int* timeout);//主动（客户端）
        private:
        ECSocket(){}
        ~ECSocket(){}
    };
}

#endif