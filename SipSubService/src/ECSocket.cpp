#include"ECSocket.h"
#include"ECEventPoll.h"
#include<errno.h>
using namespace EC;

namespace {
int SetSocketFlags(int sockfd,int flags)
{
    return fcntl(sockfd,F_SETFL,flags);
}

bool IsRetryableConnectError(int err)
{
    return err==ECONNREFUSED || err==EHOSTUNREACH || err==ENETUNREACH;
}

int CreateBoundClientSocket(int localPort)
{
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG(ERROR) << "setsockopt SO_REUSEADDR error";
        close(sockfd);
        return -1;
    }

    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    client_addr.sin_family=AF_INET;
    client_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    client_addr.sin_port=htons(localPort);
    if(bind(sockfd,(struct sockaddr*)&client_addr,sizeof(client_addr))==-1)
    {
        LOG(ERROR)<<"socket bind error";
        close(sockfd);
        return -1;
    }

    return sockfd;
}

StatusType WaitForConnectResult(int sockfd,int* timeout)
{
    EventPoll eventPoll;
    if(eventPoll.init(1)!=0)
    {
        return ST_SYSERROR;
    }
    if(eventPoll.addEvent(sockfd,EC_POLLOUT)!=0||eventPoll.addEvent(sockfd,EC_POLLERR)!=0)
    {
        return ST_SYSERROR;
    }

    vector<PollEventType> pollEvents;
    while(true)
    {
        StatusType status=eventPoll.poll(pollEvents,timeout);
        if(status==ST_OK)
        {
            for(size_t i=0;i<pollEvents.size();i++)
            {
                if(pollEvents[i].sockfd!=sockfd)
                {
                    continue;
                }

                int socketError=0;
                socklen_t errorLen=sizeof(socketError);
                if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&socketError,&errorLen)<0)
                {
                    return ST_SYSERROR;
                }
                if(socketError!=0)
                {
                    errno=socketError;
                    return ST_SYSERROR;
                }
                return ST_OK;
            }
            continue;
        }
        if(status==ST_TIMEOUT)
        {
            errno=ETIMEDOUT;
            return ST_TIMEOUT;
        }
        errno=EIO;
        return ST_SYSERROR;
    }
}
}

//被动模式下先创建监听 socket，再 bind + listen，等客户端连进来。
//主动模式下先创建客户端 socket，再 bind + connect，去连接目标端。

StatusType ECSocket::createConnByPassive(int localport,int* lsockfd,int* connfd,int* timeout)
{
    if(lsockfd==NULL||connfd==NULL)
    {
        return ST_UNINIT;
    }
    *lsockfd=-1;
    *connfd=-1;

    LOG(INFO)<<"start tcpserver localport="<<localport;
    //向内核申请创建一个 TCP 套接字，返回一个文件描述符 sockfd
    //AF_INET 表示地址族是 IPv4
    //SOCK_STREAM 表示使用面向连接的字节流套接字
    //0 表示协议号让系统自动选择默认值
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return ST_SYSERROR;
    }
    //TIME_WAIT 四次挥手后，服务器端的连接会进入 TIME_WAIT 状态，持续一段时间（通常是 2 倍的最大报文生存时间，约为 4 分钟）。在此期间，如果再次尝试绑定相同的地址和端口，
    //可能会遇到 "Address already in use" 错误。通过设置 SO_REUSEADDR 选项，可以允许服务器在 TIME_WAIT 状态下重新绑定相同的地址和端口，从而避免这个问题。
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG(ERROR) << "setsockopt SO_REUSEADDR error";
        close(sockfd);
        return ST_SYSERROR;
    }

    struct sockaddr_in server_addr;//定义一个 IPv4 地址结构体变量
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;//指定地址类型：IPv4
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);//绑定本机所有网卡 IP（必须转网络字节序）
    server_addr.sin_port=htons(localport);// 设置要监听的端口号（必须转网络字节序）
    //把 sockfd 这个 socket 绑定到前面构造好的本地地址 server_addr 上
    if(bind(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1)
    {
        LOG(ERROR)<<"socket bind error";
        close(sockfd);
        return ST_SYSERROR;
    }
    //把一个已经 bind 好的流式 socket，转换成监听状态，准备接受客户端连接
    //允许大约 20 个待处理连接排队
    if(listen(sockfd,20)==-1)
    {
        LOG(ERROR)<<"socket listen error";
        close(sockfd);
        return ST_SYSERROR;
    }
    LOG(INFO)<<"tcpserver listening localport="<<localport;

    sockaddr_in clientAddr;
    socklen_t addrLen=sizeof(clientAddr);
    if(timeout==NULL)
    {
        //sockfd 负责监听
        //connfd 负责和某个具体客户端通信
        //clientAddr 用来拿到“是谁连进来了”
        //addrLen 输入时，告诉内核 clientAddr 缓冲区有多大 输出时，告诉你实际返回的地址占了多少字节
        int acceptedFd=accept(sockfd,(struct sockaddr*)&clientAddr,&addrLen);
        if(acceptedFd!=-1)
        {
            *lsockfd=sockfd;
            *connfd=acceptedFd;
            return ST_OK;
        }
        else
        {
            close(sockfd);
            return ST_SYSERROR;
        }
    }

    //如果设置有超时时间，则使用事件驱动的方式等待连接
    EventPoll eventPoll;
    if(eventPoll.init(1)!=0)
    {
        close(sockfd);
        return ST_SYSERROR;
    }
    if(eventPoll.addEvent(sockfd,EC_POLLIN)!=0)
    {
        close(sockfd);
        return ST_SYSERROR;
    }

    //int ret=-1;
    vector<PollEventType> pollEvents;

    
    StatusType status;
    while(true)
    {
        status=eventPoll.poll(pollEvents,timeout);
        LOG(INFO)<<"pollEvents.SIZE()"<<pollEvents.size();
        if (status==ST_OK)
        {
            for(size_t i=0;i<pollEvents.size();i++)
            {
                if(pollEvents[i].sockfd==sockfd)
                {
                    int acceptedFd=accept(sockfd,(struct sockaddr*)&clientAddr,&addrLen);
                    if(acceptedFd!=-1)
                    {
                        LOG(INFO)<<"EVENT TYPE:"<<pollEvents[i].outEvents;
                        *lsockfd=sockfd;
                        *connfd=acceptedFd;
                        return ST_OK;
                    }
                    else
                    {
                        close(sockfd);
                        return ST_SYSERROR;
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        else if(status==ST_SYSERROR)
        {
            LOG(ERROR)<<"POLL ERROR";
            break;
        }
        else if(status==ST_TIMEOUT)
        {
            LOG(ERROR)<<"TIME OUT";
            break;
        }
    }
    
    eventPoll.removeEvent(sockfd);
    close(sockfd);
    return status;
}
StatusType ECSocket::createConnByActive(int localPort,string dspip,int dstport,int* connfd,int* timeout)
{
    if(connfd==NULL)
    {
        return ST_UNINIT;
    }
    *connfd=-1;

    LOG(INFO)<<"tcpclient connect localport="<<localPort<<" remote="<<dspip<<":"<<dstport;
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=inet_addr(dspip.c_str());
    server_addr.sin_port=htons(dstport);

    int remainTimeout = timeout != NULL ? *timeout : -1;
    const int retryIntervalMs = 100;
    while(true)
    {
        int sockfd=CreateBoundClientSocket(localPort);
        if(sockfd<0)
        {
            return ST_SYSERROR;
        }

        int ret=-1;
        if(timeout==NULL)
        {
            ret=connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
            if(ret<0)
            {
                LOG(ERROR)<<"connect error errno="<<errno<<" msg="<<strerror(errno)<<" localport="<<localPort<<" remote="<<dspip<<":"<<dstport;
                close(sockfd);
                return ST_SYSERROR;
            }
            *connfd=sockfd;
            return ST_OK;
        }

        int oldFlags=fcntl(sockfd,F_GETFL,0);
        if(oldFlags<0||SetSocketFlags(sockfd,oldFlags|O_NONBLOCK)<0)
        {
            LOG(ERROR)<<"set nonblock error";
            close(sockfd);
            return ST_SYSERROR;
        }

        ret=connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
        if(ret==0)
        {
            SetSocketFlags(sockfd,oldFlags);
            *connfd=sockfd;
            return ST_OK;
        }
        if(ret<0&&errno!=EINPROGRESS)//内核没有接受这次异步建连，而是当场给了一个失败结果。需要重试
        {
            int connectErr = errno;
            SetSocketFlags(sockfd,oldFlags);
            close(sockfd);

            if((connectErr==ECONNREFUSED || connectErr==EHOSTUNREACH || connectErr==ENETUNREACH)
                && remainTimeout > 0)
            {
                const int waitMs = remainTimeout > retryIntervalMs ? retryIntervalMs : remainTimeout;
                usleep(waitMs * 1000);
                remainTimeout -= waitMs;
                continue;
            }

            errno=connectErr;
            LOG(ERROR)<<"connect error errno="<<connectErr<<" msg="<<strerror(connectErr)<<" localport="<<localPort<<" remote="<<dspip<<":"<<dstport;
            return ST_SYSERROR;
        }
        LOG(INFO)<<"connect in progress, wait for result...";

        int waitTimeout = remainTimeout;
        StatusType status=WaitForConnectResult(sockfd,&waitTimeout);
        if(remainTimeout > 0 && waitTimeout >= 0)
        {
            remainTimeout = waitTimeout;
        }
        if(SetSocketFlags(sockfd,oldFlags)<0)
        {
            LOG(ERROR)<<"restore socket flags error";
            close(sockfd);
            return ST_SYSERROR;
        }

        if(status==ST_OK)
        {
            *connfd=sockfd;
            return ST_OK;
        }

        if(status==ST_TIMEOUT)
        {
            LOG(ERROR)<<"TIME OUT";
        }
        else
        {
            const int connectErr = errno;
            //LOG(ERROR)<<"connect error after wait errno="<<connectErr<<" msg="<<strerror(connectErr)<<" localport="<<localPort<<" remote="<<dspip<<":"<<dstport;
            if(IsRetryableConnectError(connectErr) && remainTimeout > 0)
            {
                const int waitMs = remainTimeout > retryIntervalMs ? retryIntervalMs : remainTimeout;
                usleep(waitMs * 1000);
                remainTimeout -= waitMs;
                close(sockfd);
                continue;
            }
            LOG(ERROR)<<"connect error after wait errno="<<connectErr<<" msg="<<strerror(connectErr)<<" localport="<<localPort<<" remote="<<dspip<<":"<<dstport;
        }
        close(sockfd);
        return status;
    }

}