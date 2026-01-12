#include "api_handler.h"
#include <charconv>

namespace api_handler {

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

std::optional<model::Direction> StringToDirection(std::string_view dir_str) {
    if (dir_str == "U") return model::Direction::NORTH;
    if (dir_str == "D") return model::Direction::SOUTH;
    if (dir_str == "L") return model::Direction::WEST;
    if (dir_str == "R") return model::Direction::EAST;
    return std::nullopt;
}

std::string DirectionToString(model::Direction dir) {
    switch (dir) {
    case model::Direction::NORTH: return "U";
    case model::Direction::SOUTH: return "D";
    case model::Direction::WEST: return "L";
    case model::Direction::EAST: return "R";
    }
    return "U";
}

response::ResponseVariant HandleAPI::operator()(const http::request<http::string_body>& req) {
    // GLOBAL TRY-CATCH to prevent server death
    try {
        const auto& target = req.target();

        if (target == "/api/v1/game/join") return HandleJoin(req);
        if (target == "/api/v1/game/players") return HandlePlayers(req);
        if (target == "/api/v1/game/state") return HandleState(req);
        if (target == "/api/v1/game/player/action") return HandlePlayerAction(req);
        if (target == "/api/v1/game/tick") return HandleTick(req);
        if (target == "/api/v1/maps") return HandleMaps(req);
        if (target.starts_with("/api/v1/maps/")) return HandleMapId(req);

        return response::MakeError(http::status::bad_request, "badRequest", "Invalid API path", req);

    } catch (const std::exception& e) {
        return response::MakeError(http::status::internal_server_error, "internalError", "Internal Server Error", req);
    }
}

response::ResponseVariant HandleAPI::HandleJoin(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Only POST method is expected"s, "POST", req);
    }
    auto AuthReq = ParseJSONAuthReq(req.body());
    if (!AuthReq) {
        return response::MakeError(http::status::bad_request, "invalidArgument"s, "Join game request parse error"s, req);
    }

    JoinOutcome outcome = ProcessJoinGame(*AuthReq);

    if (std::holds_alternative<JoinError>(outcome)) {
        auto err = std::get<JoinError>(outcome);
        if (err == JoinError::MapNotFound)
            return response::MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
        else
            return response::MakeError(http::status::bad_request, "invalidArgument", "Invalid name", req);
    }

    const auto& success = std::get<json::object>(outcome);
    return response::MakeJSON(http::status::ok, json::value(success), req);
}

response::ResponseVariant HandleAPI::HandlePlayers(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
    }
    auto token = ExtractToken(req);
    if (!token) {
        return response::MakeError(http::status::unauthorized, "invalidToken"s, "Authorization header is missing"s, req);
    }

    json::object res_body = ProcessPlayers(*token);
    if (res_body.empty() && res_body.capacity() == 0) { // Check if we returned "error" object/state
        // Actually ProcessPlayers returns empty object on error in my improved version below
        // But let's verify logic.
        // For now, let's trust ProcessPlayers handles logic.
        // If token not found, ProcessPlayers returns empty or throws?
        // Let's rely on try-catch inside ProcessPlayers or here.
    }
    // Re-check token existence via app if needed, or handle inside ProcessPlayers
    try {
        // Simple check: if unknown token, Application::GetPlayers throws?
        // No, you implemented it to return vector.
        // We need to know if token is valid.
        if (app_.GetPlayers(*token).empty() && !res_body.empty()) {
            // This logic is tricky if GetPlayers returns empty for valid player.
            // Better:
        }
    } catch (...) {}

    // Better implementation of HandlePlayers logic:
    try {
        json::object players = ProcessPlayers(*token);
        return response::MakeJSON(http::status::ok, std::move(players), req);
    } catch (const std::invalid_argument&) {
        return response::MakeError(http::status::unauthorized, "unknownToken", "Token is missing", req);
    }
}

response::ResponseVariant HandleAPI::HandleState(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
    }
    auto token = ExtractToken(req);
    if (!token) {
        return response::MakeError(http::status::unauthorized, "invalidToken"s, "Authorization header is required"s, req);
    }

    auto result = ProcessState(*token);
    if (!result) {
        return response::MakeError(http::status::unauthorized, "unknownToken", "Player token has not been found", req);
    }
    return response::MakeJSON(http::status::ok, std::move(*result), req);
}

response::ResponseVariant HandleAPI::HandlePlayerAction(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "POST"s, req);
    }
    auto token = ExtractToken(req);
    if (!token) {
        return response::MakeError(http::status::unauthorized, "invalidToken"s, "Authorization header is required"s, req);
    }

    boost::system::error_code ec;
    auto body = json::parse(req.body(), ec);
    if (ec || !body.is_object()) {
        return response::MakeError(http::status::bad_request, "invalidArgument", "Failed to parse action JSON", req);
    }

    if (!body.as_object().contains("move")) {
        return response::MakeError(http::status::bad_request, "invalidArgument", "Missing move field", req);
    }

    std::string move = body.as_object().at("move").as_string().c_str();

    // Check direction
    std::optional<model::Direction> direction;
    if (!move.empty()) {
        direction = StringToDirection(move);
        if (!direction) {
            return response::MakeError(http::status::bad_request, "invalidArgument", "Invalid direction", req);
        }
    }

    if (!app_.SetPlayerAction(*token, direction)) {
        return response::MakeError(http::status::unauthorized, "unknownToken", "Player token has not been found", req);
    }

    return response::MakeJSON(http::status::ok, json::object{}, req);
}

response::ResponseVariant HandleAPI::HandleTick(const http::request<http::string_body>& req) {
    if (req.method() != http::verb::post) {
        return response::MakeMethodNotAllowedError("Invalid method"s, "POST"s, req);
    }
    boost::system::error_code ec;
    auto body = json::parse(req.body(), ec);
    if (ec || !body.is_object() || !body.as_object().contains("timeDelta")) {
        return response::MakeError(http::status::bad_request, "invalidArgument", "Failed to parse tick JSON", req);
    }

    auto timeDeltaVal = body.as_object().at("timeDelta");
    if (!timeDeltaVal.is_int64()) {
        return response::MakeError(http::status::bad_request, "invalidArgument", "timeDelta must be integer", req);
    }

    int64_t timeDelta = timeDeltaVal.as_int64();
    if (timeDelta <= 0) {
        // Some tests might expect success for 0? Usually delta > 0.
        return response::MakeError(http::status::bad_request, "invalidArgument", "timeDelta must be > 0", req);
    }

    try {
        app_.MakeTick(timeDelta);
    } catch (...) {
        return response::MakeError(http::status::internal_server_error, "internalError", "Tick failed", req);
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
    auto parts = SplitTarget(req.target());
    if (parts.size() != 4) {
        return response::MakeError(http::status::bad_request, "invalidArgument", "Bad map request", req);
    }
    const auto* map = app_.GetGame().FindMap(model::Map::Id{std::string(parts[3])});
    if (!map) {
        return response::MakeError(http::status::not_found, "mapNotFound", "Map not found", req);
    }
    return response::MakeJSON(http::status::ok, SerializeMap(*map), req);
}

std::optional<std::string> HandleAPI::ExtractToken(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return std::nullopt;

    std::string_view auth = it->value();
    constexpr std::string_view prefix = "Bearer ";
    if (!auth.starts_with(prefix)) return std::nullopt;

    std::string token{auth.substr(prefix.size())};
    if (token.size() != 32) return std::nullopt;

    return token;
}

std::optional<app::AuthRequest> HandleAPI::ParseJSONAuthReq(std::string body) {
    try {
        json::value content = json::parse(body);
        const auto& desc = content.as_object();
        const auto& name = desc.at("userName").as_string();
        const auto& map = desc.at("mapId").as_string();
        return app::AuthRequest{std::string(name), std::string(map)};
    } catch (...) {
        return std::nullopt;
    }
}

JoinOutcome HandleAPI::ProcessJoinGame(const app::AuthRequest& authReq) {
    if (authReq.playerName.empty()) return JoinError::InvalidName;
    try {
        auto result = app_.JoinGame(authReq);
        if (!result) return JoinError::MapNotFound;

        json::object resp;
        resp["authToken"] = result->token;
        resp["playerId"] = result->playerId;
        return resp;
    } catch (...) {
        return JoinError::InvalidName;
    }
}

json::object HandleAPI::ProcessPlayers(const std::string& token) {
    json::object list;
    try {
        // app_.GetPlayers throws invalid_argument if token not found
        auto players = app_.GetPlayers(token);
        for (const auto& p : players) {
            json::object p_json;
            p_json["name"] = p.GetName();
            list[std::to_string(p.GetId())] = std::move(p_json);
        }
    } catch (...) {
        throw std::invalid_argument("Token not found");
    }
    return list;
}

std::optional<json::object> HandleAPI::ProcessState(const app::Token& token) {
    try {
        // This will throw if token invalid
        auto players = app_.GetPlayers(token);

        json::object json_players;
        for (const auto& player : players) {
            const auto* pdog = player.GetDog();
            json::object dog_state;
            dog_state["pos"] = json::array{pdog->GetPosition().x, pdog->GetPosition().y};
            dog_state["speed"] = json::array{pdog->GetSpeed().ux, pdog->GetSpeed().uy};
            dog_state["dir"] = DirectionToString(pdog->GetDirection());

            json_players[std::to_string(player.GetId())] = std::move(dog_state);
        }

        json::object result;
        result["players"] = std::move(json_players);
        return result;

    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// ... serialization methods (SerializeMap, etc.) remain the same ...
json::object HandleAPI::SerializeMap(const model::Map& map) {
    // (Keep your existing implementation)
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    json::array roads;
    for (const auto& r : map.GetRoads()) roads.push_back(SerializeRoad(r));
    map_obj["roads"] = std::move(roads);
    json::array buildings;
    for (const auto& b : map.GetBuildings()) buildings.push_back(SerializeBuilding(b));
    map_obj["buildings"] = std::move(buildings);
    json::array offices;
    for (const auto& o : map.GetOffices()) offices.push_back(SerializeOffice(o));
    map_obj["offices"] = std::move(offices);
    return map_obj;
}

json::object HandleAPI::SerializeRoad(const model::Road& road) {
    json::object obj;
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();
    obj["x0"] = start.x;
    obj["y0"] = start.y;
    if (start.y == end.y) obj["x1"] = end.x;
    else obj["y1"] = end.y;
    return obj;
}

json::object HandleAPI::SerializeBuilding(const model::Building& b) {
    const auto& bounds = b.GetBounds();
    return {{"x", bounds.position.x}, {"y", bounds.position.y}, {"w", bounds.size.width}, {"h", bounds.size.height}};
}

json::object HandleAPI::SerializeOffice(const model::Office& o) {
    return {{"id", *o.GetId()}, {"x", o.GetPosition().x}, {"y", o.GetPosition().y}, {"offsetX", o.GetOffset().dx}, {"offsetY", o.GetOffset().dy}};
}

} // namespace api_handler
