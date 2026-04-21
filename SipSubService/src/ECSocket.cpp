#include"ECSocket.h"
#include"ECEventPoll.h"
using namespace EC;

int ECSocket::createConnByPassive(int localport,int* lsockfd,int* timeout)
{
    LOG(INFO)<<"start tcpserver...";
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return -1;
    }
    //TIME_WAIT 四次挥手后，服务器端的连接会进入 TIME_WAIT 状态，持续一段时间（通常是 2 倍的最大报文生存时间，约为 4 分钟）。在此期间，如果再次尝试绑定相同的地址和端口，
    //可能会遇到 "Address already in use" 错误。通过设置 SO_REUSEADDR 选项，可以允许服务器在 TIME_WAIT 状态下重新绑定相同的地址和端口，从而避免这个问题。
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char*)&sockfd,sizeof(sockfd));

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(localport);
    if(bind(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1)
    {
        LOG(ERROR)<<"socket bind error";
        return -1;
    }

    if(listen(sockfd,20)==-1)
    {
        LOG(ERROR)<<"socket listen error";
        close(sockfd);
        return -1;
    }

    sockaddr_in clientAddr;
    socklen_t addrLen=sizeof(clientAddr);
    if(timeout==NULL)
    {
        int connfd=accept(sockfd,(struct sockaddr*)&clientAddr,&addrLen);
        if(connfd!=-1)
        {
            *lsockfd=sockfd;
            return connfd;
        }
        else
        {
            close(sockfd);
            return -1;
        }
    }

    //如果设置有超时时间，则使用事件驱动的方式等待连接
    EventPoll eventPoll;
    eventPoll.init(1);
    eventPoll.addEvent(sockfd,EC_POLLIN);

    //int ret=-1;
    vector<PollEventType> pollEvents;

    
    StatusType status;
    while(true)
    {
        status=eventPoll.poll(pollEvents,timeout);
        LOG(INFO)<<"pollEvents.SIZE()"<<pollEvents.size();
        if (status==ST_OK)
        {
            for(int i=0;i<pollEvents.size();i++)
            {
                if(pollEvents[i].sockfd==sockfd)
                {
                    int connfd=accept(sockfd,(struct sockaddr*)&clientAddr,&addrLen);
                    if(connfd!=-1)
                    {
                        LOG(INFO)<<"EVENT TYPE:"<<pollEvents[i].outEvents;
                        *lsockfd=sockfd;
                        return connfd;
                    }
                    else
                    {
                        close(sockfd);
                        return -1;
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
        else
        {
            continue;
        }
    }
    
    eventPoll.removeEvent(sockfd);
    close(sockfd);
    return status;
}
int ECSocket::createConnByActive(int localPort,string dspip,int dstport,int* timeout)
{
    LOG(INFO)<<"tcpclient connect...";
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==sockfd)
    {
        LOG(ERROR)<<"socket create error";
        return -1;
    }
    //TIME_WAIT
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

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=inet_addr(dspip.c_str());
    server_addr.sin_port=htons(dstport);

    int ret=-1;
    if(timeout==NULL)
    {
        ret=connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
        if(ret<0)
        {
            LOG(ERROR)<<"connect error";
            close(sockfd);
            return ret;
        }
        return sockfd;
    }

    ret=connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
    if(ret<0)
    {
        LOG(ERROR)<<"connect error";
        close(sockfd);
        return ret;
    }
    EventPoll eventPoll;
    eventPoll.init(1);
    eventPoll.addEvent(sockfd,EC_POLLOUT);

    ret=-1;
    vector<PollEventType> pollEvents;
    while(true)
    {
        ret=eventPoll.poll(pollEvents,timeout);
        if (ret==ST_OK)
        {
            for(int i=0;i<pollEvents.size();i++)
            {
                if(pollEvents[i].sockfd==sockfd)
                {
                    return sockfd;
                }
                else
                {
                    continue;
                }
            }
        }
        else if(ret==ST_TIMEOUT)
        {
            LOG(ERROR)<<"TIME OUT";
            break;
        }
        else if(ret==ST_SYSERROR)
        {
            LOG(ERROR)<<"POLL ERROR";
            break;
        }
    }
    eventPoll.removeEvent(sockfd);
    close(sockfd);
    return ret;

}