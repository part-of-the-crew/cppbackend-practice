#include "request_handler.h"

#include <charconv>  // For std::from_chars

namespace http_handler {

// Helper remains inline because it's simple and used by the template operator()
std::vector<std::string_view> SplitTarget(std::string_view target) {
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
bool IsSubPath(fs::path path, fs::path base) {
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

std::string UrlDecode(std::string_view text) {
    std::string res;
    // Reserve memory to avoid reallocations, assuming decoded string <= source
    res.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%') {
            // Check if there are at least two characters following '%'
            if (i + 2 < text.size()) {
                int value;
                // std::from_chars parses the range [first, last)
                auto [ptr, ec] = std::from_chars(text.data() + i + 1, text.data() + i + 3, value, 16);

                // Ensure successful parse AND that we consumed exactly 2 chars
                // (ptr should point to start + 2 if 2 hex digits were read)
                if (ec == std::errc{} && ptr == (text.data() + i + 3)) {
                    res += static_cast<char>(value);
                    i += 2;
                } else {
                    // Invalid hex (e.g., %G1 or %1G), treat '%' as literal
                    res += '%';
                }
            } else {
                // Not enough chars for a hex escape (e.g., "test%" or "test%1")
                res += '%';
            }
        } else if (text[i] == '+') {
            // URL standard: '+' decodes to a space
            res += ' ';
        } else {
            res += text[i];
        }
    }
    return res;
}

std::string_view DefineMIMEType(const std::filesystem::path& path) {
    using namespace std::literals;

    // Get extension and convert to lowercase for comparison
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".htm"sv || ext == ".html"sv)
        return "text/html"sv;
    if (ext == ".css"sv)
        return "text/css"sv;
    if (ext == ".txt"sv)
        return "text/plain"sv;
    if (ext == ".js"sv)
        return "text/javascript"sv;
    if (ext == ".json"sv)
        return "application/json"sv;
    if (ext == ".xml"sv)
        return "application/xml"sv;
    if (ext == ".png"sv)
        return "image/png"sv;
    if (ext == ".jpg"sv || ext == ".jpe"sv || ext == ".jpeg"sv)
        return "image/jpeg"sv;
    if (ext == ".gif"sv)
        return "image/gif"sv;
    if (ext == ".bmp"sv)
        return "image/bmp"sv;
    if (ext == ".ico"sv)
        return "image/vnd.microsoft.icon"sv;
    if (ext == ".tiff"sv || ext == ".tif"sv)
        return "image/tiff"sv;
    if (ext == ".svg"sv || ext == ".svgz"sv)
        return "image/svg+xml"sv;
    if (ext == ".mp3"sv)
        return "audio/mpeg"sv;

    // Default for unknown files
    return "application/octet-stream"sv;
}

}  // namespace http_handler
