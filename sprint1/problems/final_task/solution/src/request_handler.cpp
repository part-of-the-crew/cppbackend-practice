#include "request_handler.h"

namespace http_handler {

namespace json = boost::json;

std::string RequestHandler::HandleMaps() {
    json::array json_maps;
    for (const auto& map : game_.GetMaps()) {
        json::object json_map;
        json_map["id"] = *map.GetId();
        json_map["name"] = map.GetName();
        json_maps.emplace_back(std::move(json_map));
    }
    return json::serialize(json_maps);
}

std::pair<std::string, bool> RequestHandler::HandleMapId(std::string_view name_map) {
    const auto* map = game_.FindMap(model::Map::Id{std::string(name_map)});
    if (!map) {
        return {HandleErrorRequest("mapNotFound", "Map not found"), true};
    }
    return {json::serialize(SerializeMap(*map)), false};
}

std::string RequestHandler::HandleErrorRequest(std::string_view code, std::string_view message) {
    json::object error = {{"code", code}, {"message", message}};
    return json::serialize(error);
}

json::object RequestHandler::SerializeMap(const model::Map& map) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();

    json::array roads;
    for (const auto& r : map.GetRoads())
        roads.push_back(SerializeRoad(r));
    map_obj["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& b : map.GetBuildings())
        buildings.push_back(SerializeBuilding(b));
    map_obj["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& o : map.GetOffices())
        offices.push_back(SerializeOffice(o));
    map_obj["offices"] = std::move(offices);

    return map_obj;
}

json::object RequestHandler::SerializeRoad(const model::Road& road) {
    json::object obj;
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();
    obj["x0"] = start.x;
    obj["y0"] = start.y;
    if (start.y == end.y)
        obj["x1"] = end.x;
    else
        obj["y1"] = end.y;
    return obj;
}

json::object RequestHandler::SerializeBuilding(const model::Building& b) {
    const auto& bounds = b.GetBounds();
    return {{"x", bounds.position.x}, {"y", bounds.position.y}, {"w", bounds.size.width}, {"h", bounds.size.height}};
}

json::object RequestHandler::SerializeOffice(const model::Office& o) {
    return {{"id", *o.GetId()},
            {"x", o.GetPosition().x},
            {"y", o.GetPosition().y},
            {"offsetX", o.GetOffset().dx},
            {"offsetY", o.GetOffset().dy}};
}

}  // namespace http_handler