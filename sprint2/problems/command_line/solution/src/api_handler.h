#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "app.h"
#include "responses.h"

namespace api_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
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

    response::ResponseVariant operator()(const http::request<http::string_body>& req);

private:
    response::ResponseVariant HandleMaps(const http::request<http::string_body>& req);
    response::ResponseVariant HandleMapId(const http::request<http::string_body>& req);
    response::ResponseVariant HandleJoin(const http::request<http::string_body>& req);
    response::ResponseVariant HandleState(const http::request<http::string_body>& req);
    response::ResponseVariant HandlePlayers(const http::request<http::string_body>& req);
    response::ResponseVariant HandlePlayerAction(const http::request<http::string_body>& req);
    response::ResponseVariant HandleTick(const http::request<http::string_body>& req);
    std::optional<std::string> ExtractToken(const http::request<http::string_body>& req);
    app::Application& app_;

    std::optional<app::AuthRequest> ParseJSONAuthReq(std::string body);

    JoinOutcome ProcessJoinGame(const app::AuthRequest& params);
    std::string ProcessPlayers(const std::string& token);
    std::optional<std::string> ProcessState(const app::Token& token);
    // std::optional<std::string> ProcessPlayerAction(const app::Token& token);

    boost::json::object SerializeMap(const model::Map& map);
    boost::json::object SerializeRoad(const model::Road& road);
    boost::json::object SerializeBuilding(const model::Building& b);
    boost::json::object SerializeOffice(const model::Office& o);
};

}  // namespace api_handler
