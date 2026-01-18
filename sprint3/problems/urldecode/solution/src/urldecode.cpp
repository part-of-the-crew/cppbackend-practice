#include "urldecode.h"

#include <charconv>
#include <stdexcept>

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
                    throw std::invalid_argument{"Invalid hex (e.g., \%G1 or \%1G)"};
                    // res += '%';
                }
            } else {
                // Not enough chars for a hex escape (e.g., "test%" or "test%1")
                throw std::invalid_argument{"Not enough chars for a hex escape"};
                // res += '%';
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