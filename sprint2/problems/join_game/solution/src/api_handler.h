#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <algorithm>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>  // Required for std::tie
#include <variant>
#include <vector>

#include "app.h"
// #include "http_server.h"
#include "responses.h"

namespace api_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

// 2. Logic Result (Success or specific Error)
enum class JoinError { None, InvalidName, MapNotFound, JsonParseError };

// Helper remains inline because it's simple and used by the template operator()
std::vector<std::string_view> SplitTarget(std::string_view target);

using JoinOutcome = std::variant<json::object, JoinError>;

class HandleAPI {
public:
    explicit HandleAPI(app::Application& app) : app_(app) {}

    response::ResponseVariant operator()(const http::request<http::string_body>& req) {
        const auto& target = req.target();

        if (target == "/api/v1/game/join") {
            return HandleJoin(req);
        }
        if (target == "/api/v1/game/players") {
            return HandlePlayers(req);
        }
        if (target == "/api/v1/maps") {
            return response::MakeJSON(http::status::ok, std::move(HandleMaps()), req);
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

private:
    response::ResponseVariant HandleJoin(const http::request<http::string_body>& req) {
        if (req.method() != http::verb::post) {
            return response::MakeMethodNotAllowedError("Only POST method is expected"s, "POST", req);
        }
        if (req.body().empty())
            return response::MakeError(
                http::status::bad_request, "invalidArgument"s, "Join game request parse error"s, req);
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

    response::ResponseVariant HandlePlayers(const http::request<http::string_body>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return response::MakeMethodNotAllowedError("Invalid method"s, "GET, HEAD"s, req);
        }
        auto it = req.find(http::field::authorization);
        if (it == req.end()) {
            return response::MakeError(
                http::status::unauthorized, "invalidToken"s, "Authorization header is missing"s, req);
        }
        auto auth = it->value();
        constexpr std::string_view prefix = "Bearer ";
        if (!auth.starts_with(prefix)) {
            return response::MakeError(
                http::status::unauthorized, "invalidToken"s, "Authorization header is missing"s, req);
        }
        std::string token{auth.substr(prefix.size())};
        if (token.empty()) {
            return response::MakeError(
                http::status::unauthorized, "invalidToken"s, "Authorization header is missing"s, req);
        }

        try {
            std::string res_body = ProcessPlayers(token);
            return response::MakeJSON(http::status::ok, json::parse(res_body), req);
        } catch (const std::invalid_argument&) {
            return response::MakeError(http::status::unauthorized, "unknownToken", "Token is missing", req);
        }
    }

    app::Application& app_;
    json::value HandleMaps();
    std::optional<app::AuthRequest> ParseJSONAuthReq(std::string body);

    JoinOutcome ProcessJoinGame(const app::AuthRequest& params);
    std::string ProcessPlayers(const std::string& token);

    std::pair<json::value, bool> HandleMapId(std::string_view name_map);
    boost::json::object SerializeMap(const model::Map& map);
    boost::json::object SerializeRoad(const model::Road& road);
    boost::json::object SerializeBuilding(const model::Building& b);
    boost::json::object SerializeOffice(const model::Office& o);
};

}  // namespace api_handler
