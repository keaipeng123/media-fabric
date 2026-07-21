#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc, char* argv[])
{
    std::string socketPath = "/tmp/media-fabric.sock"; int first = 1;
    if (argc > 2 && std::string(argv[1]) == "--socket") { socketPath = argv[2]; first = 3; }
    if (argc <= first) { std::cerr << "Usage: mfcli [--socket path] peers|register|invite [peer-id]" << std::endl; return 1; }
    std::ostringstream request; for (int i=first;i<argc;++i) { if(i>first) request << ' '; request << argv[i]; } request << '\n';
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0); if(fd < 0) return 1;
    sockaddr_un address; std::memset(&address,0,sizeof(address)); address.sun_family=AF_UNIX;
    if(socketPath.size() >= sizeof(address.sun_path)) { ::close(fd); return 1; }
    std::strncpy(address.sun_path,socketPath.c_str(),sizeof(address.sun_path)-1);
    if(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) != 0) { std::cerr << "cannot connect to media-fabric at " << socketPath << std::endl; ::close(fd); return 1; }
    const std::string text=request.str(); ::write(fd,text.data(),text.size()); char buffer[8192]; ssize_t count=::read(fd,buffer,sizeof(buffer)-1); ::close(fd);
    if(count<=0) return 1; buffer[count]='\0'; std::cout << buffer; return std::string(buffer).find("ERROR")==0 ? 1 : 0;
}
