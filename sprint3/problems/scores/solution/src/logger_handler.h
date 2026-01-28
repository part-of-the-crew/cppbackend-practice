#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <chrono>

#include "my_logger.h"

namespace logger_handler {

namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

// Helper to check if it's a shared_ptr
template <typename T>
struct is_shared_ptr : std::false_type {};
template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};
template <typename T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <class Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler handler) : handler_(std::move(handler)) {}
    template <class Request, class Send>
    void operator()(tcp::endpoint ep, Request&& req, Send&& send) {
        // 1. Record start time locally (on the stack, not in a member variable)
        auto start_ts = std::chrono::system_clock::now();

        // 2. Extract request info before moving req
        std::string ip = ep.address().to_string();
        std::string uri{req.target()};
        std::string method{req.method_string()};

        // 3. Log the request immediately
        logger::LogServerRequest(ip, uri, method);

        // 4. Create the completion wrapper
        // Capture start_ts and ip by VALUE so each request has its own copy
        auto logged_send = [start_ts, ip, send = std::forward<Send>(send)](auto&& response) mutable {
            // Calculate duration
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - start_ts);

            // Extract response metadata
            int status_code = response.result_int();
            std::string content_type = "unknown";
            auto it = response.find(http::field::content_type);
            if (it != response.end()) {
                content_type = std::string(it->value());
            }

            // Log the response
            logger::LogServerResponse(duration.count(), status_code, content_type);

            // Finally, call the original send
            send(std::forward<decltype(response)>(response));
        };

        // 5. Pass the request to the actual handler
        if constexpr (is_shared_ptr_v<Handler>) {
            (*handler_)(ep, std::forward<Request>(req), std::move(logged_send));
        } else {
            handler_(ep, std::forward<Request>(req), std::move(logged_send));
        }
    }

private:
    Handler handler_;
};

}  // namespace logger_handler