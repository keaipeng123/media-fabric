#include "BusinessQueryService.h"

#include <sstream>
#include <vector>

namespace gb28181 {

namespace {

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        const unsigned char ch = static_cast<unsigned char>(*it);
        switch (ch)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (ch < 0x20)
            {
                static const char* digits = "0123456789abcdef";
                output << "\\u00" << digits[(ch >> 4) & 0x0f] << digits[ch & 0x0f];
            }
            else
            {
                output << *it;
            }
            break;
        }
    }
    return output.str();
}

void appendJsonString(std::ostringstream& output, const std::string& value)
{
    output << '"' << jsonEscape(value) << '"';
}

void appendStringArray(std::ostringstream& output, const std::vector<std::string>& values)
{
    output << '[';
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            output << ',';
        }
        appendJsonString(output, values[i]);
    }
    output << ']';
}

void appendItem(std::ostringstream& output, const ManscdpItem& item)
{
    output << '{';
    output << "\"device_id\":";
    appendJsonString(output, item.deviceId);
    output << ",\"name\":";
    appendJsonString(output, item.name);
    output << ",\"manufacturer\":";
    appendJsonString(output, item.manufacturer);
    output << ",\"model\":";
    appendJsonString(output, item.model);
    output << ",\"owner\":";
    appendJsonString(output, item.owner);
    output << ",\"civil_code\":";
    appendJsonString(output, item.civilCode);
    output << ",\"address\":";
    appendJsonString(output, item.address);
    output << ",\"parent_id\":";
    appendJsonString(output, item.parentId);
    output << ",\"safety_way\":";
    appendJsonString(output, item.safetyWay);
    output << ",\"register_way\":";
    appendJsonString(output, item.registerWay);
    output << ",\"secrecy\":";
    appendJsonString(output, item.secrecy);
    output << ",\"status\":";
    appendJsonString(output, item.status);
    output << ",\"longitude\":";
    appendJsonString(output, item.longitude);
    output << ",\"latitude\":";
    appendJsonString(output, item.latitude);
    output << ",\"file_path\":";
    appendJsonString(output, item.filePath);
    output << ",\"start_time\":";
    appendJsonString(output, item.startTime);
    output << ",\"end_time\":";
    appendJsonString(output, item.endTime);
    output << '}';
}

void appendItems(std::ostringstream& output, const std::vector<ManscdpItem>& items)
{
    output << '[';
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i != 0)
        {
            output << ',';
        }
        appendItem(output, items[i]);
    }
    output << ']';
}

std::string itemsJson(const std::string& type,
                      const std::string& peerId,
                      size_t totalItems,
                      const std::vector<ManscdpItem>& items)
{
    std::ostringstream output;
    output << "{\"type\":";
    appendJsonString(output, type);
    output << ",\"peer_id\":";
    appendJsonString(output, peerId);
    output << ",\"total_items\":" << totalItems;
    output << ",\"items\":";
    appendItems(output, items);
    output << '}';
    return output.str();
}

} // namespace

BusinessQueryService::BusinessQueryService(const BusinessState& state)
    : m_state(state)
{
}

std::string BusinessQueryService::summaryJson() const
{
    std::ostringstream output;
    output << "{\"catalog_items\":" << m_state.catalogItemCount();
    output << ",\"record_items\":" << m_state.recordItemCount();
    output << ",\"catalog_peers\":";
    appendStringArray(output, m_state.catalogPeerIds());
    output << ",\"record_peers\":";
    appendStringArray(output, m_state.recordPeerIds());
    output << '}';
    return output.str();
}

std::string BusinessQueryService::catalogJson(const std::string& peerId) const
{
    return itemsJson("catalog", peerId, m_state.catalogItemCount(peerId), m_state.catalogItems(peerId));
}

std::string BusinessQueryService::recordJson(const std::string& peerId) const
{
    return itemsJson("record", peerId, m_state.recordItemCount(peerId), m_state.recordItems(peerId));
}

} // namespace gb28181
