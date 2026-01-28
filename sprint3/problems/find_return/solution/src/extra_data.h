#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace extra_data {
namespace json = boost::json;

struct LootType {
    std::string name;
    std::string file;
    std::string type;

    int rotation = 0;  // degrees
    std::string color = "#000000";
    double scale = 1.0;
    // Add this so REQUIRE(vector == vector) works
    bool operator==(const LootType&) const = default;
};

class ExtraData {
public:
    // default constructs an empty object {}
    ExtraData() = default;

    std::string GetMapValue(const std::string& name) const;
    std::optional<unsigned long> GetNumberLootforMap(const std::string& name) const;
    void AddMapLoot(std::string name, json::value);

    bool Contains(const std::string& name) const;
    std::size_t Size() const noexcept;

private:
    std::unordered_map<std::string, json::value> extra_;
};

}  // namespace extra_data
