#pragma once

#include <tuple>  // Required for std::tie
// #include <boost/url.hpp>

#include "http_server.h"
#include "model.h"

#pragma once

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

// Helper remains inline because it's simple and used by the template operator()
inline std::vector<std::string_view> SplitTarget(std::string_view target) {
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

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
};

// Template functions MUST stay in the header
template <class Request>
http::response<http::string_body> MakeTextResponse(http::status status, std::string body, const Request& req,
                                                   std::string_view content_type = ContentType::TEXT_HTML) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game) : game_{game} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // This is a template, so it stays in the header
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::status status = http::status::ok;
        std::string body;
        bool is_error = false;

        if (req.method() == http::verb::get) {
            if (req.target() == "/api/v1/maps") {
                body = HandleMaps();
            } else if (req.target().starts_with("/api/v1/maps/")) {
                auto parts = SplitTarget(req.target());
                if (parts.size() == 4) {
                    auto [response_body, error] = HandleMapId(parts[3]);
                    body = std::move(response_body);
                    if (error)
                        status = http::status::not_found;
                }
            } else if (req.target().starts_with("/api")) {
                body = HandleErrorRequest("badRequest", "Bad Request");
                status = http::status::bad_request;
            }
        } else {
            status = http::status::method_not_allowed;
            body = "Invalid method"s;
        }

        send(MakeTextResponse(status, body, req, ContentType::APP_JSON));
    }

private:
    model::Game& game_;

    // These non-template methods move to the .cpp
    std::string HandleMaps();
    std::pair<std::string, bool> HandleMapId(std::string_view name_map);
    std::string HandleErrorRequest(std::string_view code, std::string_view message);

    boost::json::object SerializeMap(const model::Map& map);
    boost::json::object SerializeRoad(const model::Road& road);
    boost::json::object SerializeBuilding(const model::Building& b);
    boost::json::object SerializeOffice(const model::Office& o);
};

}  // namespace http_handler