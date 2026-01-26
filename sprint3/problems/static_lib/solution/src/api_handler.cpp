#include "api_handler.h"

#include <boost/json/array.hpp>
#include <string>

namespace api_handler {

struct APItype {
    APItype() = delete;
    constexpr static std::string_view V1_GAME_JOIN = "/api/v1/game/join"sv;
    constexpr static std::string_view V1_GAME_PLAYERS = "/api/v1/game/players"sv;
    constexpr static std::string_view V1_GAME_STATE = "/api/v1/game/state"sv;
    constexpr static std::string_view V1_GAME_PLAYER_ACTION = "/api/v1/game/player/action"sv;
    constexpr static std::string_view V1_GAME_TICK = "/api/v1/game/tick"sv;
    constexpr static std::string_view V1_MAPS = "/api/v1/maps"sv;
};

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
std::optional<model::Direction> StringToDirection(std::string_view dir_str) {
    if (dir_str == "U")
        return model::Direction::NORTH;
    if (dir_str == "D")
        return model::Direction::SOUTH;
    if (dir_str == "L")
        return model::Direction::WEST;
    if (dir_str == "R")
        return model::Direction::EAST;
    return std::nullopt;  // Возвращаем пустое значение, если символ не распознан
}

response::ResponseVariant HandleAPI::operator()(const http::request<http::string_body>& req) {
    const auto& target = req.target();

    if (target == APItype::V1_GAME_JOIN) {
        return HandleJoin(req);
    }
    if (target == APItype::V1_GAME_PLAYERS) {
        return HandlePlayers(req);
    }
    if (target == APItype::V1_GAME_STATE) {
        return HandleState(req);
    }
    if (target == APItype::V1_GAME_PLAYER_ACTION) {
        return HandlePlayerAction(req);
    }
    if (target == APItype::V1_GAME_TICK) {
        return HandleTick(req);
    }
    if (target == APItype::V1_MAPS) {
        return HandleMaps(req);
    }
    if (target.starts_with(APItype::V1_MAPS)) {
        return HandleMapId(req);
    }
    return response::MakeError(http::status::conflict, "badRequest", "Invalid API path", req);
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
        if (err == JoinError::MapNotFound) {
            return response::MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
        }
        return response::MakeError(http::status::bad_request, "invalidArgument", "Invalid name", req);
    }
    return response::MakeJSON(http::status::ok, std::get<json::object>(outcome), req);
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
        return response::MakeError(
            http::status::unauthorized, "unknownToken", "Player token has not been found", req);
    }
    try {
        return response::MakeJSON(http::status::ok, json::parse(*result), req);
    } catch (const std::invalid_argument&) {
        return response::MakeError(http::status::internal_server_error, "ParseError", "Parse error", req);
    }
}

response::ResponseVariant HandleAPI::HandlePlayerAction(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "POST"s, req);
    }
    auto token = ExtractToken(req);
    if (!token)
        return response::MakeError(
            http::status::unauthorized, "invalidToken"s, "Authorization header is required"s, req);

    std::string move;
    json::value body;
    try {
        body = json::parse(req.body());
    } catch (const std::invalid_argument& ex) {
        return response::MakeError(http::status::internal_server_error, "ParseError", "Parse error", req);
    }

    try {
        if (!body.as_object().contains("move")) {
            return response::MakeError(
                http::status::bad_request, "invalidArgument", "Missing move field", req);
        }

        move = body.as_object().at("move").as_string().c_str();

        // Если "move" пустая строка, значит игрок остановился (скорость 0)
        if (move.empty()) {
            // Логика остановки
            // app_.SetPlayerAction(token, std::nullopt);
            return response::MakeJSON(http::status::ok, json::object{}, req);
        }
    } catch (const std::invalid_argument& ex) {
        return response::MakeError(http::status::bad_request, "invalidArgument", ex.what(), req);
    }
    // 3. Конвертация строки в Direction
    auto direction = StringToDirection(move);
    if (!direction) {
        return response::MakeError(
            http::status::bad_request, "invalidArgument"sv, "Invalid direction"sv, req);
    }
    try {
        if (!app_.SetPlayerAction(*token, direction)) {
            return response::MakeError(
                http::status::unauthorized, "unknownToken"sv, "Player token has not been found"sv, req);
        }
    } catch (const std::invalid_argument&) {
        return response::MakeError(http::status::bad_gateway, "SetPlayerAction", "", req);
    }

    return response::MakeJSON(http::status::ok, json::object{}, req);
}

response::ResponseVariant HandleAPI::HandleTick(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "POST"s, req);
    }
    boost::system::error_code ec;
    auto body = json::parse(req.body(), ec);
    if (ec.failed() || !body.is_object() || !body.as_object().contains("timeDelta")) {
        return response::MakeError(
            http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON"sv, req);
    }

    auto timeDelta = body.as_object().at("timeDelta");

    if (!timeDelta.is_int64() || timeDelta.as_int64() <= 0) {
        return response::MakeError(
            http::status::bad_request, "invalidArgument"sv, "Failed to parse tick request JSON"sv, req);
    }
    try {
        app_.MakeTick(timeDelta.as_int64());
    } catch (const std::invalid_argument&) {
        return response::MakeError(http::status::bad_gateway, "MakeTick", "", req);
    }
    return response::MakeJSON(http::status::ok, json::object{}, req);
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

response::ResponseVariant HandleAPI::HandleMapId(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
    }
    auto parts = SplitTarget(req.target());
    if (parts.size() != 4) {
        return response::MakeError(http::status::bad_request, "invalidArgument"sv, "invalidArgument"sv, req);
    }

    const auto* map = app_.GetGame().FindMap(model::Map::Id{std::string(parts[3])});
    if (!map) {
        return response::MakeError(http::status::not_found, "mapNotFound", "map Not Found", req);
    }
    return response::MakeJSON(http::status::ok, SerializeMap(*map), req);
}

json::object HandleAPI::SerializeLootInMap(const app::Player& player) const {
    auto loots = app_.GetLootInMap(*(player.GetSession()->GetMap()->GetId()));
    json::object result;
    int n = 0;
    for (const auto& loot : loots) {
        json::object json_map;
        json_map["type"] = loot.type;
        json_map["pos"] = {loot.pos.x, loot.pos.y};
        result[std::to_string(n)] = std::move(json_map);
        n++;
    }
    return result;
}

std::optional<std::string> HandleAPI::ProcessState(const app::Token& token) {
    std::vector<app::Player> players;
    try {
        players = app_.GetPlayers(token);
    } catch (const std::exception& ex) {
        return std::nullopt;
    }

    json::object json_players;
    json::object json_lostObjects;
    json::object result;
    try {
        // 2. Итерируемся по игрокам
        for (const auto& player : players) {
            // У игрока есть доступ к его собаке
            const auto& pdog = player.GetDog();

            json::object dog_state;

            // Позиция: [x, y]
            dog_state["pos"] = {pdog->GetPosition().x, pdog->GetPosition().y};

            // Скорость: [vx, vy]
            dog_state["speed"] = {pdog->GetSpeed().ux, pdog->GetSpeed().uy};

            // Направление: "U", "D", "L", "R"
            dog_state["dir"] = DirectionToString(pdog->GetDirection());

            // Ключ — это ID игрока (строка)
            json_players[std::to_string(player.GetId())] = std::move(dog_state);

            result["lostObjects"] = SerializeLootInMap(player);
        }
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }

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

    map_obj.emplace("lootTypes", SerializeLoots(app_.GetMapValue(*map.GetId())));
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
    return {{"x", bounds.position.x}, {"y", bounds.position.y}, {"w", bounds.size.width},
        {"h", bounds.size.height}};
}

json::object HandleAPI::SerializeOffice(const model::Office& o) {
    return {{"id", *o.GetId()}, {"x", o.GetPosition().x}, {"y", o.GetPosition().y},
        {"offsetX", o.GetOffset().dx}, {"offsetY", o.GetOffset().dy}};
}

json::array HandleAPI::SerializeLoots(const std::string& loot) {
    try {
        return json::array(json::parse(loot).as_array());
    } catch (const json::system_error& e) {
        throw std::runtime_error("ParseJsonText: JSON parse error: "s + e.what());
    }
}

}  // namespace api_handler
