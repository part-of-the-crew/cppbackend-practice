#include "extra_data_serialization.h"

#include <boost/json.hpp>

namespace extra_data_ser {

namespace json = boost::json;

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

        std::string id = json::value_to<std::string>(map_obj.at("id"));

        if (auto it_loot = map_obj.find("lootTypes"); it_loot != map_obj.end()) {
            result.AddMapLoot(std::move(id), it_loot->value());
        }
    }
    return result;
}
}  // namespace extra_data_ser