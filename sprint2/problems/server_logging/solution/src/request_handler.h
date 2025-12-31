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

#include "http_server.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
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

// Возвращает true, если каталог p содержится внутри base_path.
inline bool IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;

    constexpr static std::string_view IMAGE_PNG = "image/png"sv;
    constexpr static std::string_view APP_OCT_STREAM = "application/octet-stream"sv;
};

std::string UrlDecode(std::string_view text);
std::string_view DefineMIMEType(const std::filesystem::path& path);

template <class Request>
http::response<http::string_body> MakeTextResponse(http::status status, std::string body, const Request& req,
                                                   std::string_view content_type) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

template <typename Send>
struct ResponseSender {
    Send& send;
    http::verb method;

    // Overload for any response type (string_body or file_body)
    template <typename T>
    void operator()(http::response<T>&& res) const {
        if (method == http::verb::head) {
            res.body() = {};  // Remove body for HEAD
            res.prepare_payload();
        }
        send(std::move(res));
    }
};

using ResponseVariant = std::variant<http::response<http::string_body>, http::response<http::file_body>>;

namespace responses {

template <typename Request>
ResponseVariant MakeJSON(http::status status, json::value&& body, const Request& req) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, ContentType::APP_JSON);
    res.body() = json::serialize(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}
template <typename Request>
ResponseVariant MakeError(http::status status, std::string_view code, std::string_view message, const Request& req) {
    return MakeJSON(status, json::object{{"code", code}, {"message", message}}, req);
}
template <typename Request>
ResponseVariant MakeTextError(http::status status, std::string_view message, const Request& req) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, ContentType::TEXT_PLAIN);
    res.body() = message;
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}
template <typename Request>
ResponseVariant MakeFile(http::status status, http::file_body::value_type&& file, std::string_view mime_type,
                         const Request& req) {
    http::response<http::file_body> res{status, req.version()};
    res.set(http::field::content_type, mime_type);
    res.body() = std::move(file);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

}  // namespace responses

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::filesystem::path path_to_static)
        : game_{game}, path_to_static_{std::move(path_to_static)} {}
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // This is a template, so it stays in the header
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        ResponseSender<Send> visitor{send, req.method()};

        if (req.target().starts_with("/api/")) {
            return std::visit(visitor, HandleAPI(req));
        }

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return std::visit(visitor, responses::MakeError(http::status::method_not_allowed, "invalidMethod",
                                                            "Only GET/HEAD allowed", req));
        }

        return std::visit(visitor, HandleStatic(req));
    }

private:
    model::Game& game_;
    fs::path path_to_static_;

    template <typename Request>
    ResponseVariant HandleStatic(const Request& req) {
        const auto& target = req.target();

        // prepare path
        // Decode the URL target
        std::string decoded_target = UrlDecode(target);
        fs::path rel_path{decoded_target.substr(1)};
        fs::path full_path = fs::weakly_canonical(path_to_static_ / rel_path);

        // check if this path is safe
        if (!IsSubPath(full_path, path_to_static_)) {
            return responses::MakeTextError(http::status::bad_request, "Request is badly formed", req);
        }

        // Default to index.html
        if (fs::is_directory(full_path)) {
            full_path /= "index.html";
        }

        if (!fs::exists(full_path)) {
            return responses::MakeTextError(http::status::not_found, "File not found", req);
        }

        // Use Boost.Beast to open the file
        http::file_body::value_type file;
        beast::error_code ec;
        file.open(full_path.string().c_str(), beast::file_mode::read, ec);
        if (ec) {
            return responses::MakeTextError(http::status::not_found, "File not found", req);
        }

        return responses::MakeFile(http::status::ok, std::move(file), DefineMIMEType(full_path), req);
    }

    template <typename Request>
    ResponseVariant HandleAPI(const Request& req) {
        const auto& target = req.target();

        if (target == "/api/v1/maps") {
            return responses::MakeJSON(http::status::ok, std::move(HandleMaps()), req);
        }

        if (target.starts_with("/api/v1/maps/")) {
            auto parts = SplitTarget(target);
            if (parts.size() == 4) {
                auto [response_body, error] = HandleMapId(parts[3]);
                if (error) {
                    return responses::MakeError(http::status::not_found, "mapNotFound", "map Not Found", req);
                }
                return responses::MakeJSON(http::status::ok, std::move(response_body), req);
            }
        }

        return responses::MakeError(http::status::bad_request, "badRequest", "Invalid API path", req);
    }

    json::value HandleMaps();
    std::pair<json::value, bool> HandleMapId(std::string_view name_map);

    boost::json::object SerializeMap(const model::Map& map);
    boost::json::object SerializeRoad(const model::Road& road);
    boost::json::object SerializeBuilding(const model::Building& b);
    boost::json::object SerializeOffice(const model::Office& o);
};

}  // namespace http_handler
