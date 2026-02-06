#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace std::literals;

SCENARIO("Extra data storing lifecycle", "[ExtraData]") {
    GIVEN("An empty ExtraData container") {
        int extra{};
        REQUIRE(extra == 0);
    }
}