#include "htmldecode.h"

#include <cctype>
#include <string>
#include <string_view>

using namespace std::literals;

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] != '&') {
            result.push_back(str[i]);
            continue;
        }

        auto check_and_append = [&](std::string_view name, char entity_char) -> bool {
            // 1. Check if we have enough characters for the name
            if (i + 1 + name.size() > str.size()) {
                return false;
            }

            // 2. Case-insensitive comparison of the name
            for (size_t k = 0; k < name.size(); ++k) {
                if (std::tolower(static_cast<unsigned char>(str[i + 1 + k])) !=
                    std::tolower(static_cast<unsigned char>(name[k]))) {
                    return false;
                }
            }

            // 3. Check for optional semicolon
            bool has_semicolon = false;
            if (i + 1 + name.size() < str.size() && str[i + 1 + name.size()] == ';') {
                has_semicolon = true;
            }

            // 4. Apply decode
            result.push_back(entity_char);

            // 5. Advance index
            i += name.size() + (has_semicolon ? 1 : 0);

            return true;
        };

        // Check for known entities
        // Order technically doesn't matter here as no entity name is a prefix of another
        if (check_and_append("lt"sv, '<'))
            continue;
        if (check_and_append("gt"sv, '>'))
            continue;
        if (check_and_append("amp"sv, '&'))
            continue;
        if (check_and_append("apos"sv, '\''))
            continue;
        if (check_and_append("quot"sv, '"'))
            continue;

        // If no match, treat '&' as literal
        result.push_back('&');
    }

    return result;
}