#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

/*
Пустая входная строка
Входная строка без служебных символов
Входная строка со служебными символами
Входная строка с пробелами
Входная строка с символами с кодами меньше 31 и большими или равными 128
*/
TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
    EXPECT_EQ(UrlEncode(" "sv), "\%20"s);
    EXPECT_EQ(UrlEncode("a A"sv), "a\%20A"s);
    EXPECT_EQ(UrlEncode("  "sv), "\%20\%20"s);
    {
        std::string input = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
        EXPECT_EQ(UrlEncode(input), input);
    }
    EXPECT_EQ(UrlEncode("a*A"sv), "a\%2AA"s);
    {
        std::string input = "Привет";
        std::string expected = "\%D0\%9F\%D1\%80\%D0\%B8\%D0\%B2\%D0\%B5\%D1\%82";

        EXPECT_EQ(UrlEncode(input), expected);
    }
    EXPECT_EQ(UrlEncode("\n"), "%0A");
}
/* Напишите остальные тесты самостоятельно */
