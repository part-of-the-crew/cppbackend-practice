#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>         // для logging::core
#include <boost/log/expressions.hpp>  // для выражения, задающего фильтр
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/trivial.hpp>  // для BOOST_LOG_TRIVIAL
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

using namespace std::literals;

namespace logger {
namespace logging = boost::log;
namespace json = boost::json;
namespace sinks = boost::log::sinks;

BOOST_LOG_ATTRIBUTE_KEYWORD(json_data, "JsonData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

inline void MyFormatter1(logging::record_view const& rec, logging::formatting_ostream& strm) {
    auto ts = rec[timestamp];
    auto data = rec[json_data];
    auto message = rec[logging::expressions::smessage];

    if (!ts || !data || !message)
        throw std::runtime_error("!ts || !data || !message");

    boost::json::value jv = {
        {"timestamp", to_iso_extended_string(*ts)}, {"data", data->as_object()}, {"message", *message}};

    strm << json::serialize(jv);
}
// In my_logger.h

inline void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    // 1. Safe extraction
    auto ts = rec[timestamp];
    auto data = rec[json_data];
    auto message = rec[logging::expressions::smessage];

    // 2. DO NOT THROW. Handle missing data gracefully.
    boost::json::object jv;

    jv["timestamp"] = ts ? to_iso_extended_string(*ts) : "null";
    jv["message"] = message ? *message : "";

    if (data) {
        jv["data"] = data->as_object();
    } else {
        jv["data"] = nullptr;
    }

    strm << json::serialize(jv);
}

inline void InitBoostLogFilter() {
    // 1. Create the backend first
    auto backend = boost::make_shared<sinks::text_ostream_backend>();

    // 2. Add the stream (std::cout)
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter{}));

    // 3. Set auto_flush on the BACKEND
    // This tells Boost to call flush() on the stream after every log record
    backend->auto_flush(true);

    // 4. Wrap the backend in the synchronous sink (frontend)
    auto sink = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>(backend);

    sink->set_formatter(&MyFormatter1);

    logging::core::get()->add_sink(sink);
    logging::add_common_attributes();
}

inline void InitBoostLogFilter2sinks() {
    auto backend = boost::make_shared<sinks::text_ostream_backend>();

    // 1. Add std::cout
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter{}));

    // 2. Add the file stream
    // Use std::make_shared for the file stream to manage its lifetime
    auto file_stream = boost::make_shared<std::ofstream>("server.log", std::ios::app);
    backend->add_stream(file_stream);

    // Auto-flush ensures logs appear immediately (important for Docker crashes!)
    backend->auto_flush(true);

    auto sink = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>(backend);
    sink->set_formatter(&MyFormatter);

    logging::core::get()->add_sink(sink);
    logging::add_common_attributes();
}
inline void LogServerRequest(std::string ip, std::string URI, std::string_view method) {
    json::value data = {
        {"ip", std::move(ip)},
        {"URI", std::move(URI)},
        {"method", std::string(method)},
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(json_data, data) << "request received"sv;
}
inline void LogServerResponse(int64_t ms, int code, std::string_view content_type) {
    json::value data = {
        {"response_time", ms},
        {"code", code},
        {"content_type", content_type.empty() ? "null"s : std::string(content_type)},
    };

    BOOST_LOG_TRIVIAL(info) << logging::add_value(json_data, data) << "response sent"sv;
}

inline void LogServerLaunch(const boost::asio::ip::tcp::endpoint& endpoint) {
    json::value data = {
        {"port", endpoint.port()},
        {"address", endpoint.address().to_string()},
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(json_data, data) << "server started"sv;
}

inline void LogServerStop(int code, std::string what) {
    json::value data = {
        {"code", code},
        {"exception", std::move(what)},
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(json_data, data) << "server exited"sv;
}
inline void LogNetError(int code, std::string what, std::string_view where) {
    json::value data = {
        {"code", std::move(code)},
        {"text", std::move(what)},
        {"where", std::string(where)},
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(json_data, data) << "error"sv;
}

namespace net = boost::asio;
using tcp = net::ip::tcp;

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
    // explicit LoggingRequestHandler(Handler& handler) : handler_(handler) {}
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
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_ts);

            // Extract response metadata
            int status_code = response.result_int();
            std::string content_type = "unknown";
            auto it = response.find(boost::beast::http::field::content_type);
            if (it != response.end()) {
                content_type = std::string(it->value());
            }

            // Log the response
            logger::LogServerResponse(duration.count(), status_code, content_type);

            // Finally, call the original send
            send(std::forward<decltype(response)>(response));
        };

        // 5. Pass the request to the actual handler
        // handler_(ep, std::forward<Request>(req), std::move(logged_send));
        if constexpr (is_shared_ptr_v<Handler>) {
            (*handler_)(ep, std::forward<Request>(req), std::move(logged_send));
        } else {
            handler_(ep, std::forward<Request>(req), std::move(logged_send));
        }
    }

private:
    Handler handler_;
    // No more start_ts_ here!
};

}  // namespace logger
