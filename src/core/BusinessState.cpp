#include "BusinessState.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace gb28181 {

namespace {

const char* kSnapshotHeader = "GB28181_BUSINESS_STATE_V1";
const size_t kSnapshotFieldCountV1 = 19;
const size_t kSnapshotFieldCountWithParental = 20;

void setError(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

std::string escapeValue(const std::string& value)
{
    std::string result;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        switch (*it)
        {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        default:
            result += *it;
            break;
        }
    }
    return result;
}

bool unescapeValue(const std::string& value, std::string* result)
{
    result->clear();
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] != '\\')
        {
            result->push_back(value[i]);
            continue;
        }
        if (i + 1 >= value.size())
        {
            return false;
        }
        const char escaped = value[++i];
        switch (escaped)
        {
        case '\\':
            result->push_back('\\');
            break;
        case 't':
            result->push_back('\t');
            break;
        case 'n':
            result->push_back('\n');
            break;
        case 'r':
            result->push_back('\r');
            break;
        default:
            return false;
        }
    }
    return true;
}

std::vector<std::string> splitFields(const std::string& line)
{
    std::vector<std::string> fields;
    std::string current;
    for (std::string::const_iterator it = line.begin(); it != line.end(); ++it)
    {
        if (*it == '\t')
        {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(*it);
    }
    fields.push_back(current);
    return fields;
}

void appendField(std::ostringstream& output, const std::string& value)
{
    output << '\t' << escapeValue(value);
}

void appendItemLine(std::ostringstream& output,
                    const char* type,
                    const std::string& peerId,
                    const ManscdpItem& item)
{
    output << type;
    appendField(output, peerId);
    appendField(output, item.deviceId);
    appendField(output, item.name);
    appendField(output, item.manufacturer);
    appendField(output, item.model);
    appendField(output, item.owner);
    appendField(output, item.civilCode);
    appendField(output, item.address);
    appendField(output, item.parentId);
    appendField(output, item.safetyWay);
    appendField(output, item.registerWay);
    appendField(output, item.secrecy);
    appendField(output, item.status);
    appendField(output, item.longitude);
    appendField(output, item.latitude);
    appendField(output, item.filePath);
    appendField(output, item.startTime);
    appendField(output, item.endTime);
    appendField(output, item.parental);
    output << '\n';
}

bool decodeField(const std::vector<std::string>& fields,
                 size_t index,
                 std::string* value,
                 std::string* error)
{
    if (!unescapeValue(fields[index], value))
    {
        setError(error, "invalid escape sequence in business state snapshot");
        return false;
    }
    return true;
}

bool decodeItem(const std::vector<std::string>& fields,
                std::string* peerId,
                ManscdpItem* item,
                std::string* error)
{
    return decodeField(fields, 1, peerId, error) &&
           decodeField(fields, 2, &item->deviceId, error) &&
           decodeField(fields, 3, &item->name, error) &&
           decodeField(fields, 4, &item->manufacturer, error) &&
           decodeField(fields, 5, &item->model, error) &&
           decodeField(fields, 6, &item->owner, error) &&
           decodeField(fields, 7, &item->civilCode, error) &&
           decodeField(fields, 8, &item->address, error) &&
           decodeField(fields, 9, &item->parentId, error) &&
           decodeField(fields, 10, &item->safetyWay, error) &&
           decodeField(fields, 11, &item->registerWay, error) &&
           decodeField(fields, 12, &item->secrecy, error) &&
           decodeField(fields, 13, &item->status, error) &&
           decodeField(fields, 14, &item->longitude, error) &&
           decodeField(fields, 15, &item->latitude, error) &&
           decodeField(fields, 16, &item->filePath, error) &&
           decodeField(fields, 17, &item->startTime, error) &&
           decodeField(fields, 18, &item->endTime, error) &&
           (fields.size() == kSnapshotFieldCountV1 ||
            decodeField(fields, 19, &item->parental, error));
}

} // namespace

void BusinessState::clear()
{
    m_catalogItems.clear();
    m_recordItems.clear();
}

void BusinessState::updateCatalog(const std::string& peerId, const std::vector<ManscdpItem>& items)
{
    m_catalogItems[peerId] = items;
}

void BusinessState::appendCatalog(const std::string& peerId, const std::vector<ManscdpItem>& items)
{
    std::vector<ManscdpItem>& catalog = m_catalogItems[peerId];
    for (std::vector<ManscdpItem>::const_iterator item = items.begin(); item != items.end(); ++item)
    {
        std::vector<ManscdpItem>::iterator existing = std::find_if(
            catalog.begin(), catalog.end(),
            [item](const ManscdpItem& candidate) { return candidate.deviceId == item->deviceId; });
        if (existing == catalog.end())
        {
            catalog.push_back(*item);
        }
        else
        {
            *existing = *item;
        }
    }
}

void BusinessState::updateRecords(const std::string& peerId, const std::vector<ManscdpItem>& items)
{
    m_recordItems[peerId] = items;
}

size_t BusinessState::catalogItemCount() const
{
    return itemCount(m_catalogItems);
}

size_t BusinessState::recordItemCount() const
{
    return itemCount(m_recordItems);
}

size_t BusinessState::catalogItemCount(const std::string& peerId) const
{
    return itemCount(m_catalogItems, peerId);
}

size_t BusinessState::recordItemCount(const std::string& peerId) const
{
    return itemCount(m_recordItems, peerId);
}

std::vector<std::string> BusinessState::catalogPeerIds() const
{
    return peerIds(m_catalogItems);
}

std::vector<std::string> BusinessState::recordPeerIds() const
{
    return peerIds(m_recordItems);
}

std::vector<ManscdpItem> BusinessState::catalogItems(const std::string& peerId) const
{
    ItemMap::const_iterator it = m_catalogItems.find(peerId);
    return it == m_catalogItems.end() ? std::vector<ManscdpItem>() : it->second;
}

std::vector<ManscdpItem> BusinessState::recordItems(const std::string& peerId) const
{
    ItemMap::const_iterator it = m_recordItems.find(peerId);
    return it == m_recordItems.end() ? std::vector<ManscdpItem>() : it->second;
}

std::string BusinessState::serialize() const
{
    std::ostringstream output;
    output << kSnapshotHeader << '\n';
    for (ItemMap::const_iterator it = m_catalogItems.begin(); it != m_catalogItems.end(); ++it)
    {
        for (std::vector<ManscdpItem>::const_iterator item = it->second.begin(); item != it->second.end(); ++item)
        {
            appendItemLine(output, "CATALOG", it->first, *item);
        }
    }
    for (ItemMap::const_iterator it = m_recordItems.begin(); it != m_recordItems.end(); ++it)
    {
        for (std::vector<ManscdpItem>::const_iterator item = it->second.begin(); item != it->second.end(); ++item)
        {
            appendItemLine(output, "RECORD", it->first, *item);
        }
    }
    return output.str();
}

bool BusinessState::restore(const std::string& data, std::string* error)
{
    std::istringstream input(data);
    std::string line;
    if (!std::getline(input, line))
    {
        setError(error, "empty business state snapshot");
        return false;
    }
    if (!line.empty() && line[line.size() - 1] == '\r')
    {
        line.erase(line.size() - 1);
    }
    if (line != kSnapshotHeader)
    {
        setError(error, "unsupported business state snapshot header");
        return false;
    }

    ItemMap catalogItems;
    ItemMap recordItems;
    size_t lineNumber = 1;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }
        if (line.empty())
        {
            continue;
        }

        const std::vector<std::string> fields = splitFields(line);
        if (fields.size() != kSnapshotFieldCountV1 && fields.size() != kSnapshotFieldCountWithParental)
        {
            std::ostringstream message;
            message << "invalid business state snapshot field count at line " << lineNumber;
            setError(error, message.str());
            return false;
        }

        std::string peerId;
        ManscdpItem item;
        if (!decodeItem(fields, &peerId, &item, error))
        {
            return false;
        }

        if (fields[0] == "CATALOG")
        {
            catalogItems[peerId].push_back(item);
        }
        else if (fields[0] == "RECORD")
        {
            recordItems[peerId].push_back(item);
        }
        else
        {
            std::ostringstream message;
            message << "unknown business state snapshot item type at line " << lineNumber;
            setError(error, message.str());
            return false;
        }
    }

    m_catalogItems.swap(catalogItems);
    m_recordItems.swap(recordItems);
    return true;
}

bool BusinessState::saveToFile(const std::string& path, std::string* error) const
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!output)
    {
        setError(error, "failed to open business state snapshot for writing: " + path);
        return false;
    }
    output << serialize();
    if (!output)
    {
        setError(error, "failed to write business state snapshot: " + path);
        return false;
    }
    return true;
}

bool BusinessState::loadFromFile(const std::string& path, std::string* error)
{
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input)
    {
        setError(error, "failed to open business state snapshot for reading: " + path);
        return false;
    }
    std::ostringstream data;
    data << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        setError(error, "failed to read business state snapshot: " + path);
        return false;
    }
    return restore(data.str(), error);
}

size_t BusinessState::itemCount(const ItemMap& items)
{
    size_t count = 0;
    for (ItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        count += it->second.size();
    }
    return count;
}

size_t BusinessState::itemCount(const ItemMap& items, const std::string& peerId)
{
    ItemMap::const_iterator it = items.find(peerId);
    return it == items.end() ? 0 : it->second.size();
}

std::vector<std::string> BusinessState::peerIds(const ItemMap& items)
{
    std::vector<std::string> ids;
    for (ItemMap::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        ids.push_back(it->first);
    }
    return ids;
}

} // namespace gb28181
