#include "ManagementServer.h"
#include "Logger.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

namespace gb28181 {
ManagementServer::ManagementServer() : m_fd(-1), m_stopped(true) {}
ManagementServer::~ManagementServer() { stop(); }
bool ManagementServer::start(const std::string& socketPath, const CommandHandler& handler, std::string* error)
{
    if (socketPath.empty() || socketPath.size() >= sizeof(sockaddr_un::sun_path) || !handler) { if(error) *error="invalid management socket configuration"; return false; }
    ::unlink(socketPath.c_str()); m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0); if (m_fd < 0) { if(error) *error="cannot create management socket"; return false; }
    sockaddr_un address; std::memset(&address, 0, sizeof(address)); address.sun_family=AF_UNIX; std::strncpy(address.sun_path, socketPath.c_str(), sizeof(address.sun_path)-1);
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(m_fd, 8) != 0) { if(error) *error="cannot listen on management socket"; ::close(m_fd); m_fd=-1; return false; }
    ::chmod(socketPath.c_str(), 0600); m_socketPath=socketPath; m_handler=handler; m_stopped=false; m_thread=std::thread(&ManagementServer::run,this); media_fabric::Logger::instance().log(media_fabric::LOG_INFO,"management","listening on "+socketPath); return true;
}
void ManagementServer::stop() { m_stopped=true; if(m_fd>=0){::shutdown(m_fd,SHUT_RDWR);::close(m_fd);m_fd=-1;} if(m_thread.joinable())m_thread.join(); if(!m_socketPath.empty())::unlink(m_socketPath.c_str()); }
void ManagementServer::run() { while(!m_stopped) { int client=::accept(m_fd,NULL,NULL); if(client<0) continue; char buffer[4096]; const ssize_t count=::read(client,buffer,sizeof(buffer)-1); if(count>0){buffer[count]='\0'; const std::string response=m_handler(std::string(buffer)); ::write(client,response.data(),response.size());} ::close(client); } }
}
