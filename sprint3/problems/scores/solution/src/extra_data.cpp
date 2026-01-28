#include "extra_data.h"

#include <cstdio>
#include <stdexcept>

namespace extra_data {
namespace json = boost::json;

std::string ExtraData::GetMapValue(const std::string& name) const {
    auto it = extra_.find(name);
    if (it == extra_.end()) {
        throw std::out_of_range("GetMapValue: map not found: " + name);
    }
    return json::serialize(it->second);
}

std::optional<unsigned long> ExtraData::GetNumberLootforMap(const std::string& name) const {
    auto it = extra_.find(name);
    if (it == extra_.end())
        return std::nullopt;
    const auto& vec = it->second.as_array();
    return static_cast<unsigned long>(vec.size());
}

void ExtraData::AddMapLoot(std::string name, json::value v) {
    // move-assign or insert
    extra_.insert_or_assign(std::move(name), std::move(v));
}

bool ExtraData::Contains(const std::string& name) const {
    return extra_.contains(name);
}

std::size_t ExtraData::Size() const noexcept {
    return extra_.size();
}

int ExtraData::GetLootValue(const std::string& map_id, size_t type_idx) const {
    auto it = extra_.find(map_id);
    if (it == extra_.end()) {
        return 0;
    }

    const auto& loot_types = it->second.as_array();
    if (type_idx >= loot_types.size()) {
        return 0;
    }

    const auto& loot_obj = loot_types[type_idx].as_object();
    if (loot_obj.contains("value")) {
        return static_cast<int>(loot_obj.at("value").as_int64());
    }
    return 0;
}

}  // namespace extra_data
