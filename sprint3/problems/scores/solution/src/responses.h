#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <variant>

#include "http_server.h"

namespace response {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;

    constexpr static std::string_view IMAGE_PNG = "image/png"sv;
    constexpr static std::string_view APP_OCT_STREAM = "application/octet-stream"sv;
};

template <class Request>
http::response<http::string_body> MakeTextResponse(
    http::status status, std::string body, const Request& req, std::string_view content_type) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

using ResponseVariant = std::variant<http::response<http::string_body>, http::response<http::file_body>>;

template <typename Request>
ResponseVariant MakeJSON(
    http::status status, json::value&& body, const Request& req, std::string cache_control = "no-cache"s) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, ContentType::APP_JSON);
    res.set(http::field::cache_control, cache_control);
    res.body() = json::serialize(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());

    return res;
}
/*
template <typename Request>
ResponseVariant MakeJSON(
    http::status status, std::string body, const Request& req, std::string cache_control = "no-cache"s) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, ContentType::APP_JSON);
    res.set(http::field::cache_control, cache_control);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());

    return res;
}
*/
template <typename Request>
ResponseVariant MakeError(http::status status, std::string_view code, std::string_view message,
    const Request& req, std::string cache_control = "no-cache"s) {
    return MakeJSON(status, json::object{{"code", code}, {"message", message}}, req, cache_control);
}

template <typename Request>
ResponseVariant MakeMethodNotAllowedError(
    std::string_view message, std::string_view allow, const Request& req) {
    // 1. Generate the base JSON response
    auto variant_res = MakeJSON(
        http::status::method_not_allowed, json::object{{"code", "invalidMethod"}, {"message", message}}, req);
    // 2. If we need an Allow header, we must extract the response from the variant
    // Since MakeJSON always returns string_body for errors:
    auto& res = std::get<http::response<http::string_body>>(variant_res);
    res.set(http::field::allow, allow);

    return variant_res;
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
ResponseVariant MakeFile(
    http::status status, http::file_body::value_type&& file, std::string_view mime_type, const Request& req) {
    http::response<http::file_body> res{status, req.version()};
    res.set(http::field::content_type, mime_type);
    res.body() = std::move(file);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

}  // namespace response
