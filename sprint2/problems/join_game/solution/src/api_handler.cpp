#include "api_handler.h"

#include <charconv>  // For std::from_chars

namespace api_handler {

// Helper remains inline because it's simple and used by the template operator()
std::vector<std::string_view> SplitTarget(std::string_view target) {
    std::vector<std::string_view> result;
    while (!target.empty()) {
        if (target.front() == '/') {
            target.remove_prefix(1);
            continue;
        }
        auto pos = target.find('/');
        result.push_back(target.substr(0, pos));
        if (pos == std::string_view::npos)
            break;
        target.remove_prefix(pos + 1);
    }
    return result;
}

std::optional<app::AuthRequest> HandleAPI::ParseJSONAuthReq(std::string body) {
    // json::value content;
    try {
        json::value content = json::parse(body);
        const auto& desc = content.as_object();
        const auto& name = desc.at("userName").as_string();
        const auto& map = desc.at("mapId").as_string();
        return app::AuthRequest{std::string(name), std::string(map)};
    } catch (const std::exception& ex) {
        return std::nullopt;
    }
}

JoinOutcome HandleAPI::ProcessJoinGame(const app::AuthRequest& authReq) {
    if (authReq.playerName.empty())
        return JoinError::InvalidName;
    auto result = app_.JoinGame(authReq);
    if (!result)
        return JoinError::MapNotFound;

    json::object resp;
    resp["authToken"] = result->token;
    resp["playerId"] = result->playerId;
    return resp;
}

/*
std::string HandleAPI::ProcessJoinGame(const std::string& body) {
    json::value json_val = json::parse(body);
    std::string userName{json_val.at("userName").as_string()};
    std::string mapId{json_val.at("mapId").as_string()};

    // Call Application Facade
    // This is the key change: No direct model access
    auto result = app_.JoinGame(mapId, userName);

    json::object resp;
    resp["authToken"] = result.token;
    resp["playerId"] = result.playerId;
    return json::serialize(resp);
}
*/
std::string HandleAPI::ProcessPlayers(const std::string& token) {
    // Call Application Facade
    auto players = app_.GetPlayers(token);

    json::object list;
    for (const auto& p : players) {
        json::object p_json;
        p_json["name"] = p.GetName();
        list[std::to_string(p.GetId())] = p_json;
    }

    // According to spec, it might be an object {"id": {"name":"..."}, ...}
    return json::serialize(list);
}

json::value HandleAPI::HandleMaps() {
    json::array json_maps;
    for (const auto& map : app_.GetGame().GetMaps()) {
        json::object json_map;
        json_map["id"] = *map.GetId();
        json_map["name"] = map.GetName();
        json_maps.emplace_back(std::move(json_map));
    }
    return json_maps;
}

std::pair<json::value, bool> HandleAPI::HandleMapId(std::string_view name_map) {
    const auto* map = app_.GetGame().FindMap(model::Map::Id{std::string(name_map)});
    if (!map) {
        return {json::value{}, true};
    }
    return {SerializeMap(*map), false};
}

json::object HandleAPI::SerializeMap(const model::Map& map) {
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

json::object HandleAPI::SerializeRoad(const model::Road& road) {
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

json::object HandleAPI::SerializeBuilding(const model::Building& b) {
    const auto& bounds = b.GetBounds();
    return {{"x", bounds.position.x}, {"y", bounds.position.y}, {"w", bounds.size.width}, {"h", bounds.size.height}};
}

json::object HandleAPI::SerializeOffice(const model::Office& o) {
    return {{"id", *o.GetId()}, {"x", o.GetPosition().x}, {"y", o.GetPosition().y}, {"offsetX", o.GetOffset().dx},
        {"offsetY", o.GetOffset().dy}};
}

}  // namespace api_handler
