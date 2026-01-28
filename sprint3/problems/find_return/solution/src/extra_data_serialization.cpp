#include "extra_data_serialization.h"

#include <boost/json.hpp>  // Dependency is contained here

namespace extra_data_ser {

namespace json = boost::json;

/*
static extra_data::LootType ParseLootType(const json::value& v) {
    if (!v.is_object()) {
        throw std::invalid_argument("extra_data::LootType must be an object");
    }

    const auto& obj = v.as_object();

    extra_data::LootType lt;

    // Required
    lt.name = json::value_to<std::string>(obj.at("name"));
    lt.file = json::value_to<std::string>(obj.at("file"));
    lt.type = json::value_to<std::string>(obj.at("type"));

    // Optional
    if (auto it = obj.find("rotation"); it != obj.end()) {
        lt.rotation = json::value_to<int>(it->value());
    }
    if (auto it = obj.find("color"); it != obj.end()) {
        lt.color = json::value_to<std::string>(it->value());
    }
    if (auto it = obj.find("scale"); it != obj.end()) {
        lt.scale = json::value_to<double>(it->value());
    }

    return lt;
}

static std::vector<extra_data::LootType> ParseLootArray(const json::value& v) {
    if (!v.is_array()) {
        throw std::invalid_argument("lootTypes must be an array");
    }

    std::vector<extra_data::LootType> res;
    for (const auto& item : v.as_array()) {
        res.push_back(ParseLootType(item));
    }
    return res;
}
*/
extra_data::ExtraData ExtractExtraData(const json::value& root) {
    if (!root.is_object()) {
        throw std::invalid_argument("Root JSON must be an object");
    }

    const auto& root_obj = root.as_object();
    extra_data::ExtraData result;

    auto it_maps = root_obj.find("maps");
    if (it_maps == root_obj.end()) {
        return result;
    }

    if (!it_maps->value().is_array()) {
        throw std::invalid_argument("'maps' must be an array");
    }

    for (const auto& map_val : it_maps->value().as_array()) {
        if (!map_val.is_object()) {
            throw std::invalid_argument("Map entry must be an object");
        }
        const auto& map_obj = map_val.as_object();

        // Safe extraction
        std::string id = json::value_to<std::string>(map_obj.at("id"));

        // std::vector<extra_data::LootType> loot;
        if (auto it_loot = map_obj.find("lootTypes"); it_loot != map_obj.end()) {
            // Assuming ParseLootArray exists and works on json::value
            // loot = ParseLootArray(it_loot->value());
            result.AddMapLoot(std::move(id), it_loot->value());
        }
    }
    return result;
}
}  // namespace extra_data_ser