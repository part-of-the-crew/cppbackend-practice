#pragma once
#include <string_view>

namespace logger {

// Initialize the logging system (Sink, Formatter, etc.)
void InitBoostLogFilter();

// High-level logging functions using only standard types
void LogServerRequest(std::string_view ip, std::string_view uri, std::string_view method);
void LogServerResponse(long ms, int code, std::string_view content_type);
void LogServerLaunch(std::string_view address, unsigned short port);
void LogServerStop(int code, std::string_view what);
void LogNetError(int code, std::string_view what, std::string_view where);
}  // namespace logger