#ifndef GB28181_CORE_MANSCDPMESSAGE_H
#define GB28181_CORE_MANSCDPMESSAGE_H

#include <string>
#include <vector>

namespace gb28181 {

struct ManscdpItem
{
    std::string deviceId;
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string owner;
    std::string civilCode;
    std::string address;
    std::string parentId;
    std::string safetyWay;
    std::string registerWay;
    std::string secrecy;
    std::string status;
    std::string longitude;
    std::string latitude;
    std::string filePath;
    std::string startTime;
    std::string endTime;
};

struct ManscdpMessage
{
    std::string root;
    std::string cmdType;
    std::string sn;
    std::string deviceId;
    std::string result;
    std::string startTime;
    std::string endTime;
    int sumNum;
    std::vector<ManscdpItem> items;

    ManscdpMessage();
    bool valid() const;
    std::string event() const;
    size_t itemCount() const;
};

bool parseManscdpMessage(const std::string& body, ManscdpMessage* message);

} // namespace gb28181

#endif
