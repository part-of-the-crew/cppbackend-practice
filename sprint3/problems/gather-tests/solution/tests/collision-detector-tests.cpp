#define _USE_MATH_DEFINES
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sstream>

#include "../src/collision_detector.h"

namespace Catch {

template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << ","
            << value.time << ")";

        return tmp.str();
    }
};

}  // namespace Catch

// std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

class TestProvider : public collision_detector::ItemGathererProvider {
public:
    TestProvider() = default;
    void AddItem(geom::Point2D pos, double width) {
        items_.push_back({pos, width});
    }

    void AddGatherer(geom::Point2D start, geom::Point2D end, double width) {
        gatherers_.push_back({start, end, width});
    }

    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

/*
struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};
*/
TEST_CASE("Gatherer moves directly through item", "[collision]") {
    TestProvider provider;
    // Item at (5, 0) with width 0.0 (point)
    provider.AddItem({5, 0}, 0.0);

    // Gatherer moves from (0, 0) to (10, 0)
    provider.AddGatherer({0, 0}, {10, 0}, 0.0);

    auto events = collision_detector::FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    // Time should be 0.5 because (5,0) is exactly halfway between (0,0) and (10,0)
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-10));
}
TEST_CASE("Gatherer misses item", "[collision]") {
    TestProvider provider;

    // Item at (5, 5)
    provider.AddItem({5, 5}, 1.0);

    // Gatherer moves along X axis (0,0) -> (10,0) with small width
    provider.AddGatherer({0, 0}, {10, 0}, 1.0);

    // Distance is 5. Combined radius is 1.0 + 1.0 = 2.0.
    // 5 > 2, so no collision.
    auto events = collision_detector::FindGatherEvents(provider);

    CHECK(events.empty());
}

TEST_CASE("Gatherer collects multiple items", "[collision]") {
    TestProvider provider;

    provider.AddItem({2, 0}, 0.5);  // 1st item (closer)
    provider.AddItem({8, 0}, 0.5);  // 2nd item (further)

    provider.AddGatherer({0, 0}, {10, 0}, 0.5);

    auto events = collision_detector::FindGatherEvents(provider);

    REQUIRE(events.size() == 2);

    // Check first event (Item 0)
    CHECK(events[0].item_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.2, 1e-10));

    // Check second event (Item 1)
    CHECK(events[1].item_id == 1);
    CHECK_THAT(events[1].time, Catch::Matchers::WithinAbs(0.8, 1e-10));
}