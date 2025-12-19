#pragma once
#define BOOST_BEAST_USE_STD_STRING_VI

#include <boost/json.hpp>
#include <optional>
#include <tuple>  // Required for std::tie
// #include <boost/url.hpp>

#include "http_server.h"
#include "model.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

using namespace std::literals;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

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

template <class Request>
http::response<http::string_body> MakeTextResponse(http::status status, std::string body, const Request& req,
                                                   std::string_view content_type = ContentType::TEXT_HTML) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    // res.content_length(res.body().size());
    res.keep_alive(req.keep_alive());
    return res;
}

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game) : game_{game} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        /*
        An HTTP Request (Client to Server):
        POST /hello HTTP/1.1          <-- Start Line (Method, Path, Version)
        Host: localhost:8080          <-- Header
        Content-Type: text/plain      <-- Header
        Content-Length: 5             <-- Header
                                      <-- BLANK LINE
        World                         <-- BODY

        An HTTP Response (Server to Client):
        HTTP/1.1 200 OK               <-- Start Line (Version, Status Code, Phrase)
        Content-Type: text/html       <-- Header
        Content-Length: 13            <-- Header
                                      <-- BLANK LINE
        <h1>Hello</h1>                <-- BODY
        */
        http::status status{};
        std::string body{};
        // boost::urls::url_view u(req.target());
        // std::string_view map = u.fragment().at(3
        switch (req.method()) {
            case http::verb::get:
                status = http::status::ok;
                bool error;
                if (req.target() == "/api/v1/maps") {
                    body = HandleMaps();
                    break;
                }
                if (req.target().starts_with("/api/v1/maps/")) {
                    auto parts = SplitTarget(req.target());

                    if (parts.size() == 4) {
                        std::tie(body, error) = HandleMapId(parts[3]);
                        if (error)
                            status = http::status::not_found;
                    }
                    break;
                }
                if (req.target().starts_with("/api")) {
                    body = HandleBadRequest();
                    status = http::status::bad_request;
                    break;
                }
                // throw std::exception "Bad path";
                break;

            default:
                // Method not supported
                status = http::status::method_not_allowed;
                body = "Invalid method"s;
                break;
        }

        send(MakeTextResponse(status, body, req, ContentType::APP_JSON));
    }

private:
    model::Game& game_;

    std::string HandleMaps(void) {
        boost::json::array json_maps;
        for (const auto& map : game_.GetMaps()) {
            boost::json::object json_map;
            json_map["id"] = *map.GetId();
            json_map["name"] = map.GetName();
            json_maps.emplace_back(json_map);
        }
        return boost::json::serialize(json_maps);
    }
    std::pair<std::string, bool> HandleMapId(std::string_view name_map) {
        namespace json = boost::json;

        for (const auto& map : game_.GetMaps()) {
            if (!name_map.empty() && *map.GetId() != name_map) {
                continue;
            }

            json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();

            // ---- Roads ----
            json::array roads;
            for (const auto& road : map.GetRoads()) {
                json::object r;

                const auto& start = road.GetStart();
                const auto& end = road.GetEnd();

                r["x0"] = start.x;
                r["y0"] = start.y;

                if (start.y == end.y) {
                    // Horizontal
                    r["x1"] = end.x;
                } else {
                    // Vertical
                    r["y1"] = end.y;
                }

                roads.push_back(std::move(r));
            }
            map_obj["roads"] = std::move(roads);

            // ---- Buildings ----
            json::array buildings;
            for (const auto& b : map.GetBuildings()) {
                json::object bld;
                bld["x"] = b.GetBounds().position.x;
                bld["y"] = b.GetBounds().position.y;
                bld["w"] = b.GetBounds().size.width;
                bld["h"] = b.GetBounds().size.height;
                buildings.push_back(std::move(bld));
            }
            map_obj["buildings"] = std::move(buildings);

            // ---- Offices ----
            json::array offices;
            for (const auto& o : map.GetOffices()) {
                json::object off;
                off["id"] = *o.GetId();
                off["x"] = o.GetPosition().x;
                off["y"] = o.GetPosition().y;
                off["offsetX"] = o.GetOffset().dx;
                off["offsetY"] = o.GetOffset().dy;
                offices.push_back(std::move(off));
            }
            map_obj["offices"] = std::move(offices);

            // ---- Success ----
            return {json::serialize(map_obj), false};
        }

        json::object error;
        error["code"] = "mapNotFound";
        error["message"] = "Map not found";
        return {json::serialize(error), true};
    }

    std::string HandleBadRequest(void) {
        namespace json = boost::json;
        json::object error;
        error["code"] = "badRequest";
        error["message"] = "Bad request";
        return json::serialize(error);
    }
};

}  // namespace http_handler

