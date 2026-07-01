#ifndef GB28181_CORE_BUSINESSSTATE_H
#define GB28181_CORE_BUSINESSSTATE_H

#include "ManscdpMessage.h"

#include <map>
#include <string>
#include <vector>

namespace gb28181 {

class BusinessState
{
public:
    void clear();
    void updateCatalog(const std::string& peerId, const std::vector<ManscdpItem>& items);
    void updateRecords(const std::string& peerId, const std::vector<ManscdpItem>& items);
    size_t catalogItemCount() const;
    size_t recordItemCount() const;
    size_t catalogItemCount(const std::string& peerId) const;
    size_t recordItemCount(const std::string& peerId) const;
    std::vector<std::string> catalogPeerIds() const;
    std::vector<std::string> recordPeerIds() const;
    std::vector<ManscdpItem> catalogItems(const std::string& peerId) const;
    std::vector<ManscdpItem> recordItems(const std::string& peerId) const;
    std::string serialize() const;
    bool restore(const std::string& data, std::string* error);
    bool saveToFile(const std::string& path, std::string* error) const;
    bool loadFromFile(const std::string& path, std::string* error);

private:
    typedef std::map<std::string, std::vector<ManscdpItem> > ItemMap;

    static size_t itemCount(const ItemMap& items);
    static size_t itemCount(const ItemMap& items, const std::string& peerId);
    static std::vector<std::string> peerIds(const ItemMap& items);

    ItemMap m_catalogItems;
    ItemMap m_recordItems;
};

} // namespace gb28181

#endif
