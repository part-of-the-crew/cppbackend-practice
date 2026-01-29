#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace extra_data {
namespace json = boost::json;

class ExtraData {
public:
    // default constructs an empty object {}
    ExtraData() = default;

    std::string GetMapValue(const std::string& name) const;
    std::optional<unsigned long> GetNumberLootforMap(const std::string& name) const;
    void AddMapLoot(std::string name, json::value);

    bool Contains(const std::string& name) const;
    std::size_t Size() const noexcept;
    int GetLootValue(const std::string& map_id, size_t type_idx) const;

private:
    std::unordered_map<std::string, json::value> extra_;
};

}  // namespace extra_data
