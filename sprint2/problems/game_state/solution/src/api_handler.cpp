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

response::ResponseVariant HandleAPI::operator()(const http::request<http::string_body>& req) {
    const auto& target = req.target();
    if (target == "/api/v1/game/join") {
        return HandleJoin(req);
    }
    if (target == "/api/v1/game/players") {
        return HandlePlayers(req);
    }
    if (target == "/api/v1/game/state") {
        return HandleState(req);
    }
    if (target == "/api/v1/maps") {
        return HandleMaps(req);
    }

    if (target.starts_with("/api/v1/maps/")) {
        auto parts = SplitTarget(target);
        if (parts.size() == 4) {
            auto [response_body, error] = HandleMapId(parts[3]);
            if (error) {
                return response::MakeError(http::status::not_found, "mapNotFound", "map Not Found", req);
            }
            return response::MakeJSON(http::status::ok, std::move(response_body), req);
        }
    }
    return response::MakeError(http::status::bad_request, "badRequest", "Invalid API path", req);
}

response::ResponseVariant HandleAPI::HandleJoin(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Only POST method is expected"s, "POST", req);
    }
    auto AuthReq = ParseJSONAuthReq(req.body());
    if (!AuthReq)
        return response::MakeError(
            http::status::bad_request, "invalidArgument"s, "Join game request parse error"s, req);
    JoinOutcome outcome = ProcessJoinGame(*AuthReq);
    if (std::holds_alternative<JoinError>(outcome)) {
        auto err = std::get<JoinError>(outcome);
        if (err == JoinError::MapNotFound)
            return response::MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
        else
            return response::MakeError(http::status::bad_request, "invalidArgument", "Invalid name", req);
    }
    const auto& success = std::get<json::object>(outcome);
    return response::MakeJSON(http::status::ok, std::move(success), req);
}

response::ResponseVariant HandleAPI::HandlePlayers(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
    }
    auto token = ExtractToken(req);
    if (!token)
        return response::MakeError(
            http::status::unauthorized, "invalidToken"s, "Authorization header is missing"s, req);
    try {
        std::string res_body = ProcessPlayers(*token);
        return response::MakeJSON(http::status::ok, json::parse(res_body), req);
    } catch (const std::invalid_argument&) {
        return response::MakeError(http::status::unauthorized, "unknownToken", "Token is missing", req);
    }
}

response::ResponseVariant HandleAPI::HandleState(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
    }
    auto token = ExtractToken(req);
    if (!token)
        return response::MakeError(
            http::status::unauthorized, "invalidToken"s, "Authorization header is required"s, req);

    auto result = ProcessState(*token);
    if (!result) {
        return response::MakeError(http::status::unauthorized, "unknownToken", "Player token has not been found", req);
    }
    return response::MakeJSON(http::status::ok, json::parse(*result), req);
}

response::ResponseVariant HandleAPI::HandleMaps(const http::request<http::string_body>& req) {
    json::array json_maps;
    for (const auto& map : app_.GetGame().GetMaps()) {
        json::object json_map;
        json_map["id"] = *map.GetId();
        json_map["name"] = map.GetName();
        json_maps.emplace_back(std::move(json_map));
    }
    return response::MakeJSON(http::status::ok, std::move(json_maps), req);
}

std::string DirectionToString(model::Direction dir) {
    switch (dir) {
        case model::Direction::NORTH:
            return "U";
        case model::Direction::SOUTH:
            return "D";
        case model::Direction::WEST:
            return "L";
        case model::Direction::EAST:
            return "R";
        default:
            return "U";
    }
}

std::optional<std::string> HandleAPI::ProcessState(std::string token) {
    std::vector<app::Player> players;
    try {
        players = app_.GetPlayers(token);
    } catch (const std::exception& ex) {
        return std::nullopt;
    }

    json::object json_players;

    // 2. Итерируемся по игрокам
    for (const auto& player : players) {
        // У игрока есть доступ к его собаке
        const auto& pdog = player.GetDog();  // Убедитесь, что в классе Player есть GetDog()

        json::object dog_state;

        // Позиция: [x, y]
        dog_state["pos"] = {pdog->GetPosition().x, pdog->GetPosition().y};

        // Скорость: [vx, vy]
        dog_state["speed"] = {pdog->GetSpeed().ux, pdog->GetSpeed().uy};

        // Направление: "U", "D", "L", "R"
        dog_state["dir"] = DirectionToString(pdog->GetDirection());

        // Ключ — это ID игрока (строка)
        json_players[std::to_string(player.GetId())] = std::move(dog_state);
    }

    json::object result;
    result["players"] = std::move(json_players);

    return json::serialize(result);
}

std::optional<std::string> HandleAPI::ExtractToken(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return std::nullopt;
    }
    auto auth = it->value();
    constexpr std::string_view prefix = "Bearer ";
    if (!auth.starts_with(prefix)) {
        return std::nullopt;
    }
    std::string token{auth.substr(prefix.size())};
    if (token.size() != 32u) {
        return std::nullopt;
    }
    return token;
}

std::optional<app::AuthRequest> HandleAPI::ParseJSONAuthReq(std::string body) {
    if (body.empty()) {
        return std::nullopt;
    }
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

std::string HandleAPI::ProcessPlayers(const std::string& token) {
    // Call Application Facade
    auto players = app_.GetPlayers(token);

    json::object list;
    for (const auto& p : players) {
        json::object p_json;
        p_json["name"] = p.GetName();
        list[std::to_string(p.GetId())] = p_json;
    }

    return json::serialize(list);
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
