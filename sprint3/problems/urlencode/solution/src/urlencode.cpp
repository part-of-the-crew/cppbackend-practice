#include "urlencode.h"

#include <cctype>
#include <iomanip>
#include <string>
#include <string_view>

std::string UrlEncode(std::string_view str) {
    std::string encoded;
    // Reserve minimal likely size to avoid early reallocations
    encoded.reserve(str.size());

    for (char c : str) {
        // RFC 3986 defines unreserved characters as:
        // ALPHA, DIGIT, "-", ".", "_", "~"
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            // Encode all other characters as %HH
            encoded += '%';

            // Convert to Hex
            static const char hex_digits[] = "0123456789ABCDEF";
            unsigned char uc = static_cast<unsigned char>(c);
            encoded += hex_digits[(uc >> 4) & 0x0F];  // High nibble
            encoded += hex_digits[uc & 0x0F];         // Low nibble
        }
    }
    return encoded;
}
