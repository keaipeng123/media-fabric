#include "ManscdpMessage.h"

#include <cstdlib>

namespace gb28181 {
namespace {

std::string trim(const std::string& value)
{
    std::string::size_type begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r' || value[begin] == '\n'))
    {
        ++begin;
    }

    std::string::size_type end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n'))
    {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string rootName(const std::string& body)
{
    std::string::size_type begin = body.find('<');
    while (begin != std::string::npos && begin + 1 < body.size() && (body[begin + 1] == '?' || body[begin + 1] == '!'))
    {
        begin = body.find('<', begin + 1);
    }
    if (begin == std::string::npos)
    {
        return "";
    }

    const std::string::size_type end = body.find_first_of(" \t\r\n>/", begin + 1);
    if (end == std::string::npos || end <= begin + 1)
    {
        return "";
    }
    return body.substr(begin + 1, end - begin - 1);
}

std::string elementText(const std::string& body, const std::string& name)
{
    const std::string beginTag = "<" + name;
    const std::string endTag = "</" + name + ">";

    std::string::size_type begin = body.find(beginTag);
    if (begin == std::string::npos)
    {
        return "";
    }
    begin = body.find('>', begin + beginTag.size());
    if (begin == std::string::npos)
    {
        return "";
    }
    ++begin;

    const std::string::size_type end = body.find(endTag, begin);
    if (end == std::string::npos || end < begin)
    {
        return "";
    }
    return trim(body.substr(begin, end - begin));
}

int toInt(const std::string& value)
{
    return value.empty() ? 0 : std::atoi(value.c_str());
}

std::string elementBlock(const std::string& body, const std::string& name)
{
    const std::string beginTag = "<" + name;
    const std::string endTag = "</" + name + ">";

    std::string::size_type begin = body.find(beginTag);
    if (begin == std::string::npos)
    {
        return "";
    }
    begin = body.find('>', begin + beginTag.size());
    if (begin == std::string::npos)
    {
        return "";
    }
    ++begin;

    const std::string::size_type end = body.find(endTag, begin);
    if (end == std::string::npos || end < begin)
    {
        return "";
    }
    return body.substr(begin, end - begin);
}

void parseItemsInBlock(const std::string& block, std::vector<ManscdpItem>* items)
{
    if (items == NULL || block.empty())
    {
        return;
    }

    std::string::size_type cursor = 0;
    while (cursor < block.size())
    {
        std::string::size_type begin = block.find("<Item", cursor);
        std::string endTag = "</Item>";
        if (begin == std::string::npos)
        {
            begin = block.find("<item", cursor);
            endTag = "</item>";
        }
        if (begin == std::string::npos)
        {
            break;
        }

        begin = block.find('>', begin);
        if (begin == std::string::npos)
        {
            break;
        }
        ++begin;

        const std::string::size_type end = block.find(endTag, begin);
        if (end == std::string::npos)
        {
            break;
        }

        const std::string itemBody = block.substr(begin, end - begin);
        ManscdpItem item;
        item.deviceId = elementText(itemBody, "DeviceID");
        item.name = elementText(itemBody, "Name");
        item.manufacturer = elementText(itemBody, "Manufacturer");
        item.model = elementText(itemBody, "Model");
        item.owner = elementText(itemBody, "Owner");
        item.civilCode = elementText(itemBody, "CivilCode");
        item.address = elementText(itemBody, "Address");
        item.parentId = elementText(itemBody, "ParentID");
        item.safetyWay = elementText(itemBody, "SafetyWay");
        item.registerWay = elementText(itemBody, "RegisterWay");
        item.secrecy = elementText(itemBody, "Secrecy");
        item.status = elementText(itemBody, "Status");
        item.longitude = elementText(itemBody, "Longitude");
        item.latitude = elementText(itemBody, "Latitude");
        item.filePath = elementText(itemBody, "FilePath");
        item.startTime = elementText(itemBody, "StartTime");
        item.endTime = elementText(itemBody, "EndTime");
        items->push_back(item);

        cursor = end + endTag.size();
    }
}

std::vector<ManscdpItem> parseItems(const std::string& body)
{
    std::vector<ManscdpItem> items;
    parseItemsInBlock(elementBlock(body, "DeviceList"), &items);
    parseItemsInBlock(elementBlock(body, "RecordList"), &items);
    return items;
}

} // namespace

ManscdpMessage::ManscdpMessage()
    : sumNum(0)
{
}

bool ManscdpMessage::valid() const
{
    return !root.empty() && !cmdType.empty();
}

std::string ManscdpMessage::event() const
{
    return valid() ? root + "/" + cmdType : "";
}

size_t ManscdpMessage::itemCount() const
{
    return items.size();
}

bool parseManscdpMessage(const std::string& body, ManscdpMessage* message)
{
    if (body.empty() || message == NULL)
    {
        return false;
    }

    ManscdpMessage parsed;
    parsed.root = rootName(body);
    parsed.cmdType = elementText(body, "CmdType");
    parsed.sn = elementText(body, "SN");
    parsed.deviceId = elementText(body, "DeviceID");
    parsed.result = elementText(body, "Result");
    parsed.startTime = elementText(body, "StartTime");
    parsed.endTime = elementText(body, "EndTime");
    parsed.sumNum = toInt(elementText(body, "SumNum"));
    parsed.items = parseItems(body);

    if (!parsed.valid())
    {
        return false;
    }

    *message = parsed;
    return true;
}

} // namespace gb28181
