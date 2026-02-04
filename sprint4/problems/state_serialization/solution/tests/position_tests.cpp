#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "app.h"
#include "geom.h"
#include "model.h"
using namespace std::literals;

using namespace model;
using namespace geom;
using namespace Catch::Matchers;
using namespace app;

TEST_CASE("CalculateNewPosition with Tagged IDs", "[app][physics]") {
    // FIX: Must use Map::Id{std::string} because Tagged constructor is explicit
    Map map{Map::Id{"test_map"s}, "test_map"};

    const double HALF_WIDTH = Road::HALF_WIDTH;

    SECTION("Horizontal movement and clamping") {
        // Road from (0,0) to (10,0)
        map.AddRoad(Road{Road::HORIZONTAL, {0, 0}, 10});

        SECTION("Standard movement") {
            Position start{2.0, 0.0};
            Speed speed{1.0, 0.0};
            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            CHECK_THAT(result.x, WithinRel(3.0, 1e-9));
            CHECK_THAT(result.y, WithinRel(0.0, 1e-9));
        }

        SECTION("Clamping at start boundary") {
            Position start{0.0, 0.0};
            Speed speed{-10.0, 0.0};  // Moving left out of bounds
            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // Should be clamped to -HALF_WIDTH (0.0 - HALF_WIDTH)
            CHECK_THAT(result.x, WithinAbs(-HALF_WIDTH, 1e-9));
        }

        SECTION("Large dt clamps correctly") {
            Position start{5.0, 0.0};
            Speed speed{100.0, 0.0};  // very fast
            double dt = 1.0;

            auto result = CalculateNewPosition(&map, start, speed, dt);

            // Clamped to r_max + HALF_WIDTH = 10 + HALF_WIDTH
            CHECK_THAT(result.x, WithinAbs(10.0 + HALF_WIDTH, 1e-9));
        }

        SECTION("Outside road width: movement not constrained (current behavior)") {
            // start just outside the road corridor in y
            Position start{5.0, HALF_WIDTH + 0.01};
            Speed speed{1.0, 0.0};

            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // No valid bounds found for this position -> free movement
            CHECK_THAT(result.x, WithinRel(6.0, 1e-9));
            CHECK_THAT(result.y, WithinRel(start.y, 1e-9));
        }

        SECTION("Movement exactly on road boundary") {
            Position start{0.0 - HALF_WIDTH, 0.0};
            Speed speed{1.0, 0.0};

            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // start + 1.0
            CHECK_THAT(result.x, WithinRel(1.0 - HALF_WIDTH, 1e-9));
        }

        SECTION("Negative dt reverses movement direction") {
            Position start{5.0, 0.0};
            Speed speed{1.0, 0.0};

            auto result = CalculateNewPosition(&map, start, speed, -1.0);

            CHECK_THAT(result.x, WithinRel(4.0, 1e-9));
        }
    }

    SECTION("Vertical movement and junctions") {
        // Creating a T-junction
        map.AddRoad(Road{Road::VERTICAL, {5, -5}, 5});    // Vertical road through x=5
        map.AddRoad(Road{Road::HORIZONTAL, {0, 0}, 10});  // Horizontal road through y=0

        SECTION("Moving through junction") {
            Position start{5.0, -1.0};
            Speed speed{0.0, 2.0};  // Moving North to South
            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // Should move to y=1.0 because we are within the vertical road
            CHECK_THAT(result.y, WithinRel(1.0, 1e-9));
            CHECK_THAT(result.x, WithinRel(5.0, 1e-9));
        }

        SECTION("Clamping at dead end") {
            Position start{5.0, 5.0};
            Speed speed{0.0, 5.0};  // Moving further down past the end of vertical road
            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // Should be clamped to 5.0 + HALF_WIDTH
            CHECK_THAT(result.y, WithinAbs(5.0 + HALF_WIDTH, 1e-9));
        }

        SECTION("Horizontal movement constrained by crossing vertical road") {
            // Vertical road crossing at x = 5; start exactly at crossing
            Position start{5.0, 0.0};
            Speed speed{10.0, 0.0};

            auto result = CalculateNewPosition(&map, start, speed, 1.0);

            // Should be constrained to the crossing's x +/- HALF_WIDTH
            CHECK_THAT(result.x, WithinAbs(5.0 + HALF_WIDTH, 1e-9));
        }
    }

    SECTION("Edge Case: Zero Speed") {
        map.AddRoad(Road{Road::HORIZONTAL, {0, 0}, 10});
        Position start{5.0, 0.0};
        Speed speed{0.0, 0.0};

        auto result = CalculateNewPosition(&map, start, speed, 10.0);
        CHECK(result.x == start.x);
        CHECK(result.y == start.y);
    }

    SECTION("No roads: free movement without clamping") {
        // map has no roads in this section (fresh map per SECTION)
        Position start{0.0, 0.0};
        Speed speed{10.0, 0.0};

        auto result = CalculateNewPosition(&map, start, speed, 1.0);

        CHECK_THAT(result.x, WithinRel(10.0, 1e-9));
        CHECK_THAT(result.y, WithinRel(0.0, 1e-9));
    }
}