#ifndef MEDIA_FABRIC_COMMON_LOGGER_H
#define MEDIA_FABRIC_COMMON_LOGGER_H

#include <string>

namespace media_fabric {

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

class Logger
{
public:
    static Logger& instance();
    void configure(LogLevel level, const std::string& filePath, size_t maxBytes, int maxFiles);
    void log(LogLevel level, const std::string& module, const std::string& detail);
private:
    Logger();
    LogLevel m_level;
    std::string m_filePath;
    size_t m_maxBytes;
    int m_maxFiles;
};

} // namespace media_fabric

#endif
