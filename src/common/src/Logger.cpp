#include "Logger.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace media_fabric {
namespace {
std::mutex g_logMutex;
const char* levelName(LogLevel level) { return level == LOG_DEBUG ? "DEBUG" : level == LOG_INFO ? "INFO" : level == LOG_WARN ? "WARN" : "ERROR"; }
}

Logger::Logger() : m_level(LOG_INFO), m_maxBytes(0), m_maxFiles(0) {}
Logger& Logger::instance() { static Logger logger; return logger; }
void Logger::configure(LogLevel level, const std::string& filePath, size_t maxBytes, int maxFiles)
{ std::lock_guard<std::mutex> lock(g_logMutex); m_level = level; m_filePath = filePath; m_maxBytes = maxBytes; m_maxFiles = maxFiles; }
void Logger::log(LogLevel level, const std::string& module, const std::string& detail)
{
    if (level < m_level) return;
    std::lock_guard<std::mutex> lock(g_logMutex);
    const std::time_t now = std::time(NULL); std::tm local; localtime_r(&now, &local);
    std::ostringstream line; line << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << " [" << module << "] [" << levelName(level) << "] " << detail;
    std::cout << line.str() << std::endl;
    if (m_filePath.empty()) return;
    std::ifstream sizeInput(m_filePath.c_str(), std::ios::binary | std::ios::ate);
    if (m_maxBytes > 0 && sizeInput.good() && static_cast<size_t>(sizeInput.tellg()) >= m_maxBytes) {
        for (int i = m_maxFiles - 1; i >= 1; --i) { std::ostringstream from, to; from << m_filePath << "." << i; to << m_filePath << "." << (i + 1); std::rename(from.str().c_str(), to.str().c_str()); }
        std::string first = m_filePath + ".1"; std::rename(m_filePath.c_str(), first.c_str());
    }
    std::ofstream output(m_filePath.c_str(), std::ios::app); if (output.good()) output << line.str() << '\n';
}
} // namespace media_fabric
