#include <boost/test/tools/old/interface.hpp>
#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

/*
+Пустая строка.
+Строка без %-последовательностей.
Строка с валидными %-последовательностями, записанными в разном регистре.
Строка с невалидными %-последовательностями.
Строка с неполными %-последовательностями.
+Строка с символом +.
*/

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);
    {
        std::string str = "1234567890Aa"s;
        BOOST_TEST(UrlDecode(str) == str);
    }
    BOOST_TEST(UrlDecode("12345+67890"sv) == "12345 67890");
    BOOST_TEST(UrlDecode("0\%4B"sv) == "0K");
    {
        std::string input = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
        BOOST_TEST(UrlDecode(input) == input);
    }
    {
        std::string input = "\%D0\%9F\%D1\%80\%D0\%B8\%D0\%B2\%D0\%B5\%D1\%82";
        std::string expected = "Привет";
        BOOST_TEST(UrlDecode(input) == expected);
    }
    BOOST_CHECK_THROW(UrlDecode("12\%A"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("12\%G0"sv), std::invalid_argument);
}