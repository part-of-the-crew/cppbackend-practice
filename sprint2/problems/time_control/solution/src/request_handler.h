#pragma once
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <algorithm>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "api_handler.h"
#include "http_server.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

bool IsSubPath(fs::path path, fs::path base);
std::string UrlDecode(std::string_view text);
std::string_view DefineMIMEType(const std::filesystem::path& path);

template <typename Send>
struct ResponseSender {
    Send& send;
    http::verb method;

    template <typename T>
    void operator()(http::response<T>&& res) const {
        if (method == http::verb::head) {
            res.body() = {};
            res.prepare_payload();
        }
        send(std::move(res));
    }
};

class RequestHandler {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    RequestHandler(fs::path path_to_static, Strand& api_strand, app::Application& application)
        : path_to_static_{std::move(path_to_static)}, handleAPI_{application}, api_strand_{api_strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        [[maybe_unused]] tcp::endpoint ep, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        
        // 1. Check for API requests FIRST. 
        // We move 'req' and 'send' into the lambda, so we cannot use them afterwards.
        if (req.target().starts_with("/api/")) {
            auto task = [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                // Re-create the visitor INSIDE the lambda where 'send' is valid
                ResponseSender<Send> visitor{send, req.method()};
                std::visit(visitor, handleAPI_(req));
            };
            return net::dispatch(api_strand_, std::move(task));
        }

        // 2. If not API, 'send' was NOT moved, so we can use it here.
        ResponseSender<Send> visitor{send, req.method()};

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return std::visit(visitor,
                response::MakeError(http::status::method_not_allowed, "invalidMethod", "Only GET/HEAD allowed", req));
        }

        return std::visit(visitor, HandleStatic(req));
    }

private:
    fs::path path_to_static_;
    Strand& api_strand_;
    api_handler::HandleAPI handleAPI_;

    template <typename Request>
    response::ResponseVariant HandleStatic(const Request& req) {
        std::string decoded_target = UrlDecode(req.target());
        fs::path rel_path{decoded_target.substr(1)};
        fs::path full_path = fs::weakly_canonical(path_to_static_ / rel_path);

        if (!IsSubPath(full_path, path_to_static_)) {
            return response::MakeTextError(http::status::bad_request, "Request is badly formed", req);
        }

        if (fs::is_directory(full_path)) {
            full_path /= "index.html";
        }
        if (!fs::exists(full_path)) {
            return response::MakeTextError(http::status::not_found, "File not found"s, req);
        }

        http::file_body::value_type file;
        beast::error_code ec;
        file.open(full_path.string().c_str(), beast::file_mode::read, ec);
        if (ec) {
            return response::MakeTextError(http::status::forbidden, "Couldn't open file"s, req);
        }
        return response::MakeFile(http::status::ok, std::move(file), DefineMIMEType(full_path), req);
    }
};

}  // namespace http_handler
