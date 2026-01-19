#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
    {
        std::string input = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
        CHECK(HtmlDecode(input) == input);
    }

    CHECK(HtmlDecode("&amp"sv) == "&"s);
    CHECK(HtmlDecode("&amp;"sv) == "&"s);
    CHECK(HtmlDecode("&AMP"sv) == "&"s);
    CHECK(HtmlDecode("&AMP;"sv) == "&"s);

    CHECK(HtmlDecode("&quot"sv) == "\""s);
    CHECK(HtmlDecode("&quot;"sv) == "\""s);
    CHECK(HtmlDecode("&QUOT"sv) == "\""s);
    CHECK(HtmlDecode("&QUOT;"sv) == "\""s);

    CHECK(HtmlDecode("&"sv) == "&"s);
    CHECK(HtmlDecode("&&"sv) == "&&"s);
    CHECK(HtmlDecode("&&&"sv) == "&&&"s);

    CHECK(HtmlDecode("&amplt"sv) == "&lt"s);

    CHECK(HtmlDecode("&apos"sv) == "\'"s);
    CHECK(HtmlDecode("&apos;"sv) == "\'"s);
    CHECK(HtmlDecode("&APOS"sv) == "\'"s);
    CHECK(HtmlDecode("&APOS;"sv) == "\'"s);

    CHECK(HtmlDecode("&lt"sv) == "<"s);
    CHECK(HtmlDecode("&lt;"sv) == "<"s);
    CHECK(HtmlDecode("&LT"sv) == "<"s);
    CHECK(HtmlDecode("&LT;"sv) == "<"s);

    CHECK(HtmlDecode("&gt"sv) == ">"s);
    CHECK(HtmlDecode("&gt;"sv) == ">"s);
    CHECK(HtmlDecode("&GT"sv) == ">"s);
    CHECK(HtmlDecode("&GT;"sv) == ">"s);
}

/*
Символ < кодируется последовательностью &lt.
Символ > кодируется последовательностью &gt.
Символ & кодируется последовательностью &amp.
Символ ' кодируется как &apos.
Символ " кодируется как &quot.
Строка без HTML-мнемоник.
Строка с HTML-мнемониками.
Пустая строка.
Строка с HTML-мнемониками, записанными в верхнем регистре.
Строка с HTML-мнемониками, записанными в смешанном регистре.
Строка с HTML-мнемониками в начале, конце и середине.
Строка с недописанными HTML-мнемониками.
Строка с HTML-мнемониками, заканчивающимися и не заканчивающимися на точку с запятой.
*/