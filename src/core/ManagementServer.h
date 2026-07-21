#ifndef MEDIA_FABRIC_CORE_MANAGEMENTSERVER_H
#define MEDIA_FABRIC_CORE_MANAGEMENTSERVER_H

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace gb28181 {
class ManagementServer {
public:
    typedef std::function<std::string(const std::string&)> CommandHandler;
    ManagementServer(); ~ManagementServer();
    bool start(const std::string& socketPath, const CommandHandler& handler, std::string* error);
    void stop();
private:
    void run();
    int m_fd; std::string m_socketPath; CommandHandler m_handler; std::atomic<bool> m_stopped; std::thread m_thread;
};
}
#endif
