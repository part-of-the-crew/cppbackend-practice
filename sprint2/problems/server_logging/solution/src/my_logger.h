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
namespace keywords = logging::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(json_data, "JsonData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

inline void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    auto ts = rec[timestamp];
    auto data = rec[json_data];
    auto message = rec[logging::expressions::smessage];

    if (!ts || !data || !message)
        throw std::runtime_error("!ts || !data || !message");

    boost::json::value jv = {
        {"timestamp", to_iso_extended_string(*ts)}, {"data", data->as_object()}, {"message", *message}};

    strm << json::serialize(jv);
}

inline void InitBoostLogFilter() {
    auto sink = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>();

    // Write to std::cout
    sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter{}));

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
inline void LogServerResponse(std::string logging_handler, int code, std::string_view content_type) {
    json::value data = {
        {"response_time", std::move(logging_handler)},
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

}  // namespace logger
